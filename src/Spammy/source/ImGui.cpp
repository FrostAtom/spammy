#include "ImGui.h"
#include <cstring>

#include "Resources/ImFont_JetBrainsMono.inl"
#include "Resources/ImFont_RajdhaniBold.inl"
#include "Resources/ImFont_RajdhaniSemiBold.inl"

ImFont* ImGui::UiFonts::Semi = NULL;
ImFont* ImGui::UiFonts::Bold = NULL;
ImFont* ImGui::UiFonts::Mono = NULL;

static ImGuiStorage s_animStorage;

namespace {
struct KeyPalette {
    ImU32 fill, accent, text, pressed;
};
// indexed by UiKeyStyle
constexpr KeyPalette s_keyPalettes[] = {
    {0, 0, 0, 0},
    {ImGui::UiCol::SpamFill, ImGui::UiCol::Spam, ImGui::UiCol::SpamText, IM_COL32(0x3E, 0x2A, 0x1C, 0xFF)},
    {ImGui::UiCol::SpeedyFill, ImGui::UiCol::Speedy, ImGui::UiCol::SpeedyText, IM_COL32(0x16, 0x3B, 0x49, 0xFF)},
    {ImGui::UiCol::DangerFill, ImGui::UiCol::Danger, ImGui::UiCol::DangerText, IM_COL32(0x3E, 0x1F, 0x24, 0xFF)},
};

struct HitBox {
    bool clicked;
    ImGuiID id;
};

ImGuiID SubId(ImGuiID id, int n)
{
    return ImHashData(&n, sizeof(n), id);
}

HitBox PlaceInvisibleButton(const char* id, const ImVec2& pos, const ImVec2& size)
{
    ImGui::SetCursorScreenPos(pos);
    bool clicked = ImGui::InvisibleButton(id, size);
    return {clicked, ImGui::GetItemID()};
}

float ItemAnim(ImGuiID itemId, int slot, bool target, float speed)
{
    return ImGui::UiAnim(SubId(itemId, slot), target ? 1.f : 0.f, speed);
}

float Breath(float freq, float base, float amp)
{
    return base + amp * sinf((float)ImGui::GetTime() * freq);
}

void PushStyleColorTriplet(ImGuiCol idx, const ImVec4& col)
{
    for (int i = 0; i < 3; i++)
        ImGui::PushStyleColor(idx + i, col);
}

void AddStatusDot(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, bool glow)
{
    if (glow) {
        const float breath = Breath(3.f, 0.7f, 0.3f);
        for (int i = 1; i <= 4; i++) {
            const float t = 1.f - (float)i / 5.f;
            dl->AddCircle(center, radius + i, ImGui::UiWithAlpha(col, 0.35f * t * t * breath), 0, 1.5f);
        }
    }
    dl->AddCircleFilled(center, radius, col);
}

bool UiToggle(const char* id, const ImVec2& pos, bool on)
{
    using namespace ImGui;
    const ImVec2 size(38.f, 20.f);
    const HitBox hit = PlaceInvisibleButton(id, pos, size);
    const float t = ItemAnim(hit.id, 1, on, 16.f);
    const float e = t * t * (3.f - 2.f * t);

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = pos + size;
    if (e > 0.01f) AddGlow(dl, pos, max, UiCol::Spam, 10.f, 5, 0.2f * e);
    dl->AddRectFilled(pos, max, UiMixColor(UiCol::Bg0, UiCol::Spam, e), 10.f);
    if (e < 0.99f) dl->AddRect(pos, max, UiWithAlpha(UiCol::Stroke, 1.f - e), 10.f);
    dl->AddCircleFilled(ImVec2(pos.x + 10.f + 18.f * e, pos.y + 10.f), 7.f, UiMixColor(UiCol::Mute, UiCol::Bg0, e));
    return hit.clicked;
}

bool UiStepper(const char* id, const ImVec2& pos, float width, int count, int& value)
{
    using namespace ImGui;
    const float height = 20.f;
    const HitBox hit = PlaceInvisibleButton(id, pos, ImVec2(width, height));

    const float pad = height * 0.5f;
    const float span = width - pad * 2.f;
    const int last = count > 1 ? count - 1 : 1;

    bool changed = false;
    if (IsItemActive() && span > 0.f) {
        const float t = ImClamp((GetMousePos().x - (pos.x + pad)) / span, 0.f, 1.f);
        const int next = (int)(t * last + 0.5f);
        changed = next != value;
        value = next;
    }
    value = ImClamp(value, 0, last);

    const float hoverT = ItemAnim(hit.id, 1, IsItemHovered(), 16.f);
    const float knobT = UiAnim(SubId(hit.id, 2), (float)value / last, 22.f);

    ImDrawList* dl = GetWindowDrawList();
    const float cy = pos.y + height * 0.5f;
    const ImVec2 a(pos.x + pad, cy);
    const ImVec2 b(pos.x + pad + span, cy);
    dl->AddLine(a, b, UiCol::Stroke, 3.f);

    for (int i = 0; i < count; i++)
        dl->AddCircleFilled(ImVec2(a.x + span * (float)i / last, cy), 2.f, i <= value ? UiCol::Spam : UiCol::Mute);

    const float knobX = a.x + span * knobT;
    dl->AddLine(a, ImVec2(knobX, cy), UiCol::Spam, 3.f);
    if (hoverT > 0.01f)
        AddGlow(dl, ImVec2(knobX - 7.f, cy - 7.f), ImVec2(knobX + 7.f, cy + 7.f), UiCol::Spam, 7.f, 4, 0.2f * hoverT);
    dl->AddCircleFilled(ImVec2(knobX, cy), 6.f, UiMixColor(UiCol::Spam, UiCol::SpamText, hoverT));
    dl->AddCircleFilled(ImVec2(knobX, cy), 3.f, UiCol::Bg0);
    return changed;
}
} // namespace

void ImGui::Tip(const char* fmt, ...)
{
    if (IsItemHovered() && GImGui->HoveredIdTimer > 1.f) {
        va_list args;
        va_start(args, fmt);
        SetTooltipV(fmt, args);
        va_end(args);
    }
}

ImVec4 ImGui::FlashColor(float r, float g, float b, float periodSec, float minAlpha, float maxAlpha)
{
    const float phase = fmodf((float)GetTime() / periodSec, 1.f);
    return ImVec4(r, g, b, minAlpha + (maxAlpha - minAlpha) * fabsf(sinf(2.f * IM_PI * phase)));
}

ImU32 ImGui::UiFlashDanger()
{
    return GetColorU32(FlashColor(1.f, .3f, .37f, 2.f, .4f, 1.f));
}

ImU32 ImGui::UiFlashWarn()
{
    return GetColorU32(FlashColor(1.f, .42f, .1f, 2.f, .4f, 1.f));
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

ImU32 ImGui::UiHsvColor(float hue, float sat, float val)
{
    float r, g, b;
    ColorConvertHSVtoRGB(hue - floorf(hue), sat, val, r, g, b);
    return IM_COL32((int)(r * 255.f), (int)(g * 255.f), (int)(b * 255.f), 255);
}

bool ImGui::UiBeginPopup(const char* str_id)
{
    const ImGuiID animId = SubId(GetID(str_id), 0x506F5055);
    if (!IsPopupOpen(str_id)) {
        s_animStorage.SetFloat(animId, 0.f);
        return BeginPopup(str_id);
    }
    const float t = UiAnim(animId, 1.f, 18.f, 0.f);
    const float e = 1.f - (1.f - t) * (1.f - t);
    ImGuiContext& g = *GImGui;
    if (g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasPos) g.NextWindowData.PosVal.y -= 8.f * (1.f - e);
    PushStyleVar(ImGuiStyleVar_Alpha, e);
    if (BeginPopup(str_id)) return true;
    PopStyleVar();
    return false;
}

void ImGui::UiEndPopup()
{
    EndPopup();
    PopStyleVar();
}

void ImGui::LoadUiFonts()
{
    ImGuiIO& io = GetIO();
    io.Fonts->Clear();

    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.PixelSnapH = true;

    ImFontConfig merge = cfg;
    merge.MergeMode = true;

    auto addWithMono = [&](const void* data, int size) {
        ImFont* font = io.Fonts->AddFontFromMemoryTTF((void*)data, size, 20.f, &cfg);
        io.Fonts->AddFontFromMemoryTTF((void*)JetBrainsMono_data, JetBrainsMono_size, 18.f, &merge);
        return font;
    };
    UiFonts::Semi = addWithMono(RajdhaniSemiBold_data, RajdhaniSemiBold_size);
    UiFonts::Bold = addWithMono(RajdhaniBold_data, RajdhaniBold_size);
    UiFonts::Mono = io.Fonts->AddFontFromMemoryTTF((void*)JetBrainsMono_data, JetBrainsMono_size, 14.f, &cfg);
    io.FontDefault = UiFonts::Semi;
}

void ImGui::LoadUiStyle()
{
    ImGuiStyle& style = GetStyle();
    style.WindowPadding = ImVec2(10.f, 10.f);
    style.FramePadding = ImVec2(10.f, 6.f);
    style.CellPadding = ImVec2(6.f, 6.f);
    style.ItemSpacing = ImVec2(8.f, 6.f);
    style.ItemInnerSpacing = ImVec2(6.f, 6.f);
    style.IndentSpacing = 20.f;
    style.WindowBorderSize = 0.f;
    style.ChildBorderSize = 1.f;
    style.PopupBorderSize = 1.f;
    style.FrameBorderSize = 1.f;
    style.WindowRounding = 0.f;
    style.ChildRounding = 10.f;
    style.FrameRounding = 6.f;
    style.PopupRounding = 10.f;
    style.ScrollbarRounding = 9.f;
    style.GrabRounding = 6.f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);

    static constexpr struct {
        ImGuiCol idx;
        ImU32 col;
    } s_colors[] = {
        {ImGuiCol_Text, UiCol::Text},
        {ImGuiCol_TextDisabled, UiCol::Mute},
        {ImGuiCol_WindowBg, UiCol::Bg0},
        {ImGuiCol_ChildBg, UiCol::Bg1},
        {ImGuiCol_PopupBg, UiCol::Bg1},
        {ImGuiCol_Border, UiCol::Stroke},
        {ImGuiCol_BorderShadow, 0},
        {ImGuiCol_FrameBg, UiCol::Bg2},
        {ImGuiCol_FrameBgHovered, UiCol::HoverFill},
        {ImGuiCol_FrameBgActive, UiCol::PressFill},
        {ImGuiCol_TitleBg, UiCol::Bg0},
        {ImGuiCol_TitleBgActive, UiCol::Bg0},
        {ImGuiCol_TitleBgCollapsed, UiCol::Bg0},
        {ImGuiCol_MenuBarBg, UiCol::Bg1},
        {ImGuiCol_ScrollbarBg, UiCol::Bg1},
        {ImGuiCol_ScrollbarGrab, UiCol::Stroke},
        {ImGuiCol_ScrollbarGrabHovered, UiCol::HoverStroke},
        {ImGuiCol_ScrollbarGrabActive, UiCol::Sub},
        {ImGuiCol_CheckMark, UiCol::Spam},
        {ImGuiCol_SliderGrab, UiCol::Spam},
        {ImGuiCol_SliderGrabActive, UiCol::SpamText},
        {ImGuiCol_Button, UiCol::Bg2},
        {ImGuiCol_ButtonHovered, UiCol::HoverFill},
        {ImGuiCol_ButtonActive, UiCol::PressFill},
        {ImGuiCol_Header, UiCol::Bg2},
        {ImGuiCol_HeaderHovered, UiCol::HoverFill},
        {ImGuiCol_HeaderActive, UiCol::PressFill},
        {ImGuiCol_Separator, UiCol::StrokeSoft},
        {ImGuiCol_SeparatorHovered, UiCol::Stroke},
        {ImGuiCol_SeparatorActive, UiCol::Sub},
        {ImGuiCol_TextSelectedBg, UiWithAlpha(UiCol::Spam, 0.35f)},
        {ImGuiCol_NavHighlight, UiWithAlpha(UiCol::Spam, 0.8f)},
        {ImGuiCol_ModalWindowDimBg, UiWithAlpha(UiCol::Bg0, 0.6f)},
    };
    for (const auto& [idx, col] : s_colors)
        style.Colors[idx] = ColorConvertU32ToFloat4(col);
}

ImVec2 ImGui::CalcTrackedTextSize(ImFont* font, float size, const char* text, float tracking)
{
    ImFontBaked* baked = font->GetFontBaked(size);
    const char* end = text + strlen(text);
    float width = 0.f;
    for (const char* p = text; p < end;) {
        unsigned int c = 0;
        const int len = ImTextCharFromUtf8(&c, p, end);
        if (!len) break;
        p += len;
        if (const ImFontGlyph* glyph = baked->FindGlyph((ImWchar)c)) width += glyph->AdvanceX + tracking;
    }
    if (width > 0.f) width -= tracking;
    return ImVec2(width, size);
}

void ImGui::AddTrackedText(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 col, const char* text,
                           float tracking)
{
    ImFontBaked* baked = font->GetFontBaked(size);
    const char* end = text + strlen(text);
    float x = pos.x;
    for (const char* p = text; p < end;) {
        unsigned int c = 0;
        const int len = ImTextCharFromUtf8(&c, p, end);
        if (!len) break;
        const char* next = p + len;
        if (const ImFontGlyph* glyph = baked->FindGlyph((ImWchar)c)) {
            dl->AddText(font, size, ImVec2(x, pos.y), col, p, next);
            x += glyph->AdvanceX + tracking;
        }
        p = next;
    }
}

void ImGui::AddGlow(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 col, float rounding, int spread,
                    float alpha)
{
    for (int i = 1; i <= spread; i++) {
        const float t = 1.f - (float)i / (spread + 1);
        const float o = (float)i;
        dl->AddRect(min - ImVec2(o, o), max + ImVec2(o, o), UiWithAlpha(col, alpha * t * t), rounding + o, 0, 1.5f);
    }
}

void ImGui::AddLogoMark(ImDrawList* dl, const ImVec2& pos, float size)
{
    const float s = size / 22.f;
    const ImVec2 max = pos + ImVec2(size, size);
    AddGlow(dl, pos, max, UiCol::Spam, 5.f * s, (int)(8.f * s), 0.3f * Breath(1.6f, 0.85f, 0.15f));
    dl->AddRectFilled(pos, max, UiCol::Spam, 5.f * s);
    // "S" glyph as 5 bars in a 22x22 grid: x, y, w, h
    static constexpr float segs[][4] = {
        {6, 4, 10, 2}, {6, 4, 2, 8}, {6, 10, 10, 2}, {14, 10, 2, 8}, {6, 16, 10, 2},
    };
    for (const float* seg : segs) {
        const ImVec2 a(pos.x + seg[0] * s, pos.y + seg[1] * s);
        dl->AddRectFilled(a, ImVec2(a.x + seg[2] * s, a.y + seg[3] * s), UiCol::Bg0, 1.f * s);
    }
}

void ImGui::AddAccentHairline(ImDrawList* dl, const ImVec2& pos, float width, float height)
{
    const ImU32 col = UiWithAlpha(UiCol::Spam, 0.55f);
    const ImU32 clear = UiWithAlpha(UiCol::Spam, 0.f);
    const float fade = width * 0.2f;
    dl->AddRectFilledMultiColor(pos, ImVec2(pos.x + fade, pos.y + height), clear, col, col, clear);
    dl->AddRectFilled(ImVec2(pos.x + fade, pos.y), ImVec2(pos.x + width - fade, pos.y + height), col);
    dl->AddRectFilledMultiColor(ImVec2(pos.x + width - fade, pos.y), ImVec2(pos.x + width, pos.y + height), col, clear,
                                clear, col);
}

void ImGui::AddPanel(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float rounding)
{
    dl->AddRectFilled(min, ImVec2(max.x, min.y + rounding), UiCol::PanelTop, rounding, ImDrawFlags_RoundCornersTop);
    dl->AddRectFilledMultiColor(ImVec2(min.x, min.y + rounding), ImVec2(max.x, max.y - rounding), UiCol::PanelTop,
                                UiCol::PanelTop, UiCol::PanelBottom, UiCol::PanelBottom);
    dl->AddRectFilled(ImVec2(min.x, max.y - rounding), max, UiCol::PanelBottom, rounding,
                      ImDrawFlags_RoundCornersBottom);
    dl->AddRect(min, max, UiCol::StrokeSoft, rounding);
}

void ImGui::AddChevronDown(ImDrawList* dl, const ImVec2& center, ImU32 col, float flip)
{
    const float f = 2.f * (1.f - 2.f * flip);
    dl->AddLine(ImVec2(center.x - 4.f, center.y - f), ImVec2(center.x, center.y + f), col, 1.8f);
    dl->AddLine(ImVec2(center.x, center.y + f), ImVec2(center.x + 4.f, center.y - f), col, 1.8f);
}

void ImGui::AddKeycap(ImDrawList* dl, const ImVec2& min, const ImVec2& max, const char* text, ImU32 textCol)
{
    dl->AddRectFilled(min, max, UiCol::Bg2, 4.f);
    dl->AddRect(min, max, UiCol::Stroke, 4.f);
    const ImVec2 size = UiFonts::Mono->CalcTextSizeA(12.f, FLT_MAX, 0.f, text);
    dl->AddText(UiFonts::Mono, 12.f, ImVec2((min.x + max.x - size.x) * 0.5f, (min.y + max.y - 12.f) * 0.5f), textCol,
                text);
}

bool ImGui::UiBadge(const char* id, const ImVec2& pos, const char* text, ImU32 accent)
{
    const ImVec2 textSize = CalcTrackedTextSize(UiFonts::Bold, 13.f, text, 1.f);
    const ImVec2 size(textSize.x + 36.f, 26.f);
    const HitBox hit = PlaceInvisibleButton(id, pos, size);
    const float hoverT = ItemAnim(hit.id, 1, IsItemHovered(), 18.f);

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = pos + size;
    const float cy = pos.y + size.y * 0.5f;
    AddGlow(dl, pos, max, accent, 13.f, 5, (0.12f + 0.18f * hoverT) * Breath(2.5f, 0.8f, 0.2f));
    dl->AddRectFilled(pos, max, UiMixColor(UiCol::Bg1, accent, 0.12f + 0.08f * hoverT), 13.f);
    dl->AddRect(pos, max, UiWithAlpha(accent, 0.7f + 0.3f * hoverT), 13.f, 0, 1.f + 0.5f * hoverT);
    AddStatusDot(dl, ImVec2(pos.x + 13.f, cy), 3.f, accent, true);
    AddTrackedText(dl, UiFonts::Bold, 13.f, ImVec2(pos.x + 24.f, cy - 6.5f),
                   UiMixColor(accent, UiCol::Text, 0.25f + 0.35f * hoverT), text, 1.f);
    return hit.clicked;
}

bool ImGui::UiGhostButton(const char* id, const ImVec2& pos, float size, UiGlyph glyph)
{
    const HitBox hit = PlaceInvisibleButton(id, pos, ImVec2(size, size));
    const float hoverT = ItemAnim(hit.id, 1, IsItemHovered(), 18.f);
    const float pressT = ItemAnim(hit.id, 2, IsItemActive(), 28.f);

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = pos + ImVec2(size, size);
    const ImVec2 c = pos + ImVec2(size, size) * 0.5f;
    const float s = size / 30.f * (1.f - 0.12f * pressT);

    const bool danger = glyph == UiGlyph_Close;
    if (hoverT > 0.01f) dl->AddRectFilled(pos, max, UiWithAlpha(danger ? UiCol::DangerFill : UiCol::Bg2, hoverT), 6.f);
    const ImU32 col = UiMixColor(UiCol::Sub, danger ? UiCol::Danger : UiCol::Text, hoverT);

    auto bar = [&](float hx, float hy) { dl->AddRectFilled(c - ImVec2(hx, hy) * s, c + ImVec2(hx, hy) * s, col, s); };
    switch (glyph) {
    case UiGlyph_Gear: {
        dl->AddCircle(c, 6.f * s, col, 0, 2.f * s);
        static constexpr float teeth[][2] = {{0, -7}, {5, -5}, {7, 0}, {5, 5}, {0, 7}, {-5, 5}, {-7, 0}, {-5, -5}};
        for (const float* t : teeth) {
            const ImVec2 tc = c + ImVec2(t[0], t[1]) * s;
            dl->AddRectFilled(tc - ImVec2(1.5f, 1.5f) * s, tc + ImVec2(1.5f, 1.5f) * s, col, s);
        }
        break;
    }
    case UiGlyph_Minimize: bar(6.f, 1.f); break;
    case UiGlyph_Close:
        dl->AddLine(c - ImVec2(5.f, 5.f) * s, c + ImVec2(5.f, 5.f) * s, col, 2.f * s);
        dl->AddLine(c + ImVec2(-5.f, 5.f) * s, c + ImVec2(5.f, -5.f) * s, col, 2.f * s);
        break;
    case UiGlyph_Plus: bar(1.f, 5.f); [[fallthrough]];
    case UiGlyph_Minus: bar(5.f, 1.f); break;
    }
    return hit.clicked;
}

bool ImGui::UiChipFrame(const char* id, const ImVec2& pos, const ImVec2& size)
{
    const HitBox hit = PlaceInvisibleButton(id, pos, size);
    const float hoverT = ItemAnim(hit.id, 1, IsItemHovered(), 16.f);
    const float pressT = ItemAnim(hit.id, 2, IsItemActive(), 28.f);

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = pos + size;
    ImU32 fill = UiMixColor(UiCol::Bg1, IM_COL32(0x13, 0x1A, 0x28, 0xFF), hoverT);
    fill = UiMixColor(fill, UiCol::Bg2, 0.6f * pressT);
    dl->AddRectFilled(pos, max, fill, 10.f);
    dl->AddRect(pos, max, UiMixColor(UiCol::Stroke, UiCol::HoverStroke, hoverT), 10.f);
    return hit.clicked;
}

void ImGui::UiChipLabel(const ImVec2& pos, const char* text)
{
    AddTrackedText(GetWindowDrawList(), UiFonts::Semi, 12.f, pos, UiCol::Mute, text, 1.5f);
}

bool ImGui::UiLockChip(const char* id, const ImVec2& pos, const ImVec2& size, const char* label, bool locked)
{
    const bool clicked = UiChipFrame(id, pos, size);
    const float t = ItemAnim(GetItemID(), 3, locked, 14.f);
    UiChipLabel(pos + ImVec2(16.f, 7.f), label);

    ImDrawList* dl = GetWindowDrawList();
    AddStatusDot(dl, pos + ImVec2(22.f, 30.f), 3.5f, UiMixColor(UiCol::Mute, UiCol::Spam, t), locked);
    dl->AddText(UiFonts::Semi, 18.f, pos + ImVec2(32.f, 20.f), UiMixColor(UiCol::Sub, UiCol::Text, t),
                locked ? "BLOCKED" : "NONE");
    return clicked;
}

bool ImGui::UiEnablePill(const char* id, const ImVec2& pos, const ImVec2& size, bool enabled)
{
    const HitBox hit = PlaceInvisibleButton(id, pos, size);
    const float t = ItemAnim(hit.id, 1, enabled, 10.f);
    const float hoverT = ItemAnim(hit.id, 2, IsItemHovered(), 16.f);

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = pos + size;
    const ImU32 accent = UiMixColor(UiCol::Danger, UiCol::Ok, t);
    const ImU32 fill = UiMixColor(UiCol::DangerFill, UiCol::OkFill, t);
    const bool on = t >= 0.5f;
    const char* text = on ? "ENABLED" : "PAUSED";
    // content fades out through the midpoint of the color transition while the label swaps
    const float contentA = fabsf(t * 2.f - 1.f);

    if (t > 0.01f) AddGlow(dl, pos, max, accent, 10.f, 8, 0.18f * t * Breath(2.5f, 0.85f, 0.15f));
    dl->AddRectFilled(pos, max, fill, 10.f);
    dl->AddRect(pos, max, UiWithAlpha(accent, 0.9f + 0.1f * hoverT), 10.f, 0, 1.f + 0.5f * hoverT);

    const ImVec2 textSize = CalcTrackedTextSize(UiFonts::Bold, 19.f, text, 1.5f);
    const float contentW = 8.f + 10.f + textSize.x;
    const float x = pos.x + (size.x - contentW) * 0.5f;
    const float cy = pos.y + size.y * 0.5f;
    AddStatusDot(dl, ImVec2(x + 4.f, cy), 4.f, UiWithAlpha(accent, contentA), on && enabled);
    AddTrackedText(dl, UiFonts::Bold, 19.f, ImVec2(x + 18.f, cy - 9.5f), UiWithAlpha(accent, contentA), text, 1.5f);
    return hit.clicked;
}

bool ImGui::UiKey(const char* id, const ImVec2& pos, const ImVec2& size, const UiKeyDesc& desc)
{
    const HitBox hit = PlaceInvisibleButton(id, pos, size);
    const bool hovered = !desc.locked && IsItemHovered();

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = pos + size;
    const float rounding = 9.f;
    const bool multi = strlen(desc.label) > 1;

    ImU32 fill = desc.locked ? UiCol::ModFill : UiCol::KeyCap;
    ImU32 stroke = UiCol::StrokeSoft;
    float strokeW = 1.f;
    ImU32 accent = 0;
    ImU32 fillPressed = UiCol::PressFill;
    ImU32 labelCol = multi ? UiCol::Service : UiCol::Sub;

    if (desc.style != UiKeyStyle_None) {
        const KeyPalette& p = s_keyPalettes[desc.style];
        fill = p.fill;
        accent = p.accent;
        labelCol = p.text;
        fillPressed = p.pressed;
    }
    if (desc.tint) {
        accent = desc.tint;
        fill = UiMixColor(UiCol::Bg2, desc.tint, 0.14f);
        labelCol = UiMixColor(desc.tint, UiCol::Text, 0.4f);
        fillPressed = UiMixColor(UiCol::Bg2, desc.tint, 0.28f);
    }

    const float hoverT = ItemAnim(hit.id, 1, hovered, 18.f);
    const float pressT = ItemAnim(hit.id, 2, desc.pressed, 30.f);
    int* lastStyle = s_animStorage.GetIntRef(SubId(hit.id, 4), desc.style);
    float* pulse = s_animStorage.GetFloatRef(SubId(hit.id, 5), 0.f);
    if (*lastStyle != (int)desc.style) {
        *lastStyle = desc.style;
        *pulse = accent ? 1.f : 0.f;
    }
    *pulse = ImMax(0.f, *pulse - GImGui->IO.DeltaTime * 2.5f);

    if (accent) {
        stroke = UiWithAlpha(accent, desc.inherited ? 0.55f : 1.f);
        strokeW = 1.5f;
        labelCol = UiWithAlpha(labelCol, desc.inherited ? 0.7f : 1.f);
        if (!desc.inherited) AddGlow(dl, pos, max, accent, rounding, 6, 0.12f + 0.3f * *pulse * *pulse);
        fill = UiMixColor(fill, fillPressed, 0.35f * hoverT);
    } else {
        fill = UiMixColor(fill, UiCol::HoverFill, hoverT);
        stroke = UiMixColor(desc.locked ? UiCol::ModStroke : UiCol::KeyCapStroke, UiCol::Stroke, hoverT);
    }
    fill = UiMixColor(fill, fillPressed, pressT);
    if (!accent) {
        stroke = UiMixColor(stroke, UiCol::HoverStroke, pressT);
        labelCol = UiMixColor(labelCol, UiCol::Text, pressT);
    }
    if (desc.locked) labelCol = UiCol::ModText;

    if (hovered && desc.preview != UiKeyStyle_None && desc.preview != desc.style) {
        const KeyPalette& p = s_keyPalettes[desc.preview];
        fill = UiMixColor(fill, p.fill, 0.6f * hoverT);
        stroke = UiMixColor(stroke, p.accent, 0.5f * hoverT);
    }

    // slow rainbow wave drifting across the board, seeded by key position
    const ImU32 wave = UiHsvColor(pos.x * 0.0011f + pos.y * 0.0019f - (float)GetTime() * 0.09f, 0.9f, 1.f);
    AddGlow(dl, pos, max, wave, rounding, 7, 0.15f + 0.08f * hoverT);
    fill = UiMixColor(fill, wave, 0.05f);

    dl->AddRectFilled(pos, max, fill, rounding);
    dl->AddRect(pos, max, stroke, rounding, 0, strokeW);
    dl->AddLine(ImVec2(pos.x + rounding, max.y - 1.5f), ImVec2(max.x - rounding, max.y - 1.5f),
                UiWithAlpha(wave, 0.35f), 1.5f);

    ImFont* font = accent ? UiFonts::Bold : UiFonts::Semi;
    const float fontSize = multi ? 15.f : 20.f;
    const float tracking = multi ? 1.f : 0.f;
    const ImVec2 textSize = CalcTrackedTextSize(font, fontSize, desc.label, tracking);
    AddTrackedText(dl, font, fontSize, pos + (size - ImVec2(textSize.x, fontSize)) * 0.5f, labelCol, desc.label,
                   tracking);

    for (int i = 0; i < desc.dotCount; i++)
        dl->AddCircleFilled(ImVec2(max.x - 9.f - 7.f * i, pos.y + 9.f), 2.5f, desc.dots[i]);

    return hit.clicked && !desc.locked;
}

bool ImGui::UiBrushChip(const char* id, const ImVec2& pos, const ImVec2& size, const char* label, ImU32 accent,
                        bool active)
{
    const HitBox hit = PlaceInvisibleButton(id, pos, size);
    const float hoverT = ItemAnim(hit.id, 1, IsItemHovered(), 18.f);
    const float t = ItemAnim(hit.id, 2, active, 16.f);

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = pos + size;
    const ImU32 fill =
        UiMixColor(UiMixColor(UiCol::Bg2, UiCol::HoverFill, hoverT), UiMixColor(UiCol::Bg1, accent, 0.16f), t);
    const ImU32 stroke = UiMixColor(UiMixColor(UiCol::KeyCapStroke, UiCol::Stroke, hoverT), accent, t);
    const ImU32 labelCol = UiMixColor(UiCol::Sub, UiMixColor(accent, UiCol::Text, .3f), t);
    if (t > 0.01f) AddGlow(dl, pos, max, accent, 6.f, 4, 0.14f * t);
    dl->AddRectFilled(pos, max, fill, 6.f);
    dl->AddRect(pos, max, stroke, 6.f);
    const ImVec2 textSize = CalcTrackedTextSize(UiFonts::Semi, 13.f, label, 1.f);
    AddTrackedText(dl, UiFonts::Semi, 13.f, pos + (size - ImVec2(textSize.x, 13.f)) * .5f, labelCol, label, 1.f);
    return hit.clicked;
}

bool ImGui::UiToggleRow(const char* id, const char* label, bool on)
{
    const ImVec2 pos = GetCursorScreenPos();
    Dummy(ImVec2(210.f, 28.f));
    GetWindowDrawList()->AddText(UiFonts::Semi, 18.f, pos + ImVec2(8.f, 6.f), UiCol::Text, label);
    const bool clicked = UiToggle(id, pos + ImVec2(164.f, 4.f), on);
    SetCursorScreenPos(ImVec2(pos.x, pos.y + 34.f));
    return clicked;
}

bool ImGui::UiStepperRow(const char* id, const char* label, const char* value, int count, int& index)
{
    const ImVec2 pos = GetCursorScreenPos();
    Dummy(ImVec2(210.f, 24.f));
    ImDrawList* dl = GetWindowDrawList();
    dl->AddText(UiFonts::Semi, 18.f, pos + ImVec2(8.f, 3.f), UiCol::Text, label);
    const ImVec2 valueSize = UiFonts::Semi->CalcTextSizeA(18.f, FLT_MAX, 0.f, value);
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(pos.x + 202.f - valueSize.x, pos.y + 3.f), UiCol::SpamText, value);
    const bool changed = UiStepper(id, pos + ImVec2(8.f, 26.f), 194.f, count, index);
    SetCursorScreenPos(ImVec2(pos.x, pos.y + 54.f));
    return changed;
}

bool ImGui::UiMenuRow(const char* label, ImU32 dotCol, bool disabled, bool keepOpen, bool allowOverlap)
{
    char id[64];
    snprintf(id, sizeof(id), "##row_%s", label);
    const ImVec2 pos = GetCursorScreenPos();
    PushStyleColorTriplet(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    if (disabled) BeginDisabled();
    const ImGuiSelectableFlags flags = (keepOpen ? ImGuiSelectableFlags_NoAutoClosePopups : 0) |
                                       (allowOverlap ? ImGuiSelectableFlags_AllowOverlap : 0);
    const bool clicked = Selectable(id, false, flags, ImVec2(0.f, 24.f));
    if (disabled) EndDisabled();
    PopStyleColor(3);

    const ImGuiID wid = GetItemID();
    const float hoverT = ItemAnim(wid, 1, !disabled && IsItemHovered(), 18.f);
    const float pressT = ItemAnim(wid, 2, !disabled && IsItemActive(), 28.f);

    ImDrawList* dl = GetWindowDrawList();
    if (hoverT > 0.01f)
        dl->AddRectFilled(GetItemRectMin(), GetItemRectMax(),
                          UiWithAlpha(UiMixColor(UiCol::HoverFill, UiCol::PressFill, pressT), hoverT), 6.f);

    const float slide = 2.f * hoverT;
    float x = pos.x + 8.f + slide;
    if (dotCol) {
        AddStatusDot(dl, ImVec2(pos.x + 12.f + slide, pos.y + 12.f), 3.f, disabled ? UiWithAlpha(dotCol, 0.4f) : dotCol,
                     false);
        x = pos.x + 24.f + slide;
    }
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(x, pos.y + 3.f), disabled ? UiCol::Mute : UiCol::Text, label);
    return clicked;
}
