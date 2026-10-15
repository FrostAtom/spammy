#include "ImGui.h"
#include <concepts>
#include <cstring>
#include <initializer_list>
#include <numbers>
#include <optional>

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
static ImVector<int> s_popupContentVtxStart;

namespace {
constexpr float kSilkFontSize = 9.f, kSilkTracking = 1.6f;
constexpr float kTapeFontSize = 10.f, kTapeTracking = 1.2f, kTapeHeight = 24.f;
constexpr ImVec2 kTapeTextOffset(10.f, 7.f);
constexpr float kTabHeight = 34.f, kTabTextX = 28.f, kTabPadRight = 21.f;
constexpr float kPanelRowHeight = 36.f;

enum AnimSlot {
    AnimSlot_Hover = 1,
    AnimSlot_Press,
    AnimSlot_State,
    AnimSlot_Popup,
};

ImGuiID AnimId(ImGuiID id, AnimSlot slot, int index = 0)
{
    const int key[2] = {slot, index};
    return ImHashData(key, sizeof(key), id);
}

float UiAnim(ImGuiID id, float target, float speed, float initial = FLT_MAX)
{
    float* v = s_animStorage.GetFloatRef(id, initial == FLT_MAX ? target : initial);
    *v += (target - *v) * (1.f - expf(-speed * GImGui->IO.DeltaTime));
    if (fabsf(target - *v) < 0.001f) *v = target;
    return *v;
}

struct HitBox {
    bool clicked;
    ImGuiID id;
    bool hovered, active;

    float Anim(AnimSlot slot, bool target, float speed) const
    {
        return UiAnim(AnimId(id, slot), target ? 1.f : 0.f, speed);
    }
    float HoverAnim() const { return Anim(AnimSlot_Hover, hovered, 18.f); }
    float PressAnim() const { return Anim(AnimSlot_Press, active, 28.f); }
};

HitBox LastItemHitBox(bool clicked)
{
    return {clicked, GetItemID(), IsItemHovered(), IsItemActive()};
}

HitBox PlaceInvisibleButton(const char* id, const ImVec2& pos, const ImVec2& size)
{
    SetCursorScreenPos(pos);
    const bool clicked = InvisibleButton(id, size);
    return LastItemHitBox(clicked);
}

int PushTransparentColors(std::initializer_list<ImGuiCol> cols)
{
    for (const ImGuiCol idx : cols)
        PushStyleColor(idx, ImVec4(0, 0, 0, 0));
    return (int)cols.size();
}

struct AsciiUpper {
    char buf[256];

    explicit AsciiUpper(const char* text)
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

template <std::invocable<const char*, const char*, float> Fn>
float ForEachTrackedGlyph(ImFont* font, float size, const char* text, float tracking, float startX, Fn&& fn)
{
    ImFontBaked* baked = font->GetFontBaked(size);
    const char* end = text + strlen(text);
    float penX = startX;
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

// Archivo's capitals sit centered in its line box
void AddTextCentered(ImDrawList* dl, ImFont* font, float size, const ImVec2& center, ImU32 col, const char* text,
                     float tracking = 0.f)
{
    const float width = UiTextWidth(font, size, text, tracking);
    AddTrackedText(dl, font, size, ImVec2(center.x - width * 0.5f, center.y - size * 0.5f), col, text, tracking);
}

void AddTapeText(ImDrawList* dl, const ImVec2& tapePos, ImU32 col, const char* text)
{
    AddTrackedText(dl, UiFonts::Bold, kTapeFontSize, tapePos + kTapeTextOffset, col, AsciiUpper(text), kTapeTracking);
}

void PathRoundRect(ImDrawList* dl, const ImVec2& min, const ImVec2& max, const float radii[UiCorner_COUNT])
{
    const float limit = ImMax(0.f, ImMin(max.x - min.x, max.y - min.y) * 0.5f);
    auto corner = [&](float r, const ImVec2& tip, const ImVec2& center, float angle) {
        if (r < 0.5f)
            dl->PathLineTo(tip);
        else
            dl->PathArcTo(center, r, angle, angle + IM_PI * 0.5f);
    };
    const float tl = ImClamp(radii[UiCorner_TopLeft], 0.f, limit);
    const float tr = ImClamp(radii[UiCorner_TopRight], 0.f, limit);
    const float br = ImClamp(radii[UiCorner_BottomRight], 0.f, limit);
    const float bl = ImClamp(radii[UiCorner_BottomLeft], 0.f, limit);
    corner(tl, min, ImVec2(min.x + tl, min.y + tl), IM_PI);
    corner(tr, ImVec2(max.x, min.y), ImVec2(max.x - tr, min.y + tr), IM_PI * 1.5f);
    corner(br, max, ImVec2(max.x - br, max.y - br), 0.f);
    corner(bl, ImVec2(min.x, max.y), ImVec2(min.x + bl, max.y - bl), IM_PI * 0.5f);
}

void AddRoundRectFilled(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 col,
                        const float radii[UiCorner_COUNT])
{
    if ((col & IM_COL32_A_MASK) == 0 || max.x <= min.x || max.y <= min.y) return;
    PathRoundRect(dl, min, max, radii);
    dl->PathFillConvex(col);
}

void AddRoundRect(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 col, const float radii[UiCorner_COUNT],
                  float thickness)
{
    if ((col & IM_COL32_A_MASK) == 0 || max.x <= min.x || max.y <= min.y) return;
    constexpr float kToPixelCenter = 0.5f;
    float pixelCenterRadii[UiCorner_COUNT];
    for (int i = 0; i < UiCorner_COUNT; i++)
        pixelCenterRadii[i] = radii[i] - kToPixelCenter;
    const ImVec2 inset(kToPixelCenter, kToPixelCenter);
    PathRoundRect(dl, min + inset, max - inset, pixelCenterRadii);
    dl->PathStroke(col, ImDrawFlags_Closed, thickness);
}

struct CapColors {
    ImU32 face, lip, stroke;
};

CapColors CapPalette(UiCapKind kind)
{
    switch (kind) {
    case UiCapKind_Signal: return {UiCol.Signal, UiCol.SignalLip, 0};
    case UiCapKind_Tap: return {UiCol.Tap, UiCol.TapLip, 0};
    case UiCapKind_Same: return {UiCol.Same, UiCol.SameLip, 0};
    case UiCapKind_Locked: return {UiCol.CapLocked, UiCol.CapLip, 0};
    case UiCapKind_PausedAuto: return {UiCol.PausedAuto, UiCol.PausedAutoLip, 0};
    case UiCapKind_PausedTap: return {UiCol.PausedTap, UiCol.PausedTapLip, 0};
    default: return {UiCol.Cap, UiCol.CapLip, UiCol.CapStroke};
    }
}

ImU32 CapInk(UiCapKind kind)
{
    switch (kind) {
    case UiCapKind_Signal: return UiCol.White;
    case UiCapKind_Tap: return UiCol.InkInverse;
    case UiCapKind_Same: return UiCol.SameInk;
    case UiCapKind_PausedAuto: return UiCol.PausedAutoInk;
    case UiCapKind_PausedTap: return UiCol.TapeInk;
    default: return UiCol.Ink;
    }
}

ImVec2 DrawCap(ImDrawList* dl, const ImVec2& min, const ImVec2& size, const float radii[UiCorner_COUNT],
               const CapColors& colors, float pressT)
{
    const ImVec2 lip = min + ImVec2(0.f, kUiCapLipDepth);
    const ImVec2 face = min + ImVec2(0.f, kUiCapPressDepth * pressT);
    AddRoundRectFilled(dl, lip, lip + size, colors.lip, radii);
    AddRoundRectFilled(dl, face, face + size, colors.face, radii);
    if (colors.stroke) AddRoundRect(dl, face, face + size, colors.stroke, radii, 1.f);
    return face;
}

void AddHatchRounded(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 col, float spacing, float thickness,
                     const float radii[UiCorner_COUNT])
{
    const float halfStroke = thickness * 0.5f;
    const ImVec2 a = min + ImVec2(halfStroke, halfStroke), b = max - ImVec2(halfStroke, halfStroke);
    if (b.x <= a.x || b.y <= a.y || spacing <= 0.f) return;
    const ImVec2 mid = (a + b) * 0.5f;
    const float limit = ImMin(b.x - a.x, b.y - a.y) * 0.5f;
    float r[UiCorner_COUNT];
    for (int i = 0; i < UiCorner_COUNT; i++)
        r[i] = ImClamp(radii[i] - halfStroke, 0.f, limit);

    auto clipIntoCornerArc = [&](const ImVec2& end, const ImVec2& toward) -> std::optional<ImVec2> {
        const bool left = end.x < mid.x, top = end.y < mid.y;
        const UiCorner corner =
            top ? (left ? UiCorner_TopLeft : UiCorner_TopRight) : (left ? UiCorner_BottomLeft : UiCorner_BottomRight);
        const float rad = r[corner];
        if (rad <= 0.f) return end;
        const ImVec2 c(left ? a.x + rad : b.x - rad, top ? a.y + rad : b.y - rad);
        const bool inCornerSquare = (left ? end.x < c.x : end.x > c.x) && (top ? end.y < c.y : end.y > c.y);
        if (!inCornerSquare || ImLengthSqr(end - c) <= rad * rad) return end;
        const ImVec2 d = toward - end, f = end - c;
        const float qa = ImLengthSqr(d), qb = 2.f * (f.x * d.x + f.y * d.y), qc = ImLengthSqr(f) - rad * rad;
        const float disc = qb * qb - 4.f * qa * qc;
        if (qa <= 0.f || disc < 0.f) return std::nullopt;
        const float nearestHitT = (-qb - sqrtf(disc)) / (2.f * qa);
        if (nearestHitT < 0.f || nearestHitT > 1.f) return std::nullopt;
        return end + d * nearestHitT;
    };

    const float xPlusYStep = spacing * std::numbers::sqrt2_v<float>;
    for (float xPlusY = a.x + a.y + xPlusYStep * 0.5f; xPlusY < b.x + b.y; xPlusY += xPlusYStep) {
        const float x0 = ImMax(a.x, xPlusY - b.y), x1 = ImMin(b.x, xPlusY - a.y);
        if (x1 - x0 < 0.5f) continue;
        const std::optional<ImVec2> p0 = clipIntoCornerArc(ImVec2(x0, xPlusY - x0), ImVec2(x1, xPlusY - x1));
        if (!p0) continue;
        const std::optional<ImVec2> p1 = clipIntoCornerArc(ImVec2(x1, xPlusY - x1), *p0);
        if (!p1) continue;
        dl->AddLine(*p0, *p1, col, thickness);
    }
}

float GlyphSize(UiGlyph glyph)
{
    switch (glyph) {
    case UiGlyph_Gear:
    case UiGlyph_Padlock:
    case UiGlyph_Folder: return 14.f;
    case UiGlyph_Search: return 13.f;
    case UiGlyph_ExternalLink: return 9.f;
    case UiGlyph_Power: return 28.f;
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
    const ImU32 pausedAutoDot = UiWithAlpha(UiCol.PausedAutoInk, 0.9f);
    const ImU32 pausedTapDot = UiWithAlpha(UiCol.TapeInk, 0.9f);
    const ImU32 whiteDot = UiWithAlpha(UiCol.White, 0.9f);
    const ImU32 tapDot = UiWithAlpha(UiCol.InkInverse, 0.9f);
    const ImU32 sameDot = UiWithAlpha(UiCol.SameInk, 0.7f);
    switch (kind) {
    case UiKeyKind_Auto:
        if (paused) return {UiCapKind_PausedAuto, UiFonts::Bold, UiCol.PausedAutoInk, pausedAutoDot, 3};
        return {UiCapKind_Signal, UiFonts::Bold, UiCol.White, whiteDot, 3};
    case UiKeyKind_Tap:
        if (paused) return {UiCapKind_PausedTap, UiFonts::Bold, UiCol.TapeInk, pausedTapDot, 1};
        return {UiCapKind_Tap, UiFonts::Bold, UiCol.InkInverse, tapDot, 1};
    case UiKeyKind_SameAuto:
        if (paused) return {UiCapKind_PausedAuto, UiFonts::SemiBold, UiCol.PausedAutoInk, pausedAutoDot, 3};
        return {UiCapKind_Same, UiFonts::SemiBold, UiCol.SameInk, sameDot, 3};
    case UiKeyKind_SameTap:
        if (paused) return {UiCapKind_PausedTap, UiFonts::SemiBold, UiCol.TapeInk, pausedTapDot, 1};
        return {UiCapKind_Same, UiFonts::SemiBold, UiCol.SameInk, sameDot, 1};
    case UiKeyKind_Block: return {UiCapKind_Plain, UiFonts::Bold, UiCol.Block, 0, 0};
    case UiKeyKind_Locked: return {UiCapKind_Locked, UiFonts::Medium, UiCol.Ink3, 0, 0};
    default: return {UiCapKind_Plain, UiFonts::Medium, UiCol.Ink2, 0, 0};
    }
}

float RoundRectDistance(const ImVec2& p, const ImVec2& min, const ImVec2& max, float r)
{
    const ImVec2 c = (min + max) * 0.5f, half = (max - min) * 0.5f;
    const float qx = fabsf(p.x - c.x) - (half.x - r), qy = fabsf(p.y - c.y) - (half.y - r);
    const float ox = ImMax(qx, 0.f), oy = ImMax(qy, 0.f);
    return sqrtf(ox * ox + oy * oy) + ImMin(ImMax(qx, qy), 0.f) - r;
}

void AddOuterBoxShadow(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float rounding, const ImVec2& offset,
                       float blur, ImU32 col)
{
    constexpr int kCornerSegs = 8, kPoints = (kCornerSegs + 1) * 4, kRings = 10;
    constexpr float kRingsBunchingAtEdge = 1.5f;
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
        const float t = powf((float)i / kRings, kRingsBunchingAtEdge);
        for (int j = 0; j < kPoints; j++) {
            const ImVec2 d = dirs[j], at = centers[j / (kCornerSegs + 1)];
            const ImVec2 p(at.x + d.x * (rounding + t * reachX),
                           at.y + d.y * (rounding + t * (d.y > 0.f ? reachDown : reachUp)));
            const float dist = RoundRectDistance(p, shadowMin, shadowMax, rounding);
            const float k = i == kRings ? 0.f : 0.5f * std::erfc(dist / (sigma * std::numbers::sqrt2_v<float>));
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
    const ImGuiID animId = AnimId(GetID(str_id), AnimSlot_Popup);
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

    PushStyleVar(ImGuiStyleVar_Alpha, eased);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kUiPopupPad, kUiPopupPad));
    // modals take the window rounding and border, popups the popup ones
    PushStyleVar(ImGuiStyleVar_WindowRounding, 14.f);
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
    PushStyleColor(ImGuiCol_WindowBg, UiCol.Panel);
    const bool open =
        modal ? BeginPopupModal(str_id, nullptr,
                                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)
              : BeginPopup(str_id);
    if (!open) {
        PopStyleColor();
        PopStyleVar(4);
        return false;
    }
    const ImGuiWindow* window = GetCurrentWindow();
    AddOuterBoxShadow(window->DrawList, window->Pos, window->Pos + window->Size, window->WindowRounding,
                      ImVec2(0.f, 14.f), 36.f, UiWithAlpha(UiCol.Shadow, GetStyle().Alpha));
    s_popupContentVtxStart.push_back(window->DrawList->VtxBuffer.Size);
    return true;
}

void EndPanelRow(const ImVec2& rowPos, float width)
{
    SetCursorScreenPos(rowPos);
    Dummy(ImVec2(width, kPanelRowHeight));
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
    // ImGui sizes a font by ascent - descent, not by em
    constexpr float kArchivoAscentMinusDescentEm = 1.088f;
    cfg.ExtraSizeScale = kArchivoAscentMinusDescentEm;

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

void ImGui::LoadUiStyle(bool dark)
{
    UiCol = dark ? kUiDarkPalette : kUiLightPalette;
    ImGuiStyle& style = GetStyle();
    if (dark)
        StyleColorsDark(&style);
    else
        StyleColorsLight(&style);
    style.WindowPadding = ImVec2(0.f, 0.f);
    style.FramePadding = ImVec2(10.f, 6.f);
    style.ItemSpacing = ImVec2(8.f, 0.f);
    style.ItemInnerSpacing = ImVec2(6.f, 6.f);
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

    const struct {
        ImGuiCol idx;
        ImU32 col;
    } colors[] = {
        {ImGuiCol_Text, UiCol.Ink},
        {ImGuiCol_TextDisabled, UiCol.Ink3},
        {ImGuiCol_WindowBg, UiCol.Chassis},
        {ImGuiCol_ChildBg, 0},
        {ImGuiCol_PopupBg, UiCol.Panel},
        {ImGuiCol_Border, UiCol.PanelStroke},
        {ImGuiCol_BorderShadow, 0},
        {ImGuiCol_FrameBg, UiCol.Recess},
        {ImGuiCol_FrameBgHovered, UiCol.RecessHover},
        {ImGuiCol_FrameBgActive, UiCol.RecessStroke},
        {ImGuiCol_ScrollbarBg, 0},
        {ImGuiCol_ScrollbarGrab, UiCol.CapLip},
        {ImGuiCol_ScrollbarGrabHovered, UiCol.Ink3},
        {ImGuiCol_ScrollbarGrabActive, UiCol.Ink2},
        {ImGuiCol_Button, UiCol.Cap},
        {ImGuiCol_ButtonHovered, UiCol.RowHover},
        {ImGuiCol_ButtonActive, UiCol.Recess},
        {ImGuiCol_Header, UiCol.RowHover},
        {ImGuiCol_HeaderHovered, UiCol.RowHover},
        {ImGuiCol_HeaderActive, UiCol.Recess},
        {ImGuiCol_Separator, UiCol.Divider},
        {ImGuiCol_SeparatorHovered, UiCol.Line},
        {ImGuiCol_SeparatorActive, UiCol.Ink3},
        {ImGuiCol_InputTextCursor, UiCol.Ink},
        {ImGuiCol_TextSelectedBg, UiWithAlpha(UiCol.Signal, 0.3f)},
        {ImGuiCol_NavCursor, UiWithAlpha(UiCol.Signal, 0.6f)},
        {ImGuiCol_ModalWindowDimBg, UiWithAlpha(UiCol.Chassis, 0.6f)},
    };
    for (const auto& [idx, col] : colors)
        style.Colors[idx] = ColorConvertU32ToFloat4(col);
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

bool ImGui::UiBeginModal(const char* str_id)
{
    return BeginPopupAnimated(str_id, true);
}

void ImGui::UiEndPopup()
{
    if (!s_popupContentVtxStart.empty()) {
        const float styleAlpha = GetStyle().Alpha;
        if (styleAlpha < 1.f) {
            ImVector<ImDrawVert>& vtx = GetWindowDrawList()->VtxBuffer;
            for (int i = s_popupContentVtxStart.back(); i < vtx.Size; i++)
                vtx[i].col = UiWithAlpha(vtx[i].col, styleAlpha);
        }
        s_popupContentVtxStart.pop_back();
    }
    EndPopup();
    PopStyleColor();
    PopStyleVar(4);
}

float ImGui::UiTextWidth(ImFont* font, float size, const char* text, float tracking)
{
    if (tracking == 0.f) return font->CalcTextSizeA(size, FLT_MAX, 0.f, text).x;
    const float widthWithTrailingTracking =
        ForEachTrackedGlyph(font, size, text, tracking, 0.f, [](const char*, const char*, float) {});
    return widthWithTrailingTracking > 0.f ? widthWithTrailingTracking - tracking : 0.f;
}

void ImGui::AddTrackedText(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 col, const char* text,
                           float tracking)
{
    if (tracking == 0.f) {
        dl->AddText(font, size, pos, col, text);
        return;
    }
    ForEachTrackedGlyph(font, size, text, tracking, pos.x, [&](const char* glyphBegin, const char* glyphEnd, float x) {
        dl->AddText(font, size, ImVec2(x, pos.y), col, glyphBegin, glyphEnd);
    });
}

void ImGui::AddSilk(ImDrawList* dl, const ImVec2& topLeft, const char* text, ImU32 col)
{
    AddTrackedText(dl, UiFonts::SemiBold, kSilkFontSize, topLeft, col, AsciiUpper(text), kSilkTracking);
}

float ImGui::SilkWidth(const char* text)
{
    return UiTextWidth(UiFonts::SemiBold, kSilkFontSize, AsciiUpper(text), kSilkTracking);
}

ImVec2 ImGui::AddCap(ImDrawList* dl, const ImVec2& min, const ImVec2& size, UiCapKind kind, float rounding,
                     float pressT, float hoverT)
{
    const float radii[UiCorner_COUNT] = {rounding, rounding, rounding, rounding};
    return AddCap(dl, min, size, kind, radii, pressT, hoverT);
}

ImVec2 ImGui::AddCap(ImDrawList* dl, const ImVec2& min, const ImVec2& size, UiCapKind kind,
                     const float radii[UiCorner_COUNT], float pressT, float hoverT)
{
    CapColors colors = CapPalette(kind);
    const bool outlineHover = kind == UiCapKind_Plain;
    if (!outlineHover && kind != UiCapKind_Locked) colors.face = UiMixColor(colors.face, UiCol.White, 0.1f * hoverT);
    const ImVec2 face = DrawCap(dl, min, size, radii, colors, pressT);
    if (outlineHover && hoverT > 0.01f)
        AddRoundRect(dl, face, face + size, UiWithAlpha(UiCol.Ink2, hoverT), radii, 1.5f);
    return face;
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
        const float px = size / GlyphSize(UiGlyph_Search), t = 1.6f * px;
        const ImVec2 ringCenter = c - ImVec2(px, px);
        dl->AddCircle(ringCenter, 5.5f * px - t * 0.5f, col, 0, t);
        dl->AddLine(c + ImVec2(2.6f, 2.6f) * px, c + ImVec2(6.5f, 6.5f) * px, col, t);
        break;
    }
    case UiGlyph_ExternalLink: {
        const float t = ImMax(1.2f, size * 0.17f);
        dl->AddLine(c + ImVec2(-h, h), c + ImVec2(h, -h), col, t);
        dl->PathLineTo(ImVec2(c.x - h * 0.3f, c.y - h));
        dl->PathLineTo(ImVec2(c.x + h, c.y - h));
        dl->PathLineTo(ImVec2(c.x + h, c.y + h * 0.3f));
        dl->PathStroke(col, 0, t);
        break;
    }
    case UiGlyph_Power: {
        const float t = size * (3.f / 28.f), r = h - t * 0.5f, halfGap = 0.62f;
        auto addRoundCap = [&](const ImVec2& p) { dl->AddCircleFilled(p, t * 0.5f, col); };
        const float a0 = -IM_PI * 0.5f + halfGap, a1 = IM_PI * 1.5f - halfGap;
        dl->PathArcTo(c, r, a0, a1);
        dl->PathStroke(col, 0, t);
        addRoundCap(c + ImVec2(cosf(a0), sinf(a0)) * r);
        addRoundCap(c + ImVec2(cosf(a1), sinf(a1)) * r);
        const ImVec2 top(c.x, c.y - h), bottom(c.x, c.y - h * 0.08f);
        dl->AddLine(top, bottom, col, t);
        addRoundCap(top);
        addRoundCap(bottom);
        break;
    }
    case UiGlyph_Padlock: {
        const float t = size * 0.11f, bodyHalfW = size * 0.36f, shackleR = size * 0.22f;
        const float bodyTop = c.y - size * 0.04f, bottom = c.y + h;
        dl->AddRectFilled(ImVec2(c.x - bodyHalfW, bodyTop), ImVec2(c.x + bodyHalfW, bottom), col, size * 0.12f);
        const float shackleArcY = c.y - h + shackleR + t * 0.5f;
        dl->PathLineTo(ImVec2(c.x - shackleR, c.y));
        dl->PathArcTo(ImVec2(c.x, shackleArcY), shackleR, IM_PI, IM_PI * 2.f);
        dl->PathLineTo(ImVec2(c.x + shackleR, c.y));
        dl->PathStroke(col, 0, t);
        break;
    }
    case UiGlyph_Folder: {
        const float r = size * 0.08f, top = c.y - size * 0.4f, bodyTop = c.y - size * 0.24f, bottom = c.y + size * 0.4f;
        dl->AddRectFilled(ImVec2(c.x - h, top), ImVec2(c.x - h * 0.1f, bodyTop + r), col, r);
        dl->AddRectFilled(ImVec2(c.x - h, bodyTop), ImVec2(c.x + h, bottom), col, r);
        break;
    }
    }
}

void ImGui::AddRecess(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float rounding)
{
    dl->AddRectFilled(min, max, UiCol.Recess, rounding);
    dl->AddRect(min, max, UiCol.RecessStroke, rounding);
}

void ImGui::AddRateBadge(ImDrawList* dl, const ImVec2& pointerTip, const char* text, float alpha, float textScale)
{
    if (alpha <= 0.f) return;
    constexpr float kPointerHalfW = 5.f, kPointerH = 5.f, kSeamOverlap = 0.5f;
    const float width = ImMax(60.f, UiTextWidth(UiFonts::Bold, 11.f, text) + 20.f);
    const ImVec2 max(pointerTip.x + width * 0.5f, pointerTip.y - kPointerH);
    const ImVec2 min(pointerTip.x - width * 0.5f, max.y - 22.f);
    const ImU32 fill = UiWithAlpha(UiCol.Ink, alpha);
    dl->AddRectFilled(min, max, fill, 5.f);
    const float pointerBaseY = max.y - kSeamOverlap;
    // clockwise, or ImGui's anti-aliasing fringe goes inward
    dl->AddTriangleFilled(ImVec2(pointerTip.x - kPointerHalfW, pointerBaseY),
                          ImVec2(pointerTip.x + kPointerHalfW, pointerBaseY), pointerTip, fill);
    AddTextCentered(dl, UiFonts::Bold, 11.f * textScale, (min + max) * 0.5f, UiWithAlpha(UiCol.InkInverse, alpha), text);
}

float ImGui::TapeWidth(const char* text)
{
    return UiTextWidth(UiFonts::Bold, kTapeFontSize, AsciiUpper(text), kTapeTracking) + kTapeTextOffset.x * 2.f;
}

void ImGui::AddTape(ImDrawList* dl, const ImVec2& pos, const char* text)
{
    dl->AddRectFilled(pos, pos + ImVec2(TapeWidth(text), kTapeHeight), UiCol.Tape, 2.f);
    AddTapeText(dl, pos, UiCol.TapeInk, text);
}

UiCapHit ImGui::UiCapFrame(const char* id, const ImVec2& pos, const ImVec2& size, UiCapKind kind, float rounding)
{
    const HitBox hit = PlaceInvisibleButton(id, pos, size + ImVec2(0.f, kUiCapLipDepth));
    UiCapHit out;
    out.clicked = hit.clicked;
    out.hoverT = hit.HoverAnim();
    out.pressT = hit.PressAnim();
    out.face = AddCap(GetWindowDrawList(), pos, size, kind, rounding, out.pressT, out.hoverT);
    return out;
}

bool ImGui::UiButton(const char* id, const ImVec2& pos, const ImVec2& size, const char* label, UiButtonKind kind)
{
    const UiCapHit hit =
        UiCapFrame(id, pos, size, kind == UiButtonKind_Primary ? UiCapKind_Signal : UiCapKind_Plain, 8.f);
    const ImU32 col = kind == UiButtonKind_Primary  ? UiCol.White
                      : kind == UiButtonKind_Danger ? UiCol.Block
                                                    : UiCol.Ink;
    AddTextCentered(GetWindowDrawList(), UiFonts::Bold, 13.f, hit.face + size * 0.5f, col, label);
    return hit.clicked;
}

bool ImGui::UiCapIconButton(const char* id, const ImVec2& pos, float size, UiGlyph glyph)
{
    const UiCapHit hit = UiCapFrame(id, pos, ImVec2(size, size), UiCapKind_Plain, 6.f);
    AddGlyph(GetWindowDrawList(), hit.face + ImVec2(size, size) * 0.5f, GlyphSize(glyph) * size / 30.f, glyph,
             UiMixColor(UiCol.Ink2, UiCol.Ink, hit.hoverT));
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
        dl->AddRectFilled(pos, pos + box, UiWithAlpha(UiMixColor(UiCol.Recess, UiCol.RecessStroke, pressT), hoverT),
                          8.f);
    const ImU32 col = UiMixColor(UiCol.Ink2, glyph == UiGlyph_Close ? UiCol.Block : UiCol.Ink, hoverT);
    AddGlyph(dl, pos + box * 0.5f, GlyphSize(glyph) * size / 30.f * (1.f - 0.1f * pressT), glyph, col);
    return hit.clicked;
}

bool ImGui::UiOutlineButton(const char* id, const ImVec2& pos, const char* label)
{
    const ImVec2 size(TapeWidth(label), kTapeHeight);
    const HitBox hit = PlaceInvisibleButton(id, pos, size);
    const float hoverT = hit.HoverAnim();
    const float pressT = hit.PressAnim();

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = pos + size;
    if (hoverT > 0.01f)
        dl->AddRectFilled(pos, max, UiWithAlpha(UiMixColor(UiCol.Recess, UiCol.RecessStroke, pressT), hoverT), 2.f);
    dl->AddRect(pos, max, UiCol.Ink3, 2.f);
    AddTapeText(dl, pos, UiMixColor(UiCol.Ink2, UiCol.Ink, hoverT), label);
    return hit.clicked;
}

bool ImGui::UiBrowseButton(const char* id, const ImVec2& pos, const ImVec2& size, const char* label, const char* hint,
                           ImU32 hintCol)
{
    constexpr float kPadX = 16.f, kGlyphSize = 14.f, kGlyphGap = 10.f;
    const UiCapHit hit = UiCapFrame(id, pos, size, UiCapKind_Plain, 8.f);
    ImDrawList* dl = GetWindowDrawList();
    const float centerY = hit.face.y + size.y * 0.5f;
    AddGlyph(dl, ImVec2(hit.face.x + kPadX + kGlyphSize * 0.5f, centerY), kGlyphSize, UiGlyph_Folder,
             UiMixColor(UiCol.Ink2, UiCol.Signal, hit.hoverT));
    auto topForCenteredY = [centerY](ImFont* font, float fontSize, const char* text) {
        return ImFloor(centerY - font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, text).y * 0.5f);
    };
    const float labelX = hit.face.x + kPadX + kGlyphSize + kGlyphGap;
    dl->AddText(UiFonts::Bold, 13.f, ImVec2(labelX, topForCenteredY(UiFonts::Bold, 13.f, label)), UiCol.Ink, label);
    if (hint && *hint) {
        const float hintX = hit.face.x + size.x - kPadX - UiTextWidth(UiFonts::Medium, 12.f, hint);
        dl->AddText(UiFonts::Medium, 12.f, ImVec2(hintX, topForCenteredY(UiFonts::Medium, 12.f, hint)), hintCol, hint);
    }
    return hit.clicked;
}

bool ImGui::UiToggle(const char* id, const ImVec2& pos, bool on)
{
    constexpr float kTrackR = kUiToggleSize.y * 0.5f, kKnobR = 10.f, kKnobTravel = 20.f, kKnobLipDepth = 2.f;
    const HitBox hit = PlaceInvisibleButton(id, pos, kUiToggleSize);
    const float hoverT = hit.HoverAnim();
    const float t = hit.Anim(AnimSlot_State, on, 16.f);
    const float eased = t * t * (3.f - 2.f * t);
    const float offT = 1.f - eased;

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = pos + kUiToggleSize;
    dl->AddRectFilled(pos, max, UiCol.Recess, kTrackR);
    if (offT > 0.f)
        dl->AddRect(pos, max, UiWithAlpha(UiMixColor(UiCol.RecessStroke, UiCol.Ink3, 0.6f * hoverT), offT), kTrackR);
    if (eased > 0.f)
        dl->AddRectFilled(pos, max, UiWithAlpha(UiMixColor(UiCol.Signal, UiCol.White, 0.08f * hoverT), eased),
                          kTrackR);

    const ImVec2 knob(pos.x + kTrackR + kKnobTravel * eased, pos.y + kTrackR);
    if (offT > 0.f) dl->AddCircleFilled(knob + ImVec2(0.f, kKnobLipDepth), kKnobR, UiWithAlpha(UiCol.CapLip, offT));
    dl->AddCircleFilled(knob, kKnobR, UiMixColor(UiCol.Cap, UiCol.White, eased));
    if (offT > 0.f) dl->AddCircle(knob, kKnobR - 0.5f, UiWithAlpha(UiCol.CapStroke, offT));
    return hit.clicked;
}

float ImGui::TabWidth(const char* label)
{
    return kTabTextX + UiTextWidth(UiFonts::Bold, 13.f, label) + kTabPadRight;
}

bool ImGui::UiTab(const char* id, const ImVec2& pos, const char* label, bool selected, bool lit, bool dim)
{
    const ImVec2 size(TabWidth(label), kTabHeight);
    const HitBox hit = PlaceInvisibleButton(id, pos, size + ImVec2(0.f, selected ? kUiCapLipDepth : 0.f));
    const float hoverT = hit.HoverAnim();
    const float pressT = hit.Anim(AnimSlot_Press, selected && hit.active, 28.f);

    ImDrawList* dl = GetWindowDrawList();
    ImVec2 face = pos;
    if (selected) {
        face = AddCap(dl, pos, size, UiCapKind_Plain, 8.f, pressT);
    } else {
        dl->AddRectFilled(pos, pos + size, UiMixColor(UiCol.Recess, UiCol.RecessStroke, 0.3f * hoverT), 8.f);
        dl->AddRect(pos, pos + size, UiCol.RecessStroke, 8.f);
    }

    const ImU32 dot = !lit ? UiCol.DotOff : dim ? UiCol.Ink3 : UiCol.Signal;
    dl->AddCircleFilled(face + ImVec2(16.f, 17.f), 3.f, dot);
    dl->AddText(selected ? UiFonts::Bold : UiFonts::SemiBold, 13.f, face + ImVec2(kTabTextX, 9.f),
                selected ? UiCol.Ink : UiMixColor(UiCol.Ink2, UiCol.Ink, hoverT), label);
    return hit.clicked;
}

bool ImGui::UiSegmented(const char* id, const ImVec2& pos, const ImVec2& size, std::span<const char* const> labels,
                        int& index, UiLabelCase labelCase, std::span<const UiCapKind> chosenKinds)
{
    IM_ASSERT(chosenKinds.empty() || chosenKinds.size() == labels.size());
    const int count = (int)labels.size();
    const HitBox hit = PlaceInvisibleButton(id, pos, size);
    if (count == 0) return false;
    const float segW = size.x / (float)count;
    const int hovered = hit.hovered ? ImClamp((int)((GetMousePos().x - pos.x) / segW), 0, count - 1) : -1;

    bool changed = false;
    if (hit.clicked && hovered >= 0 && hovered != index) {
        index = hovered;
        changed = true;
    }
    const int chosen = ImClamp(index, 0, count - 1);
    const float slide = UiAnim(AnimId(hit.id, AnimSlot_State), (float)chosen, 20.f);
    const float pressT = hit.Anim(AnimSlot_Press, hit.active && hovered == chosen, 28.f);

    ImDrawList* dl = GetWindowDrawList();
    AddRecess(dl, pos, pos + size, size.y <= 32.f ? 8.f : 9.f);
    const UiCapKind kind = chosenKinds.empty() ? UiCapKind_Plain : chosenKinds[chosen];
    const float capH = size.y - 9.f;
    AddCap(dl, ImVec2(pos.x + slide * segW + 3.f, pos.y + 3.f), ImVec2(segW - 6.f, capH), kind, 6.f, pressT);

    const bool uppercase = labelCase == UiLabelCase_Upper;
    const float textSize = uppercase ? 11.f : 12.f, tracking = uppercase ? 0.6f : 0.f;
    const float centerY = pos.y + 3.f + capH * 0.5f;
    for (int i = 0; i < count; i++) {
        const AsciiUpper upper(labels[i]);
        const char* text = uppercase ? (const char*)upper : labels[i];
        const float hoverT = UiAnim(AnimId(hit.id, AnimSlot_Hover, i), hovered == i ? 1.f : 0.f, 18.f);
        const float capUnderText = ImSaturate(1.f - fabsf(slide - (float)i));
        const ImU32 col = UiMixColor(UiMixColor(UiCol.Ink2, UiCol.Ink, hoverT), CapInk(kind), capUnderText);
        const bool isChosen = i == chosen;
        AddTextCentered(dl, isChosen ? UiFonts::Bold : UiFonts::SemiBold, textSize,
                        ImVec2(pos.x + segW * ((float)i + 0.5f), centerY + (isChosen ? 3.f * pressT : 0.f)), col, text,
                        tracking);
    }
    return changed;
}

bool ImGui::UiSegmentedFlags(const char* id, const ImVec2& pos, const ImVec2& size, const char* noFlagsLabel,
                             std::span<const UiFlagSegment> flagSegments, unsigned& flags, UiLabelCase labelCase)
{
    boost::container::small_vector<UiFlagSegment, 8> segments = {{noFlagsLabel, 0u}};
    segments.insert(segments.end(), flagSegments.begin(), flagSegments.end());
    const int count = (int)segments.size();
    const HitBox hit = PlaceInvisibleButton(id, pos, size);
    const float segW = size.x / (float)count;
    const int hovered = hit.hovered ? ImClamp((int)((GetMousePos().x - pos.x) / segW), 0, count - 1) : -1;

    const unsigned before = flags;
    if (hit.clicked && hovered >= 0) {
        const unsigned clickedFlag = segments[hovered].flag;
        flags = clickedFlag ? flags ^ clickedFlag : 0u;
    }

    ImDrawList* dl = GetWindowDrawList();
    AddRecess(dl, pos, pos + size, size.y <= 32.f ? 8.f : 9.f);
    const float capH = size.y - 9.f;
    const bool uppercase = labelCase == UiLabelCase_Upper;
    const float textSize = uppercase ? 11.f : 12.f, tracking = uppercase ? 0.6f : 0.f;
    const float centerY = pos.y + 3.f + capH * 0.5f;
    for (int i = 0; i < count; i++) {
        const UiFlagSegment& segment = segments[i];
        const bool lit = segment.flag ? (flags & segment.flag) != 0 : flags == 0;
        const float litT = UiAnim(AnimId(hit.id, AnimSlot_State, i), lit ? 1.f : 0.f, 20.f);
        const float hoverT = UiAnim(AnimId(hit.id, AnimSlot_Hover, i), hovered == i ? 1.f : 0.f, 18.f);
        const float pressT = UiAnim(AnimId(hit.id, AnimSlot_Press, i), hit.active && hovered == i ? 1.f : 0.f, 28.f);
        const float segX = pos.x + segW * (float)i;
        if (lit) AddCap(dl, ImVec2(segX + 3.f, pos.y + 3.f), ImVec2(segW - 6.f, capH), UiCapKind_Plain, 6.f, pressT);

        const AsciiUpper upper(segment.label);
        const char* text = uppercase ? (const char*)upper : segment.label;
        const ImU32 col = UiMixColor(UiMixColor(UiCol.Ink2, UiCol.Ink, hoverT), CapInk(UiCapKind_Plain), litT);
        AddTextCentered(dl, lit ? UiFonts::Bold : UiFonts::SemiBold, textSize,
                        ImVec2(segX + segW * 0.5f, centerY + (lit ? 3.f * pressT : 0.f)), col, text, tracking);
    }
    return flags != before;
}

bool ImGui::UiStepper(const char* id, const ImVec2& pos, const ImVec2& size, const char* value, int& index, int count)
{
    const HitBox hit = PlaceInvisibleButton(id, pos, size);
    const float midX = pos.x + size.x * 0.5f;
    const bool leftHalf = GetMousePos().x < midX;
    const int last = ImMax(count - 1, 0);

    bool changed = false;
    if (hit.clicked) {
        const int next = ImClamp(index + (leftHalf ? -1 : 1), 0, last);
        changed = next != index;
        index = next;
    }
    const float leftT = UiAnim(AnimId(hit.id, AnimSlot_Hover, 0), hit.hovered && leftHalf ? 1.f : 0.f, 18.f);
    const float rightT = UiAnim(AnimId(hit.id, AnimSlot_Hover, 1), hit.hovered && !leftHalf ? 1.f : 0.f, 18.f);

    ImDrawList* dl = GetWindowDrawList();
    AddRecess(dl, pos, pos + size, 7.f);
    const float cy = pos.y + size.y * 0.5f;
    const ImU32 leftCol = index > 0 ? UiMixColor(UiCol.Ink3, UiCol.Ink, leftT) : UiCol.DotOff;
    const ImU32 rightCol = index < last ? UiMixColor(UiCol.Ink3, UiCol.Ink, rightT) : UiCol.DotOff;
    AddTextCentered(dl, UiFonts::SemiBold, 15.f, ImVec2(pos.x + 12.f, cy), leftCol, "\xE2\x80\xB9");
    AddTextCentered(dl, UiFonts::SemiBold, 15.f, ImVec2(pos.x + size.x - 12.f, cy), rightCol, "\xE2\x80\xBA");
    AddTextCentered(dl, UiFonts::Bold, 12.f, ImVec2(midX, cy), UiCol.Ink, value);
    return changed;
}

bool ImGui::UiPowerButton(const char* id, const ImVec2& center, bool on)
{
    constexpr float r = 37.f;
    const ImVec2 min = center - ImVec2(r, r), size(r * 2.f, r * 2.f);
    const HitBox hit = PlaceInvisibleButton(id, min, size + ImVec2(0.f, kUiCapLipDepth));
    const float hoverT = hit.HoverAnim();
    const float pressT = hit.PressAnim();
    const float t = hit.Anim(AnimSlot_State, on, 12.f);

    const CapColors colors = {
        UiMixColor(UiMixColor(UiCol.Cap, UiCol.Signal, t), UiCol.White, 0.1f * hoverT),
        UiMixColor(UiCol.CapLip, UiCol.SignalLip, t),
        UiWithAlpha(UiCol.CapStroke, 1.f - t),
    };
    const float radii[UiCorner_COUNT] = {r, r, r, r};
    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 face = DrawCap(dl, min, size, radii, colors, pressT);
    AddGlyph(dl, face + ImVec2(r, r), 28.f, UiGlyph_Power, UiMixColor(UiCol.Ink3, UiCol.White, t));
    return hit.clicked;
}

bool ImGui::UiKnob(const char* id, const ImVec2& center, float& t, const char* minLabel, const char* maxLabel, bool dim)
{
    constexpr float r = 36.f;
    const ImVec2 min = center - ImVec2(r, r);
    const HitBox hit = PlaceInvisibleButton(id, min, ImVec2(r * 2.f, r * 2.f + kUiCapLipDepth));
    const bool dragging = hit.active;
    const ImGuiIO& io = GetIO();

    const float before = t;
    if (dragging) t += (io.MouseDelta.x - io.MouseDelta.y) * 0.004f;
    if (hit.hovered && SetItemKeyOwner(ImGuiKey_MouseWheelY) && io.MouseWheel != 0.f) t += io.MouseWheel * 0.02f;
    t = ImClamp(t, 0.f, 1.f);
    const bool changed = t != before;

    const ImGuiID shownId = AnimId(hit.id, AnimSlot_State);
    if (dragging) s_animStorage.SetFloat(shownId, t);
    const float shown = UiAnim(shownId, t, 18.f);
    const float hoverT = UiAnim(AnimId(hit.id, AnimSlot_Hover), hit.hovered || dragging ? 1.f : 0.f, 18.f);

    ImDrawList* dl = GetWindowDrawList();
    auto dirAt = [](float f) {
        constexpr float kSweepDeg = 270.f;
        const float clockwiseFromUp = (-kSweepDeg * 0.5f + kSweepDeg * f) * (IM_PI / 180.f);
        return ImVec2(sinf(clockwiseFromUp), -cosf(clockwiseFromUp));
    };
    for (int i = 0; i <= 20; i++) {
        const float f = (float)i / 20.f;
        const bool major = i % 5 == 0;
        const ImVec2 d = dirAt(f);
        dl->AddLine(center + d * (major ? 44.f : 48.f), center + d * 52.f,
                    f <= shown + 0.001f ? UiCol.Ink2 : UiCol.Ink3, major ? 1.6f : 1.1f);
    }
    AddTextCentered(dl, UiFonts::SemiBold, 9.f, ImVec2(center.x - 44.f, center.y + 44.5f), UiCol.Ink3, minLabel);
    AddTextCentered(dl, UiFonts::SemiBold, 9.f, ImVec2(center.x + 44.f, center.y + 44.5f), UiCol.Ink3, maxLabel);

    AddCap(dl, min, ImVec2(r * 2.f, r * 2.f), UiCapKind_Plain, r, 0.f, 0.5f * hoverT);
    dl->AddCircle(center, 26.f, UiCol.KnobRing, 0, 1.5f);
    const ImVec2 d = dirAt(shown);
    const ImVec2 a = center + d * 30.f, b = center + d * 10.f;
    const ImU32 pointer = dim ? UiCol.Ink3 : UiCol.Signal;
    dl->AddLine(a, b, pointer, 4.f);
    dl->AddCircleFilled(a, 2.f, pointer);
    dl->AddCircleFilled(b, 2.f, pointer);
    return changed;
}

bool ImGui::UiKey(const char* id, const ImVec2& pos, const ImVec2& size, const UiKeyDesc& desc)
{
    const bool interactive = desc.kind != UiKeyKind_Fixed && desc.kind != UiKeyKind_Locked;
    const HitBox hit = PlaceInvisibleButton(id, pos, size + ImVec2(0.f, kUiCapLipDepth));
    const float hoverT = hit.Anim(AnimSlot_Hover, interactive && hit.hovered, 18.f);
    const float pressT = hit.Anim(AnimSlot_Press, desc.pressed || (interactive && hit.active), 30.f);

    const KeyLook look = GetKeyLook(desc.kind, desc.paused);
    const bool narrow = size.x < 30.f;
    const bool block = desc.kind == UiKeyKind_Block;
    float radii[UiCorner_COUNT] = {desc.radii[0], desc.radii[1], desc.radii[2], desc.radii[3]};
    if (radii[0] == 0.f && radii[1] == 0.f && radii[2] == 0.f && radii[3] == 0.f)
        radii[0] = radii[1] = radii[2] = radii[3] = narrow ? 5.f : 7.f;

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 face = AddCap(dl, pos, size, look.cap, radii, pressT, block ? 0.f : hoverT);
    if (block) {
        AddHatchRounded(dl, face, face + size, UiCol.Block, 7.f, 1.2f, radii);
        if (hoverT > 0.01f) AddRoundRect(dl, face, face + size, UiWithAlpha(UiCol.Ink2, hoverT), radii, 1.5f);
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
            dl->AddRectFilled(face + ImVec2(5.f, 5.f), face + ImVec2(5.f + ImMax(16.f, labelW + 8.f), 23.f), UiCol.Cap,
                              3.f);
        dl->AddText(look.font, 12.f, at, look.ink, label);
        if (desc.kind == UiKeyKind_Locked)
            AddGlyph(dl, ImVec2(at.x + labelW + (*label ? 9.f : 4.f), at.y + 4.5f), 12.f, UiGlyph_Padlock, UiCol.Ink3);
        for (int i = 0; i < look.dots; i++)
            dl->AddCircleFilled(face + ImVec2(12.f + 7.f * (float)i, size.y - 12.f), 2.f, look.dot);
    }

    for (int i = 0; i < (int)desc.marks.size(); i++) {
        const ImVec2 c = face + ImVec2(size.x - 12.5f - 17.f * (float)i, 13.5f);
        dl->AddCircleFilled(c, 7.5f, desc.paused ? UiCol.Ink3 : UiCol.Ink);
        const char letter[2] = {desc.marks[i], 0};
        AddTextCentered(dl, UiFonts::Bold, 9.f, c, UiCol.InkInverse, letter);
    }
    return hit.clicked && interactive;
}

void ImGui::AddKeySwatch(ImDrawList* dl, const ImVec2& pos, UiKeyKind kind)
{
    const ImVec2 size(24.f, 21.f);
    const float radii[UiCorner_COUNT] = {5.f, 5.f, 5.f, 5.f};
    const KeyLook look = GetKeyLook(kind, false);
    AddCap(dl, pos, size, look.cap, radii);
    if (kind == UiKeyKind_Block) AddHatchRounded(dl, pos, pos + size, UiCol.Block, 6.f, 1.2f, radii);
    for (int i = 0; i < look.dots; i++)
        dl->AddCircleFilled(pos + ImVec2(6.5f + 5.f * (float)i, 14.5f), 1.5f, look.dot);
}

bool ImGui::UiToggleRow(const char* id, const char* label, bool on, float width)
{
    const ImVec2 pos = GetCursorScreenPos();
    GetWindowDrawList()->AddText(UiFonts::SemiBold, 13.f, pos + ImVec2(0.f, 5.f), UiCol.Ink, label);
    const bool clicked = UiToggle(id, ImVec2(pos.x + width - kUiToggleSize.x, pos.y), on);
    EndPanelRow(pos, width);
    return clicked;
}

bool ImGui::UiStepperRow(const char* id, const char* label, const char* value, int& index, int count, float width)
{
    const ImVec2 pos = GetCursorScreenPos();
    GetWindowDrawList()->AddText(UiFonts::SemiBold, 13.f, pos + ImVec2(0.f, 6.f), UiCol.Ink, label);
    const bool changed = UiStepper(id, ImVec2(pos.x + width - 112.f, pos.y), ImVec2(112.f, 28.f), value, index, count);
    EndPanelRow(pos, width);
    return changed;
}

bool ImGui::UiListRow(const char* id, const char* label, const char* hint, UiListRowFlags rowFlags, ImU32 dotCol)
{
    const bool hintAlways = rowFlags & UiListRowFlags_HintAlways;
    const bool disabled = rowFlags & UiListRowFlags_Disabled;
    const bool bold = rowFlags & UiListRowFlags_Bold;
    const ImVec2 pos = GetCursorScreenPos();
    const float width = GetContentRegionAvail().x;
    ImFont* font = bold ? UiFonts::Bold : UiFonts::Medium;
    const float labelX = dotCol ? 18.f : 0.f;
    const bool hasHint = hint && *hint;
    const float hintW = hasHint ? UiTextWidth(UiFonts::Medium, 10.5f, hint) : 0.f;
    const float fitWidth = labelX + UiTextWidth(font, 13.f, label) + (hasHint ? 16.f + hintW : 0.f);

    const int hiddenHighlights =
        PushTransparentColors({ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive});
    const ImGuiSelectableFlags flags = ImGuiSelectableFlags_NoAutoClosePopups | ImGuiSelectableFlags_AllowOverlap |
                                       ImGuiSelectableFlags_SpanAvailWidth |
                                       (disabled ? ImGuiSelectableFlags_Disabled : 0);
    PushID(id);
    const HitBox hit = LastItemHitBox(Selectable("##row", false, flags, ImVec2(fitWidth, 30.f)));
    PopID();
    PopStyleColor(hiddenHighlights);

    const float hoverT = hit.Anim(AnimSlot_Hover, !disabled && hit.hovered, 18.f);
    const float pressT = hit.Anim(AnimSlot_Press, !disabled && hit.active, 28.f);

    ImDrawList* dl = GetWindowDrawList();
    if (hoverT > 0.01f)
        dl->AddRectFilled(ImVec2(pos.x - 10.f, pos.y), ImVec2(pos.x + width + 10.f, pos.y + 30.f),
                          UiWithAlpha(UiMixColor(UiCol.RowHover, UiCol.Recess, 0.5f * pressT), hoverT), 6.f);
    if (dotCol) dl->AddCircleFilled(pos + ImVec2(6.f, 15.f), 3.f, disabled ? UiWithAlpha(dotCol, 0.4f) : dotCol);
    dl->AddText(font, 13.f, ImVec2(pos.x + labelX, pos.y + 8.f), disabled ? UiCol.Ink3 : UiCol.Ink, label);
    if (hasHint) {
        const float alpha = hintAlways ? 1.f : hoverT;
        if (alpha > 0.01f)
            dl->AddText(UiFonts::Medium, 10.5f, ImVec2(pos.x + width - hintW, pos.y + 10.f),
                        UiWithAlpha(UiCol.Ink3, alpha), hint);
    }
    return hit.clicked && !disabled;
}

void ImGui::UiSectionLabel(const char* text)
{
    constexpr float kSilkLineHeight = 11.f, kGapBelow = 6.f;
    const ImVec2 pos = GetCursorScreenPos();
    AddSilk(GetWindowDrawList(), pos, text);
    Dummy(ImVec2(SilkWidth(text), kSilkLineHeight + kGapBelow));
}

void ImGui::UiDivider(float width)
{
    constexpr float kMargin = 8.f, kThickness = 1.f;
    const ImVec2 pos = GetCursorScreenPos();
    GetWindowDrawList()->AddRectFilled(ImVec2(pos.x, pos.y + kMargin),
                                       ImVec2(pos.x + width, pos.y + (kMargin + kThickness)), UiCol.Divider);
    Dummy(ImVec2(width, kMargin + kThickness + kMargin));
}

bool ImGui::UiSearchField(const char* id, char* buf, size_t bufSize, float width, const char* hint, bool focus)
{
    constexpr float kHeight = 34.f, kTextX = 34.f, kFontSize = 13.f;
    const ImVec2 pos = GetCursorScreenPos();
    ImDrawList* dl = GetWindowDrawList();
    AddRecess(dl, pos, pos + ImVec2(width, kHeight), 8.f);
    AddGlyph(dl, pos + ImVec2(19.5f, 17.5f), 13.f, UiGlyph_Search, UiCol.Ink3);

    SetCursorScreenPos(ImVec2(pos.x + kTextX, pos.y));
    PushFont(UiFonts::Medium, kFontSize);
    const float textCenteringPadY = (kHeight - kFontSize) * 0.5f;
    PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, textCenteringPadY));
    const int hiddenFrame = PushTransparentColors({ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive});
    SetNextItemWidth(width - kTextX - 10.f);
    if (focus) SetKeyboardFocusHere();
    PushID(id);
    const bool edited = InputTextWithHint("##field", hint, buf, bufSize);
    PopID();
    PopStyleColor(hiddenFrame);
    PopStyleVar();
    PopFont();
    return edited;
}
