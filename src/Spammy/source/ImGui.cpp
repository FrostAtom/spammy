#include "ImGui.h"
#include <cstring>

#include "Resources/ImFont_ArchivoBold.inl"
#include "Resources/ImFont_ArchivoExtraBold.inl"
#include "Resources/ImFont_ArchivoLight.inl"
#include "Resources/ImFont_ArchivoMedium.inl"
#include "Resources/ImFont_ArchivoSemiBold.inl"

using namespace ImGui;

ImFont* ImGui::UiFonts::Light = nullptr;
ImFont* ImGui::UiFonts::Medium = nullptr;
ImFont* ImGui::UiFonts::SemiBold = nullptr;
ImFont* ImGui::UiFonts::Bold = nullptr;
ImFont* ImGui::UiFonts::Heavy = nullptr;

static ImGuiStorage s_animStorage;
// VtxBuffer size of each open UiBeginPopup's draw list right after its shadow, for fading the content in
static ImVector<int> s_popupVtxStart;

namespace {
// animation values live in s_animStorage under the widget's id hashed with one of these
enum AnimSlot {
    AnimSlot_Hover = 1,
    AnimSlot_Press,
    AnimSlot_State,
    AnimSlot_Popup = 0x506F5055, // a popup shares its id with a same-named button, keep clear of the button's slots
};

ImGuiID SubId(ImGuiID id, AnimSlot slot, int index = 0)
{
    const int key[2] = {slot, index};
    return ImHashData(key, sizeof(key), id);
}

struct HitBox {
    bool clicked;
    ImGuiID id;

    float Anim(AnimSlot slot, bool target, float speed) const
    {
        return UiAnim(SubId(id, slot), target ? 1.f : 0.f, speed);
    }
    // these query the last item, so call them before submitting another one
    float HoverAnim(float speed = 18.f) const { return Anim(AnimSlot_Hover, IsItemHovered(), speed); }
    float PressAnim() const { return Anim(AnimSlot_Press, IsItemActive(), 28.f); }
};

HitBox PlaceInvisibleButton(const char* id, const ImVec2& pos, const ImVec2& size)
{
    SetCursorScreenPos(pos);
    const bool clicked = InvisibleButton(id, size);
    return {clicked, GetItemID()};
}

void ScaleVtxAlpha(ImDrawList* dl, int start, float alpha)
{
    for (int i = start; i < dl->VtxBuffer.Size; i++)
        dl->VtxBuffer[i].col = UiWithAlpha(dl->VtxBuffer[i].col, alpha);
}

// fades everything drawn into dl while it lives
struct AlphaScope {
    ImDrawList* dl;
    int start;
    float alpha;

    AlphaScope(ImDrawList* drawList, float fade) : dl(drawList), start(drawList->VtxBuffer.Size), alpha(fade) {}
    ~AlphaScope()
    {
        if (alpha < 1.f) ScaleVtxAlpha(dl, start, alpha);
    }
};

// ascii-uppercased copy for silkscreen, tapes and the like
struct Upper {
    char buf[256];

    explicit Upper(const char* text)
    {
        size_t i = 0;
        for (; text[i] && i < sizeof(buf) - 1; i++) {
            const char c = text[i];
            buf[i] = c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c;
        }
        buf[i] = 0;
    }
    operator const char*() const { return buf; }
};

// calls fn(glyphBegin, glyphEnd, penX) for each glyph; returns the pen position past the last one
template <class Fn>
float ForEachTrackedGlyph(ImFont* font, float size, const char* text, float tracking, float penX, Fn&& fn)
{
    ImFontBaked* baked = font->GetFontBaked(size);
    const char* end = text + strlen(text);
    for (const char* p = text; p < end;) {
        unsigned int c = 0;
        const int len = ImTextCharFromUtf8(&c, p, end);
        if (!len) break;
        if (const ImFontGlyph* glyph = baked->FindGlyph((ImWchar)c)) {
            fn(p, p + len, penX);
            penX += glyph->AdvanceX + tracking;
        }
        p += len;
    }
    return penX;
}

// text whose line box is centered on center; with Archivo's metrics that also centers the capitals
void AddTextCentered(ImDrawList* dl, ImFont* font, float size, const ImVec2& center, ImU32 col, const char* text,
                     float tracking = 0.f)
{
    const float width = UiTextWidth(font, size, text, tracking);
    AddTrackedText(dl, font, size, ImVec2(center.x - width * 0.5f, center.y - size * 0.5f), col, text, tracking);
}

void PathRoundRect(ImDrawList* dl, const ImVec2& a, const ImVec2& b, const float radii[4])
{
    const float limit = ImMax(0.f, ImMin(b.x - a.x, b.y - a.y) * 0.5f);
    auto corner = [&](float r, const ImVec2& tip, const ImVec2& center, float angle) {
        if (r < 0.5f)
            dl->PathLineTo(tip);
        else
            dl->PathArcTo(center, r, angle, angle + IM_PI * 0.5f);
    };
    const float r0 = ImClamp(radii[0], 0.f, limit), r1 = ImClamp(radii[1], 0.f, limit);
    const float r2 = ImClamp(radii[2], 0.f, limit), r3 = ImClamp(radii[3], 0.f, limit);
    corner(r0, a, ImVec2(a.x + r0, a.y + r0), IM_PI);
    corner(r1, ImVec2(b.x, a.y), ImVec2(b.x - r1, a.y + r1), IM_PI * 1.5f);
    corner(r2, b, ImVec2(b.x - r2, b.y - r2), 0.f);
    corner(r3, ImVec2(a.x, b.y), ImVec2(a.x + r3, b.y - r3), IM_PI * 0.5f);
}

struct CapColors {
    ImU32 face, lip, stroke;
};

CapColors CapPalette(UiCapKind kind)
{
    switch (kind) {
    case UiCapKind_Signal: return {UiCol::Signal, UiCol::SignalLip, 0};
    case UiCapKind_Tap: return {UiCol::Tap, UiCol::TapLip, 0};
    case UiCapKind_Same: return {UiCol::Same, UiCol::SameLip, 0};
    case UiCapKind_Locked: return {UiCol::CapLocked, UiCol::CapLip, 0};
    case UiCapKind_PausedAuto: return {UiCol::PausedAuto, UiCol::PausedAutoLip, 0};
    case UiCapKind_PausedTap: return {UiCol::PausedTap, UiCol::PausedTapLip, 0};
    default: return {UiCol::Cap, UiCol::CapLip, UiCol::CapStroke};
    }
}

// text color that reads on a cap of that kind
ImU32 CapInk(UiCapKind kind)
{
    switch (kind) {
    case UiCapKind_Signal:
    case UiCapKind_Tap: return UiCol::White;
    case UiCapKind_Same: return UiCol::SameInk;
    case UiCapKind_PausedAuto: return UiCol::PausedAutoInk;
    case UiCapKind_PausedTap: return UiCol::TapeInk;
    default: return UiCol::Ink;
    }
}

// lip first, then the face sunk by up to 3px; returns the face's top-left
ImVec2 DrawCap(ImDrawList* dl, const ImVec2& min, const ImVec2& size, const float radii[4], const CapColors& colors,
               float pressT, float lip = 4.f)
{
    const ImVec2 face = min + ImVec2(0.f, ImMin(3.f, lip - 1.f) * pressT);
    AddRoundRectFilled(dl, min + ImVec2(0.f, lip), min + size + ImVec2(0.f, lip), colors.lip, radii);
    AddRoundRectFilled(dl, face, face + size, colors.face, radii);
    if (colors.stroke) AddRoundRect(dl, face, face + size, colors.stroke, radii, 1.f);
    return face;
}

// 45deg "/" stripes clipped to a rounded rect with per-corner radii
void AddHatchRounded(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 col, float spacing, float thickness,
                     const float radii[4])
{
    // keep the stroke's width inside the shape
    const float inset = thickness * 0.5f;
    const ImVec2 a = min + ImVec2(inset, inset), b = max - ImVec2(inset, inset);
    if (b.x <= a.x || b.y <= a.y || spacing <= 0.f) return;
    const ImVec2 mid = (a + b) * 0.5f;
    const float limit = ImMin(b.x - a.x, b.y - a.y) * 0.5f;
    float r[4];
    for (int i = 0; i < 4; i++)
        r[i] = ImClamp(radii[i] - inset, 0.f, limit);

    // moves p along p->q onto the corner arc when it sits in a corner outside it; false when the segment misses
    auto clipEnd = [&](ImVec2& p, const ImVec2& q) {
        const bool left = p.x < mid.x, top = p.y < mid.y;
        const float rad = r[top ? (left ? 0 : 1) : (left ? 3 : 2)];
        if (rad <= 0.f) return true;
        const ImVec2 c(left ? a.x + rad : b.x - rad, top ? a.y + rad : b.y - rad);
        const bool inCorner = (left ? p.x < c.x : p.x > c.x) && (top ? p.y < c.y : p.y > c.y);
        if (!inCorner || ImLengthSqr(p - c) <= rad * rad) return true;
        const ImVec2 d = q - p, f = p - c;
        const float qa = ImLengthSqr(d), qb = 2.f * (f.x * d.x + f.y * d.y), qc = ImLengthSqr(f) - rad * rad;
        const float disc = qb * qb - 4.f * qa * qc;
        if (qa <= 0.f || disc < 0.f) return false;
        const float t = (-qb - sqrtf(disc)) / (2.f * qa);
        if (t < 0.f || t > 1.f) return false;
        p += d * t;
        return true;
    };

    const float step = spacing * 1.41421356f; // perpendicular spacing -> spacing along x + y
    for (float k = a.x + a.y + step * 0.5f; k < b.x + b.y; k += step) {
        const float x0 = ImMax(a.x, k - b.y), x1 = ImMin(b.x, k - a.y);
        if (x1 - x0 < 0.5f) continue;
        ImVec2 p0(x0, k - x0), p1(x1, k - x1);
        if (!clipEnd(p0, p1) || !clipEnd(p1, p0)) continue;
        dl->AddLine(p0, p1, col, thickness);
    }
}

float GlyphSize(UiGlyph glyph)
{
    switch (glyph) {
    case UiGlyph_Gear:
    case UiGlyph_Padlock: return 14.f;
    case UiGlyph_Search: return 13.f;
    case UiGlyph_Arrow: return 9.f;
    case UiGlyph_Power: return 28.f;
    case UiGlyph_ChevronLeft:
    case UiGlyph_ChevronRight: return 10.f;
    default: return 12.f;
    }
}

struct KeyLook {
    UiCapKind cap;
    ImFont* font;
    ImU32 ink, dot;
    int dots;
};

KeyLook GetKeyLook(UiKeyKind kind, bool paused)
{
    const ImU32 pausedAutoDot = UiWithAlpha(UiCol::PausedAutoInk, 0.9f);
    const ImU32 pausedTapDot = UiWithAlpha(UiCol::TapeInk, 0.9f);
    const ImU32 whiteDot = UiWithAlpha(UiCol::White, 0.9f);
    const ImU32 sameDot = UiWithAlpha(UiCol::SameInk, 0.7f);
    switch (kind) {
    case UiKeyKind_Auto:
        if (paused) return {UiCapKind_PausedAuto, UiFonts::Bold, UiCol::PausedAutoInk, pausedAutoDot, 3};
        return {UiCapKind_Signal, UiFonts::Bold, UiCol::White, whiteDot, 3};
    case UiKeyKind_Tap:
        if (paused) return {UiCapKind_PausedTap, UiFonts::Bold, UiCol::TapeInk, pausedTapDot, 1};
        return {UiCapKind_Tap, UiFonts::Bold, UiCol::White, whiteDot, 1};
    case UiKeyKind_SameAuto:
        if (paused) return {UiCapKind_PausedAuto, UiFonts::SemiBold, UiCol::PausedAutoInk, pausedAutoDot, 3};
        return {UiCapKind_Same, UiFonts::SemiBold, UiCol::SameInk, sameDot, 3};
    case UiKeyKind_SameTap:
        if (paused) return {UiCapKind_PausedTap, UiFonts::SemiBold, UiCol::TapeInk, pausedTapDot, 1};
        return {UiCapKind_Same, UiFonts::SemiBold, UiCol::SameInk, sameDot, 1};
    case UiKeyKind_Block: return {UiCapKind_Plain, UiFonts::Bold, UiCol::Block, 0, 0};
    case UiKeyKind_Locked: return {UiCapKind_Locked, UiFonts::Medium, UiCol::Ink3, 0, 0};
    default: return {UiCapKind_Plain, UiFonts::Medium, UiCol::Ink2, 0, 0};
    }
}

float RoundRectDistance(const ImVec2& p, const ImVec2& min, const ImVec2& max, float r)
{
    const ImVec2 c = (min + max) * 0.5f, half = (max - min) * 0.5f;
    const float qx = fabsf(p.x - c.x) - (half.x - r), qy = fabsf(p.y - c.y) - (half.y - r);
    const float ox = ImMax(qx, 0.f), oy = ImMax(qy, 0.f);
    return sqrtf(ox * ox + oy * oy) + ImMin(ImMax(qx, qy), 0.f) - r;
}

// css-like box shadow (offset, gaussian blur) of a rounded rect, drawn only outside the rect: an opaque panel hides
// the rest anyway, and it has already been drawn by the time we get here
void AddShadow(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float rounding, const ImVec2& offset, float blur,
               ImU32 col)
{
    constexpr int kCornerSegs = 8, kPoints = (kCornerSegs + 1) * 4, kRings = 10;
    const ImVec2 centers[4] = {min + ImVec2(rounding, rounding), ImVec2(max.x - rounding, min.y + rounding),
                               max - ImVec2(rounding, rounding), ImVec2(min.x + rounding, max.y - rounding)};
    ImVec2 dirs[kPoints];
    for (int j = 0; j < kPoints; j++) {
        const int corner = j / (kCornerSegs + 1), seg = j % (kCornerSegs + 1);
        const float a = IM_PI * (1.f + 0.5f * (float)corner + 0.5f * (float)seg / kCornerSegs);
        dirs[j] = ImVec2(cosf(a), sinf(a));
    }

    const float reachX = blur + fabsf(offset.x);
    const float reachUp = ImMax(blur - offset.y, 2.f), reachDown = ImMax(blur + offset.y, 2.f);
    const ImVec2 shadowMin = min + offset, shadowMax = max + offset;
    const float sigma = ImMax(blur * 0.5f, 0.5f);

    dl->PushClipRect(min - ImVec2(reachX, reachUp), max + ImVec2(reachX, reachDown), false);
    const ImVec2 uv = dl->_Data->TexUvWhitePixel;
    dl->PrimReserve(kRings * kPoints * 6, (kRings + 1) * kPoints);
    const unsigned base = dl->_VtxCurrentIdx;
    for (int i = 0; i <= kRings; i++) {
        // rings bunch up near the edge where the falloff is steepest
        const float t = powf((float)i / kRings, 1.5f);
        for (int j = 0; j < kPoints; j++) {
            const ImVec2 d = dirs[j], at = centers[j / (kCornerSegs + 1)];
            const ImVec2 p(at.x + d.x * (rounding + t * reachX),
                           at.y + d.y * (rounding + t * (d.y > 0.f ? reachDown : reachUp)));
            const float dist = RoundRectDistance(p, shadowMin, shadowMax, rounding);
            const float k = i == kRings ? 0.f : 0.5f * std::erfc(dist / (sigma * 1.41421356f));
            dl->PrimWriteVtx(p, uv, UiWithAlpha(col, k));
        }
    }
    for (int i = 0; i < kRings; i++)
        for (int j = 0; j < kPoints; j++) {
            const ImDrawIdx a = (ImDrawIdx)(base + i * kPoints + j);
            const ImDrawIdx b = (ImDrawIdx)(base + i * kPoints + (j + 1) % kPoints);
            const ImDrawIdx c = (ImDrawIdx)(a + kPoints);
            const ImDrawIdx d = (ImDrawIdx)(b + kPoints);
            for (const ImDrawIdx idx : {a, b, d, a, d, c})
                dl->PrimWriteIdx(idx);
        }
    dl->PopClipRect();
}

bool BeginPopupAnimated(const char* str_id, bool modal)
{
    const ImGuiID animId = SubId(GetID(str_id), AnimSlot_Popup);
    float eased = 1.f;
    if (!IsPopupOpen(str_id)) {
        s_animStorage.SetFloat(animId, 0.f);
    } else {
        const float t = UiAnim(animId, 1.f, 18.f, 0.f);
        eased = 1.f - (1.f - t) * (1.f - t);
        ImGuiContext& g = *GImGui;
        if (g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasPos)
            g.NextWindowData.PosVal.y -= 8.f * (1.f - eased);
    }

    // a modal takes the regular window style, a popup the popup style: make both look like a panel
    PushStyleVar(ImGuiStyleVar_Alpha, eased);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.f, 18.f));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 14.f);
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
    PushStyleColor(ImGuiCol_WindowBg, UiCol::Panel);
    const bool open = modal ? BeginPopupModal(str_id, nullptr,
                                              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                                                  ImGuiWindowFlags_NoSavedSettings)
                            : BeginPopup(str_id);
    if (!open) {
        PopStyleColor();
        PopStyleVar(4);
        return false;
    }
    const ImGuiWindow* window = GetCurrentWindow();
    AddShadow(window->DrawList, window->Pos, window->Pos + window->Size, window->WindowRounding, ImVec2(0.f, 14.f),
              36.f, UiWithAlpha(UiCol::Shadow, 0.22f * GetStyle().Alpha));
    s_popupVtxStart.push_back(window->DrawList->VtxBuffer.Size);
    return true;
}
} // namespace

void ImGui::Tip(const char* fmt, ...)
{
    if (IsItemHovered() && GImGui->HoveredIdTimer > 1.f) {
        va_list args;
        va_start(args, fmt);
        PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 7.f));
        PushStyleVar(ImGuiStyleVar_PopupRounding, 6.f);
        SetTooltipV(fmt, args);
        PopStyleVar(2);
        va_end(args);
    }
}

ImU32 ImGui::UiFlash(ImU32 col, float periodSec, float minAlpha)
{
    const float phase = fmodf((float)GetTime() / periodSec, 1.f);
    const float alpha = minAlpha + (1.f - minAlpha) * fabsf(sinf(2.f * IM_PI * phase));
    return (col & ~IM_COL32_A_MASK) | ((ImU32)IM_F32_TO_INT8_SAT(alpha * GetStyle().Alpha) << IM_COL32_A_SHIFT);
}

void ImGui::LoadUiFonts()
{
    ImGuiIO& io = GetIO();
    io.Fonts->Clear();

    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    // ImGui sizes a font by ascent - descent, 1.088em for Archivo; scale glyphs up so sizes are em sizes like the
    // mockups' (the line box stays at the requested size)
    cfg.ExtraSizeScale = 1.088f;

    auto add = [&](const unsigned char* data, unsigned int size) {
        return io.Fonts->AddFontFromMemoryTTF((void*)data, (int)size, 13.f, &cfg);
    };
    UiFonts::Light = add(ArchivoLight_data, ArchivoLight_size);
    UiFonts::Medium = add(ArchivoMedium_data, ArchivoMedium_size);
    UiFonts::SemiBold = add(ArchivoSemiBold_data, ArchivoSemiBold_size);
    UiFonts::Bold = add(ArchivoBold_data, ArchivoBold_size);
    UiFonts::Heavy = add(ArchivoExtraBold_data, ArchivoExtraBold_size);
    io.FontDefault = UiFonts::Medium;
}

void ImGui::LoadUiStyle()
{
    ImGuiStyle& style = GetStyle();
    StyleColorsLight(&style);
    // the main window is laid out in absolute coordinates and its padding would only clip its edges; popups and
    // tooltips push their own
    style.WindowPadding = ImVec2(0.f, 0.f);
    style.FramePadding = ImVec2(10.f, 6.f);
    style.CellPadding = ImVec2(6.f, 6.f);
    // panel rows carry their own spacing
    style.ItemSpacing = ImVec2(8.f, 0.f);
    style.ItemInnerSpacing = ImVec2(6.f, 6.f);
    style.IndentSpacing = 20.f;
    style.ScrollbarSize = 8.f;
    style.WindowBorderSize = 0.f;
    style.ChildBorderSize = 1.f;
    style.PopupBorderSize = 1.f;
    style.FrameBorderSize = 0.f;
    style.WindowRounding = 0.f;
    style.ChildRounding = 10.f;
    style.FrameRounding = 8.f;
    style.PopupRounding = 14.f;
    style.ScrollbarRounding = 9.f;
    style.GrabRounding = 6.f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);

    static constexpr struct {
        ImGuiCol idx;
        ImU32 col;
    } s_colors[] = {
        {ImGuiCol_Text, UiCol::Ink},
        {ImGuiCol_TextDisabled, UiCol::Ink3},
        {ImGuiCol_WindowBg, UiCol::Chassis},
        {ImGuiCol_ChildBg, 0},
        {ImGuiCol_PopupBg, UiCol::Panel},
        {ImGuiCol_Border, UiCol::PanelStroke},
        {ImGuiCol_BorderShadow, 0},
        {ImGuiCol_FrameBg, UiCol::Recess},
        {ImGuiCol_FrameBgHovered, IM_COL32(0xCF, 0xC9, 0xBF, 0xFF)},
        {ImGuiCol_FrameBgActive, UiCol::RecessStroke},
        {ImGuiCol_TitleBg, UiCol::Chassis},
        {ImGuiCol_TitleBgActive, UiCol::Chassis},
        {ImGuiCol_TitleBgCollapsed, UiCol::Chassis},
        {ImGuiCol_MenuBarBg, UiCol::Chassis},
        {ImGuiCol_ScrollbarBg, 0},
        {ImGuiCol_ScrollbarGrab, UiCol::CapLip},
        {ImGuiCol_ScrollbarGrabHovered, UiCol::Ink3},
        {ImGuiCol_ScrollbarGrabActive, UiCol::Ink2},
        {ImGuiCol_CheckMark, UiCol::Signal},
        {ImGuiCol_SliderGrab, UiCol::Signal},
        {ImGuiCol_SliderGrabActive, UiCol::SignalLip},
        {ImGuiCol_Button, UiCol::Cap},
        {ImGuiCol_ButtonHovered, UiCol::RowHover},
        {ImGuiCol_ButtonActive, UiCol::Recess},
        {ImGuiCol_Header, UiCol::RowHover},
        {ImGuiCol_HeaderHovered, UiCol::RowHover},
        {ImGuiCol_HeaderActive, UiCol::Recess},
        {ImGuiCol_Separator, UiCol::Divider},
        {ImGuiCol_SeparatorHovered, UiCol::Line},
        {ImGuiCol_SeparatorActive, UiCol::Ink3},
        {ImGuiCol_InputTextCursor, UiCol::Ink},
        {ImGuiCol_TextLink, UiCol::Signal},
        {ImGuiCol_TextSelectedBg, UiWithAlpha(UiCol::Signal, 0.3f)},
        {ImGuiCol_NavCursor, UiWithAlpha(UiCol::Signal, 0.6f)},
        {ImGuiCol_ModalWindowDimBg, UiWithAlpha(UiCol::Chassis, 0.6f)},
    };
    for (const auto& [idx, col] : s_colors)
        style.Colors[idx] = ColorConvertU32ToFloat4(col);
}

float ImGui::UiAnim(ImGuiID id, float target, float speed, float initial)
{
    float* v = s_animStorage.GetFloatRef(id, initial == FLT_MAX ? target : initial);
    *v += (target - *v) * (1.f - expf(-speed * GImGui->IO.DeltaTime));
    if (fabsf(target - *v) < 0.001f) *v = target;
    return *v;
}

ImU32 ImGui::UiMixColor(ImU32 a, ImU32 b, float t)
{
    t = ImClamp(t, 0.f, 1.f);
    ImU32 out = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        const float ca = (float)((a >> shift) & 0xFF);
        const float cb = (float)((b >> shift) & 0xFF);
        out |= (ImU32)(ca + (cb - ca) * t + 0.5f) << shift;
    }
    return out;
}

bool ImGui::UiBeginPopup(const char* str_id)
{
    return BeginPopupAnimated(str_id, false);
}

// dims and blocks everything underneath; the caller decides how it gets dismissed
bool ImGui::UiBeginModal(const char* str_id)
{
    return BeginPopupAnimated(str_id, true);
}

void ImGui::UiEndPopup()
{
    if (!s_popupVtxStart.empty()) {
        // our draw-list content ignores style alpha, fade it in with the window
        const float alpha = GetStyle().Alpha;
        if (alpha < 1.f) ScaleVtxAlpha(GetWindowDrawList(), s_popupVtxStart.back(), alpha);
        s_popupVtxStart.pop_back();
    }
    EndPopup();
    PopStyleColor();
    PopStyleVar(4);
}

// ---- text

float ImGui::UiTextWidth(ImFont* font, float size, const char* text, float tracking)
{
    if (tracking == 0.f) return font->CalcTextSizeA(size, FLT_MAX, 0.f, text).x;
    return CalcTrackedTextSize(font, size, text, tracking).x;
}

ImVec2 ImGui::CalcTrackedTextSize(ImFont* font, float size, const char* text, float tracking)
{
    float width = ForEachTrackedGlyph(font, size, text, tracking, 0.f, [](const char*, const char*, float) {});
    if (width > 0.f) width -= tracking;
    return ImVec2(width, size);
}

void ImGui::AddTrackedText(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 col, const char* text,
                           float tracking)
{
    if (tracking == 0.f) {
        dl->AddText(font, size, pos, col, text);
        return;
    }
    ForEachTrackedGlyph(font, size, text, tracking, pos.x, [&](const char* begin, const char* end, float x) {
        dl->AddText(font, size, ImVec2(x, pos.y), col, begin, end);
    });
}

void ImGui::AddSilk(ImDrawList* dl, const ImVec2& pos, const char* text, ImU32 col)
{
    AddTrackedText(dl, UiFonts::SemiBold, 9.f, pos, col, Upper(text), 1.6f);
}

float ImGui::SilkWidth(const char* text)
{
    return UiTextWidth(UiFonts::SemiBold, 9.f, Upper(text), 1.6f);
}

// ---- drawing primitives

ImVec2 ImGui::AddCap(ImDrawList* dl, const ImVec2& min, const ImVec2& size, UiCapKind kind, float rounding,
                     float pressT, float hoverT)
{
    const float radii[4] = {rounding, rounding, rounding, rounding};
    return AddCap(dl, min, size, kind, radii, pressT, hoverT);
}

ImVec2 ImGui::AddCap(ImDrawList* dl, const ImVec2& min, const ImVec2& size, UiCapKind kind, const float radii[4],
                     float pressT, float hoverT)
{
    CapColors colors = CapPalette(kind);
    const bool outlineHover = kind == UiCapKind_Plain;
    if (!outlineHover && kind != UiCapKind_Locked) colors.face = UiMixColor(colors.face, UiCol::White, 0.1f * hoverT);
    const ImVec2 face = DrawCap(dl, min, size, radii, colors, pressT);
    if (outlineHover && hoverT > 0.01f)
        AddRoundRect(dl, face, face + size, UiWithAlpha(UiCol::Ink2, hoverT), radii, 1.5f);
    return face;
}

void ImGui::AddRoundRectFilled(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 col, const float radii[4])
{
    if ((col & IM_COL32_A_MASK) == 0 || max.x <= min.x || max.y <= min.y) return;
    PathRoundRect(dl, min, max, radii);
    dl->PathFillConvex(col);
}

void ImGui::AddRoundRect(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 col, const float radii[4],
                         float thickness)
{
    if ((col & IM_COL32_A_MASK) == 0 || max.x <= min.x || max.y <= min.y) return;
    // like AddRect: stroke the half-pixel inset path so a 1px line lands on whole pixels
    const float inset[4] = {radii[0] - 0.5f, radii[1] - 0.5f, radii[2] - 0.5f, radii[3] - 0.5f};
    PathRoundRect(dl, min + ImVec2(0.5f, 0.5f), max - ImVec2(0.5f, 0.5f), inset);
    dl->PathStroke(col, ImDrawFlags_Closed, thickness);
}

void ImGui::AddHatch(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 col, float spacing, float thickness)
{
    const float square[4] = {};
    AddHatchRounded(dl, min, max, col, spacing, thickness, square);
}

void ImGui::AddGlyph(ImDrawList* dl, const ImVec2& c, float size, UiGlyph glyph, ImU32 col)
{
    const float h = size * 0.5f;
    switch (glyph) {
    case UiGlyph_Gear: {
        const float t = size * (1.8f / 14.f);
        dl->AddCircle(c, h - t * 0.5f, col, 0, t);
        dl->AddCircleFilled(c, size * (2.f / 14.f), col);
        break;
    }
    case UiGlyph_Minimize: dl->AddLine(c - ImVec2(h, 0.f), c + ImVec2(h, 0.f), col, size * 0.15f); break;
    case UiGlyph_Close: {
        const float t = size * 0.15f;
        dl->AddLine(c - ImVec2(h, h), c + ImVec2(h, h), col, t);
        dl->AddLine(c + ImVec2(-h, h), c + ImVec2(h, -h), col, t);
        break;
    }
    case UiGlyph_Plus: dl->AddLine(c - ImVec2(0.f, h), c + ImVec2(0.f, h), col, size * (1.6f / 12.f)); [[fallthrough]];
    case UiGlyph_Minus: dl->AddLine(c - ImVec2(h, 0.f), c + ImVec2(h, 0.f), col, size * (1.6f / 12.f)); break;
    case UiGlyph_Search: {
        // drawn on a 13px grid: an 11px ring up-left, the handle down-right
        const float s = size / 13.f, t = 1.6f * s;
        dl->AddCircle(c - ImVec2(s, s), 5.5f * s - t * 0.5f, col, 0, t);
        dl->AddLine(c + ImVec2(2.6f, 2.6f) * s, c + ImVec2(6.5f, 6.5f) * s, col, t);
        break;
    }
    case UiGlyph_Arrow: {
        const float t = ImMax(1.2f, size * 0.17f);
        dl->AddLine(c + ImVec2(-h, h), c + ImVec2(h, -h), col, t);
        dl->PathLineTo(ImVec2(c.x - h * 0.3f, c.y - h));
        dl->PathLineTo(ImVec2(c.x + h, c.y - h));
        dl->PathLineTo(ImVec2(c.x + h, c.y + h * 0.3f));
        dl->PathStroke(col, 0, t);
        break;
    }
    case UiGlyph_Power: {
        const float t = size * (3.f / 28.f), r = h - t * 0.5f, gap = 0.62f;
        const float a0 = -IM_PI * 0.5f + gap, a1 = IM_PI * 1.5f - gap;
        dl->PathArcTo(c, r, a0, a1);
        dl->PathStroke(col, 0, t);
        // round caps
        dl->AddCircleFilled(c + ImVec2(cosf(a0), sinf(a0)) * r, t * 0.5f, col);
        dl->AddCircleFilled(c + ImVec2(cosf(a1), sinf(a1)) * r, t * 0.5f, col);
        const ImVec2 top(c.x, c.y - h), bottom(c.x, c.y - h * 0.08f);
        dl->AddLine(top, bottom, col, t);
        dl->AddCircleFilled(top, t * 0.5f, col);
        dl->AddCircleFilled(bottom, t * 0.5f, col);
        break;
    }
    case UiGlyph_Padlock: {
        // body in the lower half, shackle above it; size is the height
        const float t = size * 0.11f, w = size * 0.36f, rs = size * 0.22f;
        dl->AddRectFilled(ImVec2(c.x - w, c.y - size * 0.04f), ImVec2(c.x + w, c.y + h), col, size * 0.12f);
        const float shackleY = c.y - h + rs + t * 0.5f;
        dl->PathLineTo(ImVec2(c.x - rs, c.y));
        dl->PathArcTo(ImVec2(c.x, shackleY), rs, IM_PI, IM_PI * 2.f);
        dl->PathLineTo(ImVec2(c.x + rs, c.y));
        dl->PathStroke(col, 0, t);
        break;
    }
    case UiGlyph_ChevronLeft:
    case UiGlyph_ChevronRight: {
        const float dir = glyph == UiGlyph_ChevronLeft ? 1.f : -1.f, w = size * 0.15f * dir;
        dl->PathLineTo(ImVec2(c.x + w, c.y - h));
        dl->PathLineTo(ImVec2(c.x - w, c.y));
        dl->PathLineTo(ImVec2(c.x + w, c.y + h));
        dl->PathStroke(col, 0, size * 0.16f);
        break;
    }
    }
}

void ImGui::AddRecess(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float rounding)
{
    dl->AddRectFilled(min, max, UiCol::Recess, rounding);
    dl->AddRect(min, max, UiCol::RecessStroke, rounding);
}

void ImGui::AddRateBadge(ImDrawList* dl, const ImVec2& anchor, const char* text, float alpha, float pop)
{
    if (alpha <= 0.f) return;
    const float width = ImMax(60.f, UiTextWidth(UiFonts::Bold, 11.f, text) + 20.f);
    const ImVec2 max(anchor.x + width * 0.5f, anchor.y - 5.f);
    const ImVec2 min(anchor.x - width * 0.5f, max.y - 22.f);
    const ImU32 fill = UiWithAlpha(UiCol::Ink, alpha);
    dl->AddRectFilled(min, max, fill, 5.f);
    // overlaps the box by half a pixel so no seam shows; clockwise for the anti-aliased fringe
    dl->AddTriangleFilled(ImVec2(anchor.x - 5.f, max.y - 0.5f), ImVec2(anchor.x + 5.f, max.y - 0.5f), anchor, fill);
    AddTextCentered(dl, UiFonts::Bold, 11.f * pop, (min + max) * 0.5f, UiWithAlpha(UiCol::White, alpha), text);
}

float ImGui::TapeWidth(const char* text)
{
    return UiTextWidth(UiFonts::Bold, 10.f, Upper(text), 1.2f) + 20.f;
}

void ImGui::AddTape(ImDrawList* dl, const ImVec2& pos, const char* text)
{
    dl->AddRectFilled(pos, pos + ImVec2(TapeWidth(text), 24.f), UiCol::Tape, 2.f);
    AddTrackedText(dl, UiFonts::Bold, 10.f, pos + ImVec2(10.f, 7.f), UiCol::TapeInk, Upper(text), 1.2f);
}

// ---- controls

UiCapHit ImGui::UiCapFrame(const char* id, const ImVec2& pos, const ImVec2& size, UiCapKind kind, float rounding)
{
    // the hit box covers the lip too
    const HitBox hit = PlaceInvisibleButton(id, pos, size + ImVec2(0.f, 4.f));
    UiCapHit out;
    out.clicked = hit.clicked;
    out.hoverT = hit.HoverAnim();
    out.pressT = hit.PressAnim();
    out.face = AddCap(GetWindowDrawList(), pos, size, kind, rounding, out.pressT, out.hoverT);
    return out;
}

bool ImGui::UiButton(const char* id, const ImVec2& pos, const ImVec2& size, const char* label, UiButtonKind kind)
{
    const UiCapHit hit = UiCapFrame(id, pos, size, kind == UiButtonKind_Primary ? UiCapKind_Signal : UiCapKind_Plain,
                                    8.f);
    const ImU32 col = kind == UiButtonKind_Primary ? UiCol::White
                      : kind == UiButtonKind_Danger ? UiCol::Block
                                                    : UiCol::Ink;
    AddTextCentered(GetWindowDrawList(), UiFonts::Bold, 13.f, hit.face + size * 0.5f, col, label);
    return hit.clicked;
}

bool ImGui::UiCapIconButton(const char* id, const ImVec2& pos, float size, UiGlyph glyph)
{
    const UiCapHit hit = UiCapFrame(id, pos, ImVec2(size, size), UiCapKind_Plain, 6.f);
    AddGlyph(GetWindowDrawList(), hit.face + ImVec2(size, size) * 0.5f, GlyphSize(glyph) * size / 30.f, glyph,
             UiMixColor(UiCol::Ink2, UiCol::Ink, hit.hoverT));
    return hit.clicked;
}

bool ImGui::UiIconButton(const char* id, const ImVec2& pos, float size, UiGlyph glyph)
{
    const ImVec2 box(size, size);
    const HitBox hit = PlaceInvisibleButton(id, pos, box);
    const float hoverT = hit.HoverAnim();
    const float pressT = hit.PressAnim();

    ImDrawList* dl = GetWindowDrawList();
    if (hoverT > 0.01f)
        dl->AddRectFilled(pos, pos + box, UiWithAlpha(UiMixColor(UiCol::Recess, UiCol::RecessStroke, pressT), hoverT),
                          8.f);
    const ImU32 col = UiMixColor(UiCol::Ink2, glyph == UiGlyph_Close ? UiCol::Block : UiCol::Ink, hoverT);
    AddGlyph(dl, pos + box * 0.5f, GlyphSize(glyph) * size / 30.f * (1.f - 0.1f * pressT), glyph, col);
    return hit.clicked;
}

float ImGui::OutlineButtonWidth(const char* label)
{
    return UiTextWidth(UiFonts::Bold, 10.f, Upper(label), 1.2f) + 20.f;
}

bool ImGui::UiOutlineButton(const char* id, const ImVec2& pos, const char* label)
{
    const ImVec2 size(OutlineButtonWidth(label), 24.f);
    const HitBox hit = PlaceInvisibleButton(id, pos, size);
    const float hoverT = hit.HoverAnim();
    const float pressT = hit.PressAnim();

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = pos + size;
    if (hoverT > 0.01f)
        dl->AddRectFilled(pos, max, UiWithAlpha(UiMixColor(UiCol::Recess, UiCol::RecessStroke, pressT), hoverT), 2.f);
    dl->AddRect(pos, max, UiCol::Ink3, 2.f);
    AddTrackedText(dl, UiFonts::Bold, 10.f, pos + ImVec2(10.f, 7.f), UiMixColor(UiCol::Ink2, UiCol::Ink, hoverT),
                   Upper(label), 1.2f);
    return hit.clicked;
}

bool ImGui::UiToggle(const char* id, const ImVec2& pos, bool on, bool dim)
{
    const ImVec2 size(46.f, 26.f);
    const HitBox hit = PlaceInvisibleButton(id, pos, size);
    const float hoverT = hit.HoverAnim();
    const float t = hit.Anim(AnimSlot_State, on, 16.f);
    const float eased = t * t * (3.f - 2.f * t);

    ImDrawList* dl = GetWindowDrawList();
    const AlphaScope fade(dl, dim ? 0.5f : 1.f);
    const ImVec2 max = pos + size;
    dl->AddRectFilled(pos, max, UiCol::Recess, 13.f);
    if (eased < 1.f)
        dl->AddRect(pos, max, UiWithAlpha(UiMixColor(UiCol::RecessStroke, UiCol::Ink3, 0.6f * hoverT), 1.f - eased),
                    13.f);
    if (eased > 0.f)
        dl->AddRectFilled(pos, max, UiWithAlpha(UiMixColor(UiCol::Signal, UiCol::White, 0.08f * hoverT), eased), 13.f);

    // off: a plain cap with a 2px lip at the left; on: a flat white disc at the right
    const ImVec2 knob(pos.x + 13.f + 20.f * eased, pos.y + 13.f);
    if (eased < 1.f) dl->AddCircleFilled(knob + ImVec2(0.f, 2.f), 10.f, UiWithAlpha(UiCol::CapLip, 1.f - eased));
    dl->AddCircleFilled(knob, 10.f, UiMixColor(UiCol::Cap, UiCol::White, eased));
    if (eased < 1.f) dl->AddCircle(knob, 9.5f, UiWithAlpha(UiCol::CapStroke, 1.f - eased));
    return hit.clicked;
}

float ImGui::TabWidth(const char* label)
{
    return 28.f + UiTextWidth(UiFonts::Bold, 13.f, label) + 21.f;
}

bool ImGui::UiTab(const char* id, const ImVec2& pos, const char* label, bool selected, bool lit, bool dim)
{
    const ImVec2 size(TabWidth(label), 34.f);
    const HitBox hit = PlaceInvisibleButton(id, pos, size + ImVec2(0.f, selected ? 4.f : 0.f));
    const float hoverT = hit.HoverAnim();
    const float pressT = hit.Anim(AnimSlot_Press, selected && IsItemActive(), 28.f);

    ImDrawList* dl = GetWindowDrawList();
    ImVec2 face = pos;
    if (selected) {
        face = AddCap(dl, pos, size, UiCapKind_Plain, 8.f, pressT);
    } else {
        dl->AddRectFilled(pos, pos + size, UiMixColor(UiCol::Recess, UiCol::RecessStroke, 0.3f * hoverT), 8.f);
        dl->AddRect(pos, pos + size, UiCol::RecessStroke, 8.f);
    }

    const ImU32 dot = !lit ? UiCol::DotOff : dim ? UiCol::Ink3 : UiCol::Signal;
    dl->AddCircleFilled(face + ImVec2(16.f, 17.f), 3.f, dot);
    dl->AddText(selected ? UiFonts::Bold : UiFonts::SemiBold, 13.f, face + ImVec2(28.f, 9.f),
                selected ? UiCol::Ink : UiMixColor(UiCol::Ink2, UiCol::Ink, hoverT), label);
    return hit.clicked;
}

bool ImGui::UiSegmented(const char* id, const ImVec2& pos, const ImVec2& size, std::span<const char* const> labels,
                        int& index, bool uppercase, const UiCapKind* chosenKinds, bool dim)
{
    const int count = (int)labels.size();
    const HitBox hit = PlaceInvisibleButton(id, pos, size);
    if (count == 0) return false;
    const float segW = size.x / (float)count;
    const int hovered = IsItemHovered() ? ImClamp((int)((GetMousePos().x - pos.x) / segW), 0, count - 1) : -1;

    bool changed = false;
    if (hit.clicked && hovered >= 0 && hovered != index) {
        index = hovered;
        changed = true;
    }
    const int chosen = ImClamp(index, 0, count - 1);
    const float slide = UiAnim(SubId(hit.id, AnimSlot_State), (float)chosen, 20.f);
    const float pressT = hit.Anim(AnimSlot_Press, IsItemActive() && hovered == chosen, 28.f);

    ImDrawList* dl = GetWindowDrawList();
    const AlphaScope fade(dl, dim ? 0.5f : 1.f);
    AddRecess(dl, pos, pos + size, size.y <= 32.f ? 8.f : 9.f);
    const UiCapKind kind = chosenKinds ? chosenKinds[chosen] : UiCapKind_Plain;
    const float capH = size.y - 9.f;
    AddCap(dl, ImVec2(pos.x + slide * segW + 3.f, pos.y + 3.f), ImVec2(segW - 6.f, capH), kind, 6.f, pressT);

    const float textSize = uppercase ? 11.f : 12.f, tracking = uppercase ? 0.6f : 0.f;
    const float centerY = pos.y + 3.f + capH * 0.5f;
    for (int i = 0; i < count; i++) {
        const Upper upper(labels[i]);
        const char* text = uppercase ? (const char*)upper : labels[i];
        const float hoverT = UiAnim(SubId(hit.id, AnimSlot_Hover, i), hovered == i ? 1.f : 0.f, 18.f);
        // the text takes the cap's ink as the cap slides under it
        const float under = ImSaturate(1.f - fabsf(slide - (float)i));
        const ImU32 col = UiMixColor(UiMixColor(UiCol::Ink2, UiCol::Ink, hoverT), CapInk(kind), under);
        const bool isChosen = i == chosen;
        AddTextCentered(dl, isChosen ? UiFonts::Bold : UiFonts::SemiBold, textSize,
                        ImVec2(pos.x + segW * ((float)i + 0.5f), centerY + (isChosen ? 3.f * pressT : 0.f)), col, text,
                        tracking);
    }
    return changed;
}

bool ImGui::UiStepper(const char* id, const ImVec2& pos, const ImVec2& size, const char* value, int& index, int count)
{
    const HitBox hit = PlaceInvisibleButton(id, pos, size);
    const bool hovered = IsItemHovered();
    const float midX = pos.x + size.x * 0.5f;
    const bool leftHalf = GetMousePos().x < midX;
    const int last = ImMax(count - 1, 0);

    bool changed = false;
    if (hit.clicked) {
        const int next = ImClamp(index + (leftHalf ? -1 : 1), 0, last);
        changed = next != index;
        index = next;
    }
    const float leftT = UiAnim(SubId(hit.id, AnimSlot_Hover, 0), hovered && leftHalf ? 1.f : 0.f, 18.f);
    const float rightT = UiAnim(SubId(hit.id, AnimSlot_Hover, 1), hovered && !leftHalf ? 1.f : 0.f, 18.f);

    ImDrawList* dl = GetWindowDrawList();
    AddRecess(dl, pos, pos + size, 7.f);
    const float cy = pos.y + size.y * 0.5f;
    const ImU32 leftCol = index > 0 ? UiMixColor(UiCol::Ink3, UiCol::Ink, leftT) : UiCol::DotOff;
    const ImU32 rightCol = index < last ? UiMixColor(UiCol::Ink3, UiCol::Ink, rightT) : UiCol::DotOff;
    AddTextCentered(dl, UiFonts::SemiBold, 15.f, ImVec2(pos.x + 12.f, cy), leftCol, "\xE2\x80\xB9");
    AddTextCentered(dl, UiFonts::SemiBold, 15.f, ImVec2(pos.x + size.x - 12.f, cy), rightCol, "\xE2\x80\xBA");
    AddTextCentered(dl, UiFonts::Bold, 12.f, ImVec2(midX, cy), UiCol::Ink, value);
    return changed;
}

bool ImGui::UiPowerButton(const char* id, const ImVec2& center, bool on)
{
    constexpr float r = 37.f;
    const ImVec2 min = center - ImVec2(r, r), size(r * 2.f, r * 2.f);
    const HitBox hit = PlaceInvisibleButton(id, min, size + ImVec2(0.f, 4.f));
    const float hoverT = hit.HoverAnim();
    const float pressT = hit.PressAnim();
    const float t = hit.Anim(AnimSlot_State, on, 12.f);

    const CapColors colors = {
        UiMixColor(UiMixColor(UiCol::Cap, UiCol::Signal, t), UiCol::White, 0.1f * hoverT),
        UiMixColor(UiCol::CapLip, UiCol::SignalLip, t),
        UiWithAlpha(UiCol::CapStroke, 1.f - t),
    };
    const float radii[4] = {r, r, r, r};
    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 face = DrawCap(dl, min, size, radii, colors, pressT);
    AddGlyph(dl, face + ImVec2(r, r), 28.f, UiGlyph_Power, UiMixColor(UiCol::Ink3, UiCol::White, t));
    return hit.clicked;
}

bool ImGui::UiKnob(const char* id, const ImVec2& center, float& t, const char* minLabel, const char* maxLabel, bool dim)
{
    constexpr float r = 36.f;
    const ImVec2 min = center - ImVec2(r, r);
    const HitBox hit = PlaceInvisibleButton(id, min, ImVec2(r * 2.f, r * 2.f + 4.f));
    const bool active = IsItemActive();
    const bool hovered = IsItemHovered();
    const ImGuiIO& io = GetIO();

    const float before = t;
    if (active) t += (io.MouseDelta.x - io.MouseDelta.y) * 0.004f;
    if (hovered && SetItemKeyOwner(ImGuiKey_MouseWheelY) && io.MouseWheel != 0.f) t += io.MouseWheel * 0.02f;
    t = ImClamp(t, 0.f, 1.f);
    const bool changed = t != before;

    // follows the hand directly while dragging, eases when stepped from elsewhere
    const ImGuiID shownId = SubId(hit.id, AnimSlot_State);
    if (active) s_animStorage.SetFloat(shownId, t);
    const float shown = UiAnim(shownId, t, 18.f);
    const float hoverT = UiAnim(SubId(hit.id, AnimSlot_Hover), hovered || active ? 1.f : 0.f, 18.f);

    ImDrawList* dl = GetWindowDrawList();
    auto dirAt = [](float f) {
        const float a = (-135.f + 270.f * f) * (IM_PI / 180.f); // from straight up, clockwise
        return ImVec2(sinf(a), -cosf(a));
    };
    for (int i = 0; i <= 20; i++) {
        const float f = (float)i / 20.f;
        const bool major = i % 5 == 0;
        const ImVec2 d = dirAt(f);
        dl->AddLine(center + d * (major ? 44.f : 48.f), center + d * 52.f,
                    f <= shown + 0.001f ? UiCol::Ink2 : UiCol::Ink3, major ? 1.6f : 1.1f);
    }
    AddTextCentered(dl, UiFonts::SemiBold, 9.f, ImVec2(center.x - 44.f, center.y + 44.5f), UiCol::Ink3, minLabel);
    AddTextCentered(dl, UiFonts::SemiBold, 9.f, ImVec2(center.x + 44.f, center.y + 44.5f), UiCol::Ink3, maxLabel);

    AddCap(dl, min, ImVec2(r * 2.f, r * 2.f), UiCapKind_Plain, r, 0.f, 0.5f * hoverT);
    dl->AddCircle(center, 26.f, UiCol::KnobRing, 0, 1.5f);
    const ImVec2 d = dirAt(shown);
    const ImVec2 a = center + d * 30.f, b = center + d * 10.f;
    const ImU32 pointer = dim ? UiCol::Ink3 : UiCol::Signal;
    dl->AddLine(a, b, pointer, 4.f);
    dl->AddCircleFilled(a, 2.f, pointer);
    dl->AddCircleFilled(b, 2.f, pointer);
    return changed;
}

bool ImGui::UiKey(const char* id, const ImVec2& pos, const ImVec2& size, const UiKeyDesc& desc)
{
    const bool interactive = desc.kind != UiKeyKind_Fixed && desc.kind != UiKeyKind_Locked;
    const HitBox hit = PlaceInvisibleButton(id, pos, size + ImVec2(0.f, 4.f));
    const float hoverT = hit.Anim(AnimSlot_Hover, interactive && IsItemHovered(), 18.f);
    const float pressT = hit.Anim(AnimSlot_Press, desc.pressed || (interactive && IsItemActive()), 30.f);

    const KeyLook look = GetKeyLook(desc.kind, desc.paused);
    const bool narrow = size.x < 30.f; // not "small": that's a macro in rpcndr.h
    const bool block = desc.kind == UiKeyKind_Block;
    float radii[4] = {desc.radii[0], desc.radii[1], desc.radii[2], desc.radii[3]};
    if (radii[0] == 0.f && radii[1] == 0.f && radii[2] == 0.f && radii[3] == 0.f)
        radii[0] = radii[1] = radii[2] = radii[3] = narrow ? 5.f : 7.f;

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 face = AddCap(dl, pos, size, look.cap, radii, pressT, block ? 0.f : hoverT);
    if (block) {
        AddHatchRounded(dl, face, face + size, UiCol::Block, 7.f, 1.2f, radii);
        if (hoverT > 0.01f) AddRoundRect(dl, face, face + size, UiWithAlpha(UiCol::Ink2, hoverT), radii, 1.5f);
    }

    const char* label = desc.label ? desc.label : "";
    if (narrow || desc.centered) {
        const float fontSize = narrow ? 9.f : 11.f;
        const float dotR = narrow ? 1.5f : 2.f, dotGap = narrow ? 5.f : 7.f;
        const float cx = face.x + size.x * 0.5f;
        const float cy = face.y + size.y * 0.5f - (look.dots ? (narrow ? 3.f : 5.f) : 0.f);
        AddTextCentered(dl, UiFonts::SemiBold, fontSize, ImVec2(cx, cy), look.ink, label);
        const float dotY = cy + fontSize * 0.5f + (narrow ? 4.f : 6.f);
        for (int i = 0; i < look.dots; i++)
            dl->AddCircleFilled(ImVec2(cx + ((float)i - (float)(look.dots - 1) * 0.5f) * dotGap, dotY), dotR, look.dot);
    } else {
        const ImVec2 at = face + ImVec2(9.f, 8.f);
        const float labelW = UiTextWidth(look.font, 12.f, label);
        if (block)
            dl->AddRectFilled(face + ImVec2(5.f, 5.f), face + ImVec2(5.f + ImMax(16.f, labelW + 8.f), 23.f), UiCol::Cap,
                              3.f);
        dl->AddText(look.font, 12.f, at, look.ink, label);
        if (desc.kind == UiKeyKind_Locked)
            AddGlyph(dl, ImVec2(at.x + labelW + (*label ? 9.f : 4.f), at.y + 4.5f), 12.f, UiGlyph_Padlock, UiCol::Ink3);
        for (int i = 0; i < look.dots; i++)
            dl->AddCircleFilled(face + ImVec2(12.f + 7.f * (float)i, size.y - 12.f), 2.f, look.dot);
    }

    for (int i = 0; i < (int)desc.marks.size(); i++) {
        const ImVec2 c = face + ImVec2(size.x - 12.5f - 17.f * (float)i, 13.5f);
        dl->AddCircleFilled(c, 7.5f, desc.paused ? UiCol::Ink3 : UiCol::Ink);
        const char letter[2] = {desc.marks[i], 0};
        AddTextCentered(dl, UiFonts::Bold, 9.f, c, UiCol::White, letter);
    }
    return hit.clicked && interactive;
}

void ImGui::AddKeySwatch(ImDrawList* dl, const ImVec2& pos, UiKeyKind kind)
{
    const ImVec2 size(24.f, 21.f);
    const float radii[4] = {5.f, 5.f, 5.f, 5.f};
    const KeyLook look = GetKeyLook(kind, false);
    AddCap(dl, pos, size, look.cap, radii);
    if (kind == UiKeyKind_Block) AddHatchRounded(dl, pos, pos + size, UiCol::Block, 6.f, 1.2f, radii);
    for (int i = 0; i < look.dots; i++)
        dl->AddCircleFilled(pos + ImVec2(6.5f + 5.f * (float)i, 14.5f), 1.5f, look.dot);
}

// ---- panel rows

bool ImGui::UiToggleRow(const char* id, const char* label, bool on, float width)
{
    const ImVec2 pos = GetCursorScreenPos();
    GetWindowDrawList()->AddText(UiFonts::SemiBold, 13.f, pos + ImVec2(0.f, 5.f), UiCol::Ink, label);
    const bool clicked = UiToggle(id, ImVec2(pos.x + width - 46.f, pos.y), on);
    // the row-sized dummy goes last so the window extent is validated by an item, not by a bare cursor move
    SetCursorScreenPos(pos);
    Dummy(ImVec2(width, 36.f));
    return clicked;
}

bool ImGui::UiStepperRow(const char* id, const char* label, const char* value, int& index, int count, float width)
{
    const ImVec2 pos = GetCursorScreenPos();
    GetWindowDrawList()->AddText(UiFonts::SemiBold, 13.f, pos + ImVec2(0.f, 6.f), UiCol::Ink, label);
    const bool changed = UiStepper(id, ImVec2(pos.x + width - 112.f, pos.y), ImVec2(112.f, 28.f), value, index, count);
    SetCursorScreenPos(pos);
    Dummy(ImVec2(width, 36.f));
    return changed;
}

bool ImGui::UiListRow(const char* id, const char* label, const char* hint, bool hintAlways, bool disabled, bool bold,
                      ImU32 dotCol, bool keepOpen)
{
    const ImVec2 pos = GetCursorScreenPos();
    const float width = GetContentRegionAvail().x;
    ImFont* font = bold ? UiFonts::Bold : UiFonts::Medium;
    const float labelX = dotCol ? 18.f : 0.f;
    const bool hasHint = hint && *hint;
    const float hintW = hasHint ? UiTextWidth(UiFonts::Medium, 10.5f, hint) : 0.f;
    // what the row needs, so an auto-resizing popup grows to fit it; the hit box still spans the whole width
    const float needW = labelX + UiTextWidth(font, 13.f, label) + (hasHint ? 16.f + hintW : 0.f);

    // the row draws its own animated highlight instead of the Selectable's
    for (const ImGuiCol idx : {ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive})
        PushStyleColor(idx, ImVec4(0, 0, 0, 0));
    const ImGuiSelectableFlags flags = (keepOpen ? ImGuiSelectableFlags_NoAutoClosePopups : 0) |
                                       ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_SpanAvailWidth |
                                       (disabled ? ImGuiSelectableFlags_Disabled : 0);
    PushID(id);
    const HitBox hit = {Selectable("##row", false, flags, ImVec2(needW, 30.f)), GetItemID()};
    PopID();
    PopStyleColor(3);

    const float hoverT = hit.Anim(AnimSlot_Hover, !disabled && IsItemHovered(), 18.f);
    const float pressT = hit.Anim(AnimSlot_Press, !disabled && IsItemActive(), 28.f);

    ImDrawList* dl = GetWindowDrawList();
    if (hoverT > 0.01f)
        dl->AddRectFilled(ImVec2(pos.x - 10.f, pos.y), ImVec2(pos.x + width + 10.f, pos.y + 30.f),
                          UiWithAlpha(UiMixColor(UiCol::RowHover, UiCol::Recess, 0.5f * pressT), hoverT), 6.f);
    if (dotCol) dl->AddCircleFilled(pos + ImVec2(6.f, 15.f), 3.f, disabled ? UiWithAlpha(dotCol, 0.4f) : dotCol);
    dl->AddText(font, 13.f, ImVec2(pos.x + labelX, pos.y + 8.f), disabled ? UiCol::Ink3 : UiCol::Ink, label);
    if (hasHint) {
        const float alpha = hintAlways ? 1.f : hoverT;
        if (alpha > 0.01f)
            dl->AddText(UiFonts::Medium, 10.5f, ImVec2(pos.x + width - hintW, pos.y + 10.f),
                        UiWithAlpha(UiCol::Ink3, alpha), hint);
    }
    return hit.clicked && !disabled;
}

void ImGui::UiSectionLabel(const char* text)
{
    const ImVec2 pos = GetCursorScreenPos();
    AddSilk(GetWindowDrawList(), pos, text);
    Dummy(ImVec2(SilkWidth(text), 11.f + 6.f));
}

void ImGui::UiDivider(float width)
{
    const ImVec2 pos = GetCursorScreenPos();
    GetWindowDrawList()->AddRectFilled(ImVec2(pos.x, pos.y + 8.f), ImVec2(pos.x + width, pos.y + 9.f), UiCol::Divider);
    Dummy(ImVec2(width, 8.f + 1.f + 8.f));
}

bool ImGui::UiSearchField(const char* id, char* buf, size_t bufSize, float width, const char* hint, bool focus)
{
    constexpr float height = 34.f, textX = 34.f;
    const ImVec2 pos = GetCursorScreenPos();
    ImDrawList* dl = GetWindowDrawList();
    AddRecess(dl, pos, pos + ImVec2(width, height), 8.f);
    AddGlyph(dl, pos + ImVec2(19.5f, 17.5f), 13.f, UiGlyph_Search, UiCol::Ink3);

    // a frameless InputText after the magnifier, as tall as the field so the text sits centered
    SetCursorScreenPos(ImVec2(pos.x + textX, pos.y));
    PushFont(UiFonts::Medium, 13.f);
    PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, (height - 13.f) * 0.5f));
    for (const ImGuiCol idx : {ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive})
        PushStyleColor(idx, ImVec4(0, 0, 0, 0));
    SetNextItemWidth(width - textX - 10.f);
    if (focus) SetKeyboardFocusHere();
    PushID(id);
    const bool edited = InputTextWithHint("##field", hint, buf, bufSize);
    PopID();
    PopStyleColor(3);
    PopStyleVar();
    PopFont();
    return edited;
}
