#include "ImGui.h"
#include <cstring>

#include "Resources/ImFont_JetBrainsMono.inl"
#include "Resources/ImFont_RajdhaniBold.inl"
#include "Resources/ImFont_RajdhaniSemiBold.inl"

ImFont* ImGui::UiFonts::Semi = NULL;
ImFont* ImGui::UiFonts::Bold = NULL;
ImFont* ImGui::UiFonts::Mono = NULL;

void ImGui::Tip(const char* fmt, ...)
{
    if (IsItemHovered() && GImGui->HoveredIdTimer > 1.f) {
        va_list args;
        va_start(args, fmt);
        ImGui::SetTooltipV(fmt, args);
        va_end(args);
    }
}

ImVec4 ImGui::FlashColor(float r, float g, float b, float f, float min, float max)
{
    float x = fmod(ImGui::GetTime() / f, 1.f);
    float a = min + (max - min) * std::abs(std::sin((M_PI * 2) * x));
    return ImVec4(r, g, b, a);
}

ImU32 ImGui::UiFlashDanger()
{
    return GetColorU32(FlashColor(1.f, .3f, .37f, 2.f, .4f, 1.f));
}

ImU32 ImGui::UiFlashWarn()
{
    return GetColorU32(FlashColor(1.f, .42f, .1f, 2.f, .4f, 1.f));
}

void ImGui::PushStyleColorTriplet(ImGuiCol idx, ImVec4 col)
{
    for (int i = 0; i < 3; i++)
        ImGui::PushStyleColor(idx + i, col);
}

static ImU32 WithAlpha(ImU32 col, float alpha)
{
    unsigned a = (unsigned)(((col >> IM_COL32_A_SHIFT) & 0xFF) * alpha);
    return (col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}

static ImGuiStorage s_animStorage;

static ImGuiID SubId(ImGuiID id, int n)
{
    return ImHashData(&n, sizeof(n), id);
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
        float ca = (float)((a >> shift) & 0xFF);
        float cb = (float)((b >> shift) & 0xFF);
        out |= (ImU32)(ca + (cb - ca) * t + 0.5f) << shift;
    }
    return out;
}

bool ImGui::UiBeginPopup(const char* str_id)
{
    ImGuiID animId = SubId(GetID(str_id), 0x506F5055);
    if (!IsPopupOpen(str_id)) {
        s_animStorage.SetFloat(animId, 0.f);
        return BeginPopup(str_id);
    }
    float t = UiAnim(animId, 1.f, 18.f, 0.f);
    float e = 1.f - (1.f - t) * (1.f - t);
    ImGuiContext& g = *GImGui;
    if (g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasPos) g.NextWindowData.PosVal.y -= 8.f * (1.f - e);
    PushStyleVar(ImGuiStyleVar_Alpha, e);
    if (!BeginPopup(str_id)) {
        PopStyleVar();
        return false;
    }
    return true;
}

void ImGui::UiEndPopup()
{
    EndPopup();
    PopStyleVar();
}

void ImGui::LoadUiFonts()
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.PixelSnapH = true;

    ImFontConfig merge = cfg;
    merge.MergeMode = true;

    UiFonts::Semi = io.Fonts->AddFontFromMemoryTTF((void*)RajdhaniSemiBold_data, RajdhaniSemiBold_size, 20.f, &cfg);
    io.Fonts->AddFontFromMemoryTTF((void*)JetBrainsMono_data, JetBrainsMono_size, 18.f, &merge);
    UiFonts::Bold = io.Fonts->AddFontFromMemoryTTF((void*)RajdhaniBold_data, RajdhaniBold_size, 20.f, &cfg);
    io.Fonts->AddFontFromMemoryTTF((void*)JetBrainsMono_data, JetBrainsMono_size, 18.f, &merge);
    UiFonts::Mono = io.Fonts->AddFontFromMemoryTTF((void*)JetBrainsMono_data, JetBrainsMono_size, 14.f, &cfg);
    io.FontDefault = UiFonts::Semi;
}

void ImGui::LoadUiStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
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

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ColorConvertU32ToFloat4(UiCol::Text);
    colors[ImGuiCol_TextDisabled] = ColorConvertU32ToFloat4(UiCol::Mute);
    colors[ImGuiCol_WindowBg] = ColorConvertU32ToFloat4(UiCol::Bg0);
    colors[ImGuiCol_ChildBg] = ColorConvertU32ToFloat4(UiCol::Bg1);
    colors[ImGuiCol_PopupBg] = ColorConvertU32ToFloat4(UiCol::Bg1);
    colors[ImGuiCol_Border] = ColorConvertU32ToFloat4(UiCol::Stroke);
    colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_FrameBg] = ColorConvertU32ToFloat4(UiCol::Bg2);
    colors[ImGuiCol_FrameBgHovered] = ColorConvertU32ToFloat4(UiCol::HoverFill);
    colors[ImGuiCol_FrameBgActive] = ColorConvertU32ToFloat4(UiCol::PressFill);
    colors[ImGuiCol_TitleBg] = ColorConvertU32ToFloat4(UiCol::Bg0);
    colors[ImGuiCol_TitleBgActive] = ColorConvertU32ToFloat4(UiCol::Bg0);
    colors[ImGuiCol_TitleBgCollapsed] = ColorConvertU32ToFloat4(UiCol::Bg0);
    colors[ImGuiCol_MenuBarBg] = ColorConvertU32ToFloat4(UiCol::Bg1);
    colors[ImGuiCol_ScrollbarBg] = ColorConvertU32ToFloat4(UiCol::Bg1);
    colors[ImGuiCol_ScrollbarGrab] = ColorConvertU32ToFloat4(UiCol::Stroke);
    colors[ImGuiCol_ScrollbarGrabHovered] = ColorConvertU32ToFloat4(UiCol::HoverStroke);
    colors[ImGuiCol_ScrollbarGrabActive] = ColorConvertU32ToFloat4(UiCol::Sub);
    colors[ImGuiCol_CheckMark] = ColorConvertU32ToFloat4(UiCol::Spam);
    colors[ImGuiCol_SliderGrab] = ColorConvertU32ToFloat4(UiCol::Spam);
    colors[ImGuiCol_SliderGrabActive] = ColorConvertU32ToFloat4(UiCol::SpamText);
    colors[ImGuiCol_Button] = ColorConvertU32ToFloat4(UiCol::Bg2);
    colors[ImGuiCol_ButtonHovered] = ColorConvertU32ToFloat4(UiCol::HoverFill);
    colors[ImGuiCol_ButtonActive] = ColorConvertU32ToFloat4(UiCol::PressFill);
    colors[ImGuiCol_Header] = ColorConvertU32ToFloat4(UiCol::Bg2);
    colors[ImGuiCol_HeaderHovered] = ColorConvertU32ToFloat4(UiCol::HoverFill);
    colors[ImGuiCol_HeaderActive] = ColorConvertU32ToFloat4(UiCol::PressFill);
    colors[ImGuiCol_Separator] = ColorConvertU32ToFloat4(UiCol::StrokeSoft);
    colors[ImGuiCol_SeparatorHovered] = ColorConvertU32ToFloat4(UiCol::Stroke);
    colors[ImGuiCol_SeparatorActive] = ColorConvertU32ToFloat4(UiCol::Sub);
    colors[ImGuiCol_TextSelectedBg] = ColorConvertU32ToFloat4(WithAlpha(UiCol::Spam, 0.35f));
    colors[ImGuiCol_NavHighlight] = ColorConvertU32ToFloat4(WithAlpha(UiCol::Spam, 0.8f));
    colors[ImGuiCol_ModalWindowDimBg] = ColorConvertU32ToFloat4(WithAlpha(UiCol::Bg0, 0.6f));
}

ImVec2 ImGui::CalcTrackedTextSize(ImFont* font, float size, const char* text, float tracking)
{
    ImFontBaked* baked = font->GetFontBaked(size);
    const char* p = text;
    const char* end = text + strlen(text);
    float width = 0.f;
    while (p < end) {
        unsigned int c = 0;
        int len = ImTextCharFromUtf8(&c, p, end);
        if (!len) break;
        p += len;
        const ImFontGlyph* glyph = baked->FindGlyph((ImWchar)c);
        if (!glyph) continue;
        width += glyph->AdvanceX + tracking;
    }
    if (width > 0.f) width -= tracking;
    return ImVec2(width, size);
}

void ImGui::AddTrackedText(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 col, const char* text,
                           float tracking)
{
    ImFontBaked* baked = font->GetFontBaked(size);
    const char* p = text;
    const char* end = text + strlen(text);
    float x = pos.x;
    while (p < end) {
        unsigned int c = 0;
        int len = ImTextCharFromUtf8(&c, p, end);
        if (!len) break;
        const char* next = p + len;
        const ImFontGlyph* glyph = baked->FindGlyph((ImWchar)c);
        if (glyph) {
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
        float t = 1.f - (float)i / (spread + 1);
        float o = (float)i;
        dl->AddRect(ImVec2(min.x - o, min.y - o), ImVec2(max.x + o, max.y + o), WithAlpha(col, alpha * t * t),
                    rounding + o, 0, 1.5f);
    }
}

void ImGui::AddLogoMark(ImDrawList* dl, const ImVec2& pos, float size)
{
    const float s = size / 22.f;
    const ImVec2 max = ImVec2(pos.x + size, pos.y + size);
    const float breath = 0.85f + 0.15f * sinf((float)GetTime() * 1.6f);
    AddGlow(dl, pos, max, UiCol::Spam, 5.f * s, (int)(8.f * s), 0.3f * breath);
    dl->AddRectFilled(pos, max, UiCol::Spam, 5.f * s);
    static const float segs[][4] = {
        {6, 4, 10, 2}, {6, 4, 2, 8}, {6, 10, 10, 2}, {14, 10, 2, 8}, {6, 16, 10, 2},
    };
    for (const float* seg : segs) {
        ImVec2 a = ImVec2(pos.x + seg[0] * s, pos.y + seg[1] * s);
        dl->AddRectFilled(a, ImVec2(a.x + seg[2] * s, a.y + seg[3] * s), UiCol::Bg0, 1.f * s);
    }
}

void ImGui::AddStatusDot(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, bool glow)
{
    if (glow) {
        float breath = 0.7f + 0.3f * sinf((float)GetTime() * 3.f);
        for (int i = 1; i <= 4; i++) {
            float t = 1.f - (float)i / 5.f;
            dl->AddCircle(center, radius + i, WithAlpha(col, 0.35f * t * t * breath), 0, 1.5f);
        }
    }
    dl->AddCircleFilled(center, radius, col);
}

void ImGui::AddAccentHairline(ImDrawList* dl, const ImVec2& pos, float width, float height)
{
    const ImU32 col = WithAlpha(UiCol::Spam, 0.55f);
    const ImU32 clear = WithAlpha(UiCol::Spam, 0.f);
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
    ImVec2 size = UiFonts::Mono->CalcTextSizeA(12.f, FLT_MAX, 0.f, text);
    dl->AddText(UiFonts::Mono, 12.f, ImVec2((min.x + max.x - size.x) * 0.5f, (min.y + max.y - 12.f) * 0.5f), textCol,
                text);
}

bool ImGui::UiBadge(const char* id, const ImVec2& pos, const char* text, ImU32 accent)
{
    ImVec2 textSize = CalcTrackedTextSize(UiFonts::Bold, 13.f, text, 1.f);
    ImVec2 size = ImVec2(textSize.x + 36.f, 26.f);
    SetCursorScreenPos(pos);
    bool clicked = InvisibleButton(id, size);
    ImGuiID wid = GetItemID();
    float hoverT = UiAnim(SubId(wid, 1), IsItemHovered() ? 1.f : 0.f, 18.f);

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);
    const float cy = pos.y + size.y * 0.5f;
    float breath = 0.8f + 0.2f * sinf((float)GetTime() * 2.5f);
    AddGlow(dl, pos, max, accent, 13.f, 5, (0.12f + 0.18f * hoverT) * breath);
    dl->AddRectFilled(pos, max, UiMixColor(UiCol::Bg1, accent, 0.12f + 0.08f * hoverT), 13.f);
    dl->AddRect(pos, max, WithAlpha(accent, 0.7f + 0.3f * hoverT), 13.f, 0, 1.f + 0.5f * hoverT);
    AddStatusDot(dl, ImVec2(pos.x + 13.f, cy), 3.f, accent, true);
    AddTrackedText(dl, UiFonts::Bold, 13.f, ImVec2(pos.x + 24.f, cy - 6.5f),
                   UiMixColor(accent, UiCol::Text, 0.25f + 0.35f * hoverT), text, 1.f);
    return clicked;
}

bool ImGui::UiGhostButton(const char* id, const ImVec2& pos, float size, UiGlyph glyph)
{
    SetCursorScreenPos(pos);
    bool clicked = InvisibleButton(id, ImVec2(size, size));
    ImGuiID wid = GetItemID();
    float hoverT = UiAnim(SubId(wid, 1), IsItemHovered() ? 1.f : 0.f, 18.f);
    float pressT = UiAnim(SubId(wid, 2), IsItemActive() ? 1.f : 0.f, 28.f);

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = ImVec2(pos.x + size, pos.y + size);
    const ImVec2 c = ImVec2(pos.x + size * 0.5f, pos.y + size * 0.5f);
    const float s = size / 30.f * (1.f - 0.12f * pressT);

    bool danger = glyph == UiGlyph_Close;
    if (hoverT > 0.01f) dl->AddRectFilled(pos, max, WithAlpha(danger ? UiCol::DangerFill : UiCol::Bg2, hoverT), 6.f);
    ImU32 col = UiMixColor(UiCol::Sub, danger ? UiCol::Danger : UiCol::Text, hoverT);

    switch (glyph) {
    case UiGlyph_Gear: {
        dl->AddCircle(c, 6.f * s, col, 0, 2.f * s);
        static const float teeth[][2] = {{0, -7}, {5, -5}, {7, 0}, {5, 5}, {0, 7}, {-5, 5}, {-7, 0}, {-5, -5}};
        for (const float* t : teeth) {
            ImVec2 tc = ImVec2(c.x + t[0] * s, c.y + t[1] * s);
            dl->AddRectFilled(ImVec2(tc.x - 1.5f * s, tc.y - 1.5f * s), ImVec2(tc.x + 1.5f * s, tc.y + 1.5f * s), col,
                              1.f * s);
        }
        break;
    }
    case UiGlyph_Minimize:
        dl->AddRectFilled(ImVec2(c.x - 6.f * s, c.y - 1.f * s), ImVec2(c.x + 6.f * s, c.y + 1.f * s), col, 1.f * s);
        break;
    case UiGlyph_Close:
        dl->AddLine(ImVec2(c.x - 5.f * s, c.y - 5.f * s), ImVec2(c.x + 5.f * s, c.y + 5.f * s), col, 2.f * s);
        dl->AddLine(ImVec2(c.x - 5.f * s, c.y + 5.f * s), ImVec2(c.x + 5.f * s, c.y - 5.f * s), col, 2.f * s);
        break;
    case UiGlyph_Plus:
        dl->AddRectFilled(ImVec2(c.x - 1.f * s, c.y - 5.f * s), ImVec2(c.x + 1.f * s, c.y + 5.f * s), col, 1.f * s);
        [[fallthrough]];
    case UiGlyph_Minus:
        dl->AddRectFilled(ImVec2(c.x - 5.f * s, c.y - 1.f * s), ImVec2(c.x + 5.f * s, c.y + 1.f * s), col, 1.f * s);
        break;
    }
    return clicked;
}

bool ImGui::UiChipFrame(const char* id, const ImVec2& pos, const ImVec2& size)
{
    SetCursorScreenPos(pos);
    bool clicked = InvisibleButton(id, size);
    ImGuiID wid = GetItemID();
    float hoverT = UiAnim(SubId(wid, 1), IsItemHovered() ? 1.f : 0.f, 16.f);
    float pressT = UiAnim(SubId(wid, 2), IsItemActive() ? 1.f : 0.f, 28.f);

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);
    ImU32 fill = UiMixColor(UiCol::Bg1, IM_COL32(0x13, 0x1A, 0x28, 0xFF), hoverT);
    fill = UiMixColor(fill, UiCol::Bg2, 0.6f * pressT);
    dl->AddRectFilled(pos, max, fill, 10.f);
    dl->AddRect(pos, max, UiMixColor(UiCol::Stroke, UiCol::HoverStroke, hoverT), 10.f);
    return clicked;
}

void ImGui::UiChipLabel(const ImVec2& pos, const char* text)
{
    AddTrackedText(GetWindowDrawList(), UiFonts::Semi, 12.f, pos, UiCol::Mute, text, 1.5f);
}

bool ImGui::UiLockChip(const char* id, const ImVec2& pos, const ImVec2& size, const char* label, bool locked)
{
    bool clicked = UiChipFrame(id, pos, size);
    ImGuiID wid = GetItemID();
    UiChipLabel(ImVec2(pos.x + 16.f, pos.y + 7.f), label);
    float t = UiAnim(SubId(wid, 3), locked ? 1.f : 0.f, 14.f);

    ImDrawList* dl = GetWindowDrawList();
    AddStatusDot(dl, ImVec2(pos.x + 22.f, pos.y + 30.f), 3.5f, UiMixColor(UiCol::Mute, UiCol::Spam, t), locked);
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(pos.x + 32.f, pos.y + 20.f), UiMixColor(UiCol::Sub, UiCol::Text, t),
                locked ? "BLOCKED" : "NONE");
    return clicked;
}

bool ImGui::UiEnablePill(const char* id, const ImVec2& pos, const ImVec2& size, bool enabled)
{
    SetCursorScreenPos(pos);
    bool clicked = InvisibleButton(id, size);
    ImGuiID wid = GetItemID();
    float t = UiAnim(SubId(wid, 1), enabled ? 1.f : 0.f, 10.f);
    float hoverT = UiAnim(SubId(wid, 2), IsItemHovered() ? 1.f : 0.f, 16.f);

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);
    const ImU32 accent = UiMixColor(UiCol::Danger, UiCol::Ok, t);
    const ImU32 fill = UiMixColor(UiCol::DangerFill, UiCol::OkFill, t);
    const bool on = t >= 0.5f;
    const char* text = on ? "ENABLED" : "PAUSED";
    const float contentA = fabsf(t * 2.f - 1.f);

    if (t > 0.01f) {
        float breath = 0.85f + 0.15f * sinf((float)GetTime() * 2.5f);
        AddGlow(dl, pos, max, accent, 10.f, 8, 0.18f * t * breath);
    }
    dl->AddRectFilled(pos, max, fill, 10.f);
    dl->AddRect(pos, max, WithAlpha(accent, 0.9f + 0.1f * hoverT), 10.f, 0, 1.f + 0.5f * hoverT);

    ImVec2 textSize = CalcTrackedTextSize(UiFonts::Bold, 19.f, text, 1.5f);
    const float contentW = 8.f + 10.f + textSize.x;
    const float x = pos.x + (size.x - contentW) * 0.5f;
    const float cy = pos.y + size.y * 0.5f;
    AddStatusDot(dl, ImVec2(x + 4.f, cy), 4.f, WithAlpha(accent, contentA), on && enabled);
    AddTrackedText(dl, UiFonts::Bold, 19.f, ImVec2(x + 18.f, cy - 9.5f), WithAlpha(accent, contentA), text, 1.5f);
    return clicked;
}

bool ImGui::UiKey(const char* id, const ImVec2& pos, const ImVec2& size, const UiKeyDesc& desc)
{
    SetCursorScreenPos(pos);
    bool clicked = InvisibleButton(id, size);
    ImGuiID wid = GetItemID();
    bool hovered = !desc.locked && IsItemHovered();

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);
    const float rounding = 9.f;

    ImU32 fill = desc.locked ? UiCol::ModFill : UiCol::KeyCap;
    ImU32 stroke = UiCol::StrokeSoft;
    float strokeW = 1.f;
    ImU32 accent = 0;
    ImU32 fillPressed = UiCol::PressFill;
    bool multi = strlen(desc.label) > 1;
    ImU32 labelCol = multi ? UiCol::Service : UiCol::Sub;

    switch (desc.style) {
    case UiKeyStyle_Spam:
        fill = UiCol::SpamFill;
        accent = UiCol::Spam;
        labelCol = UiCol::SpamText;
        fillPressed = IM_COL32(0x3E, 0x2A, 0x1C, 0xFF);
        break;
    case UiKeyStyle_Speedy:
        fill = UiCol::SpeedyFill;
        accent = UiCol::Speedy;
        labelCol = UiCol::SpeedyText;
        fillPressed = IM_COL32(0x16, 0x3B, 0x49, 0xFF);
        break;
    case UiKeyStyle_Blocked:
        fill = UiCol::DangerFill;
        accent = UiCol::Danger;
        labelCol = UiCol::DangerText;
        fillPressed = IM_COL32(0x3E, 0x1F, 0x24, 0xFF);
        break;
    default: break;
    }

    if (desc.tint) {
        accent = desc.tint;
        fill = UiMixColor(UiCol::Bg2, desc.tint, 0.14f);
        labelCol = UiMixColor(desc.tint, UiCol::Text, 0.4f);
        fillPressed = UiMixColor(UiCol::Bg2, desc.tint, 0.28f);
    }

    float hoverT = UiAnim(SubId(wid, 1), hovered ? 1.f : 0.f, 18.f);
    float pressT = UiAnim(SubId(wid, 2), desc.pressed ? 1.f : 0.f, 30.f);
    int* lastStyle = s_animStorage.GetIntRef(SubId(wid, 4), desc.style);
    float* pulse = s_animStorage.GetFloatRef(SubId(wid, 5), 0.f);
    if (*lastStyle != (int)desc.style) {
        *lastStyle = desc.style;
        *pulse = accent ? 1.f : 0.f;
    }
    *pulse = ImMax(0.f, *pulse - GImGui->IO.DeltaTime * 2.5f);

    const float dim = desc.inherited ? 0.55f : 1.f;
    if (accent) {
        stroke = WithAlpha(accent, dim);
        strokeW = 1.5f;
        labelCol = WithAlpha(labelCol, desc.inherited ? 0.7f : 1.f);
        if (!desc.inherited) AddGlow(dl, pos, max, accent, rounding, 6, 0.12f + 0.3f * *pulse * *pulse);
        fill = UiMixColor(fill, fillPressed, 0.35f * hoverT);
    } else {
        ImU32 rest = desc.locked ? UiCol::ModStroke : UiCol::KeyCapStroke;
        fill = UiMixColor(fill, UiCol::HoverFill, hoverT);
        stroke = UiMixColor(rest, UiCol::Stroke, hoverT);
    }
    fill = UiMixColor(fill, fillPressed, pressT);
    if (!accent) {
        stroke = UiMixColor(stroke, UiCol::HoverStroke, pressT);
        labelCol = UiMixColor(labelCol, UiCol::Text, pressT);
    }
    if (desc.locked) labelCol = UiCol::ModText;

    if (hovered && desc.preview != UiKeyStyle_None && desc.preview != desc.style) {
        ImU32 previewFill = fill;
        ImU32 previewAccent = stroke;
        switch (desc.preview) {
        case UiKeyStyle_Spam:
            previewFill = UiCol::SpamFill;
            previewAccent = UiCol::Spam;
            break;
        case UiKeyStyle_Speedy:
            previewFill = UiCol::SpeedyFill;
            previewAccent = UiCol::Speedy;
            break;
        case UiKeyStyle_Blocked:
            previewFill = UiCol::DangerFill;
            previewAccent = UiCol::Danger;
            break;
        default: break;
        }
        fill = UiMixColor(fill, previewFill, 0.6f * hoverT);
        stroke = UiMixColor(stroke, previewAccent, 0.5f * hoverT);
    }

    float waveHue = pos.x * 0.0011f + pos.y * 0.0019f - (float)GetTime() * 0.09f;
    waveHue -= floorf(waveHue);
    float wr, wg, wb;
    ColorConvertHSVtoRGB(waveHue, 0.9f, 1.f, wr, wg, wb);
    ImU32 wave = IM_COL32((int)(wr * 255.f), (int)(wg * 255.f), (int)(wb * 255.f), 255);
    AddGlow(dl, pos, max, wave, rounding, 7, 0.15f + 0.08f * hoverT);
    fill = UiMixColor(fill, wave, 0.05f);

    dl->AddRectFilled(pos, max, fill, rounding);
    dl->AddRect(pos, max, stroke, rounding, 0, strokeW);
    dl->AddLine(ImVec2(pos.x + rounding, max.y - 1.5f), ImVec2(max.x - rounding, max.y - 1.5f), WithAlpha(wave, 0.35f),
                1.5f);

    ImFont* font = accent ? UiFonts::Bold : UiFonts::Semi;
    float fontSize = multi ? 15.f : 20.f;
    float tracking = multi ? 1.f : 0.f;
    ImVec2 textSize = CalcTrackedTextSize(font, fontSize, desc.label, tracking);
    AddTrackedText(dl, font, fontSize, ImVec2(pos.x + (size.x - textSize.x) * 0.5f, pos.y + (size.y - fontSize) * 0.5f),
                   labelCol, desc.label, tracking);

    float dotX = max.x - 9.f;
    for (int i = 0; i < desc.dotCount; i++) {
        dl->AddCircleFilled(ImVec2(dotX, pos.y + 9.f), 2.5f, desc.dots[i]);
        dotX -= 7.f;
    }

    return clicked && !desc.locked;
}

bool ImGui::UiToggle(const char* id, const ImVec2& pos, bool on)
{
    const ImVec2 size = ImVec2(38.f, 20.f);
    SetCursorScreenPos(pos);
    bool clicked = InvisibleButton(id, size);
    ImGuiID wid = GetItemID();
    float t = UiAnim(SubId(wid, 1), on ? 1.f : 0.f, 16.f);
    float e = t * t * (3.f - 2.f * t);

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);
    if (e > 0.01f) AddGlow(dl, pos, max, UiCol::Spam, 10.f, 5, 0.2f * e);
    dl->AddRectFilled(pos, max, UiMixColor(UiCol::Bg0, UiCol::Spam, e), 10.f);
    if (e < 0.99f) dl->AddRect(pos, max, WithAlpha(UiCol::Stroke, 1.f - e), 10.f);
    dl->AddCircleFilled(ImVec2(pos.x + 10.f + 18.f * e, pos.y + 10.f), 7.f, UiMixColor(UiCol::Mute, UiCol::Bg0, e));
    return clicked;
}

bool ImGui::UiBrushChip(const char* id, const ImVec2& pos, const ImVec2& size, const char* label, ImU32 accent,
                        bool active)
{
    SetCursorScreenPos(pos);
    bool clicked = InvisibleButton(id, size);
    ImGuiID wid = GetItemID();
    float hoverT = UiAnim(SubId(wid, 1), IsItemHovered() ? 1.f : 0.f, 18.f);
    float t = UiAnim(SubId(wid, 2), active ? 1.f : 0.f, 16.f);

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);
    ImU32 fill = UiMixColor(UiMixColor(UiCol::Bg2, UiCol::HoverFill, hoverT), UiMixColor(UiCol::Bg1, accent, 0.16f), t);
    ImU32 stroke = UiMixColor(UiMixColor(UiCol::KeyCapStroke, UiCol::Stroke, hoverT), accent, t);
    ImU32 labelCol = UiMixColor(UiCol::Sub, UiMixColor(accent, UiCol::Text, .3f), t);
    if (t > 0.01f) AddGlow(dl, pos, max, accent, 6.f, 4, 0.14f * t);
    dl->AddRectFilled(pos, max, fill, 6.f);
    dl->AddRect(pos, max, stroke, 6.f);
    ImVec2 textSize = CalcTrackedTextSize(UiFonts::Semi, 13.f, label, 1.f);
    AddTrackedText(dl, UiFonts::Semi, 13.f, ImVec2(pos.x + (size.x - textSize.x) * .5f, pos.y + (size.y - 13.f) * .5f),
                   labelCol, label, 1.f);
    return clicked;
}

bool ImGui::UiStepper(const char* id, const ImVec2& pos, float width, int count, int& value)
{
    const float height = 20.f;
    SetCursorScreenPos(pos);
    InvisibleButton(id, ImVec2(width, height));
    ImGuiID wid = GetItemID();

    const float pad = height * 0.5f;
    const float span = width - pad * 2.f;
    const int last = count > 1 ? count - 1 : 1;

    bool changed = false;
    if (IsItemActive() && span > 0.f) {
        float t = ImClamp((GetMousePos().x - (pos.x + pad)) / span, 0.f, 1.f);
        int next = (int)(t * last + 0.5f);
        if (next != value) {
            value = next;
            changed = true;
        }
    }
    value = ImClamp(value, 0, last);

    float hoverT = UiAnim(SubId(wid, 1), IsItemHovered() ? 1.f : 0.f, 16.f);
    float knobT = UiAnim(SubId(wid, 2), last > 0 ? (float)value / last : 0.f, 22.f);

    ImDrawList* dl = GetWindowDrawList();
    const float cy = pos.y + height * 0.5f;
    const ImVec2 a = ImVec2(pos.x + pad, cy);
    const ImVec2 b = ImVec2(pos.x + pad + span, cy);
    dl->AddLine(a, b, UiCol::Stroke, 3.f);

    for (int i = 0; i < count; i++) {
        float tx = a.x + span * (last > 0 ? (float)i / last : 0.f);
        dl->AddCircleFilled(ImVec2(tx, cy), 2.f, i <= value ? UiCol::Spam : UiCol::Mute);
    }

    float knobX = a.x + span * knobT;
    dl->AddLine(a, ImVec2(knobX, cy), UiCol::Spam, 3.f);
    if (hoverT > 0.01f)
        AddGlow(dl, ImVec2(knobX - 7.f, cy - 7.f), ImVec2(knobX + 7.f, cy + 7.f), UiCol::Spam, 7.f, 4, 0.2f * hoverT);
    dl->AddCircleFilled(ImVec2(knobX, cy), 6.f, UiMixColor(UiCol::Spam, UiCol::SpamText, hoverT));
    dl->AddCircleFilled(ImVec2(knobX, cy), 3.f, UiCol::Bg0);
    return changed;
}

bool ImGui::UiToggleRow(const char* id, const char* label, bool on)
{
    ImVec2 pos = GetCursorScreenPos();
    Dummy(ImVec2(210.f, 28.f));
    GetWindowDrawList()->AddText(UiFonts::Semi, 18.f, ImVec2(pos.x + 8.f, pos.y + 6.f), UiCol::Text, label);
    bool clicked = UiToggle(id, ImVec2(pos.x + 164.f, pos.y + 4.f), on);
    SetCursorScreenPos(ImVec2(pos.x, pos.y + 34.f));
    return clicked;
}

bool ImGui::UiStepperRow(const char* id, const char* label, const char* value, int count, int& index)
{
    ImVec2 pos = GetCursorScreenPos();
    Dummy(ImVec2(210.f, 24.f));
    ImDrawList* dl = GetWindowDrawList();
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(pos.x + 8.f, pos.y + 3.f), UiCol::Text, label);
    ImVec2 valueSize = UiFonts::Semi->CalcTextSizeA(18.f, FLT_MAX, 0.f, value);
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(pos.x + 202.f - valueSize.x, pos.y + 3.f), UiCol::SpamText, value);
    bool changed = UiStepper(id, ImVec2(pos.x + 8.f, pos.y + 26.f), 194.f, count, index);
    SetCursorScreenPos(ImVec2(pos.x, pos.y + 54.f));
    return changed;
}

bool ImGui::UiMenuRow(const char* label, ImU32 dotCol, bool disabled, bool keepOpen, bool allowOverlap)
{
    char id[64];
    snprintf(id, sizeof(id), "##row_%s", label);
    ImVec2 pos = GetCursorScreenPos();
    PushStyleColorTriplet(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    if (disabled) BeginDisabled();
    ImGuiSelectableFlags flags = (keepOpen ? ImGuiSelectableFlags_NoAutoClosePopups : 0) |
                                 (allowOverlap ? ImGuiSelectableFlags_AllowOverlap : 0);
    bool clicked = Selectable(id, false, flags, ImVec2(0.f, 24.f));
    if (disabled) EndDisabled();
    PopStyleColorTriplet();

    ImGuiID wid = GetItemID();
    float hoverT = UiAnim(SubId(wid, 1), (!disabled && IsItemHovered()) ? 1.f : 0.f, 18.f);
    float pressT = UiAnim(SubId(wid, 2), (!disabled && IsItemActive()) ? 1.f : 0.f, 28.f);

    ImDrawList* dl = GetWindowDrawList();
    if (hoverT > 0.01f)
        dl->AddRectFilled(GetItemRectMin(), GetItemRectMax(),
                          WithAlpha(UiMixColor(UiCol::HoverFill, UiCol::PressFill, pressT), hoverT), 6.f);

    float x = pos.x + 8.f + 2.f * hoverT;
    if (dotCol) {
        AddStatusDot(dl, ImVec2(pos.x + 12.f + 2.f * hoverT, pos.y + 12.f), 3.f,
                     disabled ? WithAlpha(dotCol, 0.4f) : dotCol, false);
        x = pos.x + 24.f + 2.f * hoverT;
    }
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(x, pos.y + 3.f), disabled ? UiCol::Mute : UiCol::Text, label);
    return clicked;
}
