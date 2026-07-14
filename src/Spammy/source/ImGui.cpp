#include "ImGui.h"
#include <cstring>

#include "ImFont_JetBrainsMono.inl"
#include "ImFont_RajdhaniBold.inl"
#include "ImFont_RajdhaniSemiBold.inl"

ImFont* ImGui::UiFonts::Semi = NULL;
ImFont* ImGui::UiFonts::Bold = NULL;
ImFont* ImGui::UiFonts::Mono = NULL;

void ImGui::Tip(const char* fmt, ...)
{
    if (IsItemHovered() && GImGui->HoveredIdTimer > 1.f) {
        va_list args;
        va_start(args, fmt);
        ImGui::SetTooltipV(fmt, args);
    }
}

ImVec4 ImGui::FlashColor(float r, float g, float b, float f, float min, float max)
{
    float x = fmod(ImGui::GetTime() / f, 1.f);
    float a = min + (max - min) * std::abs(std::sin((M_PI * 2) * x));
    return ImVec4(r, g, b, a);
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
    AddGlow(dl, pos, max, UiCol::Spam, 5.f * s, (int)(8.f * s), 0.3f);
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
        for (int i = 1; i <= 4; i++) {
            float t = 1.f - (float)i / 5.f;
            dl->AddCircle(center, radius + i, WithAlpha(col, 0.35f * t * t), 0, 1.5f);
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

void ImGui::AddChevronDown(ImDrawList* dl, const ImVec2& center, ImU32 col)
{
    dl->AddLine(ImVec2(center.x - 4.f, center.y - 2.f), ImVec2(center.x, center.y + 2.f), col, 1.8f);
    dl->AddLine(ImVec2(center.x, center.y + 2.f), ImVec2(center.x + 4.f, center.y - 2.f), col, 1.8f);
}

void ImGui::AddKeycap(ImDrawList* dl, const ImVec2& min, const ImVec2& max, const char* text, ImU32 textCol)
{
    dl->AddRectFilled(min, max, UiCol::Bg2, 4.f);
    dl->AddRect(min, max, UiCol::Stroke, 4.f);
    ImVec2 size = UiFonts::Mono->CalcTextSizeA(12.f, FLT_MAX, 0.f, text);
    dl->AddText(UiFonts::Mono, 12.f, ImVec2((min.x + max.x - size.x) * 0.5f, (min.y + max.y - 12.f) * 0.5f), textCol,
                text);
}

bool ImGui::UiGhostButton(const char* id, const ImVec2& pos, float size, UiGlyph glyph)
{
    SetCursorScreenPos(pos);
    bool clicked = InvisibleButton(id, ImVec2(size, size));
    bool hovered = IsItemHovered();

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = ImVec2(pos.x + size, pos.y + size);
    const ImVec2 c = ImVec2(pos.x + size * 0.5f, pos.y + size * 0.5f);
    const float s = size / 30.f;

    ImU32 col = UiCol::Sub;
    if (hovered) {
        bool danger = glyph == UiGlyph_Close;
        dl->AddRectFilled(pos, max, danger ? UiCol::DangerFill : UiCol::Bg2, 6.f);
        col = danger ? UiCol::Danger : UiCol::Text;
    }

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
    }
    return clicked;
}

bool ImGui::UiChipFrame(const char* id, const ImVec2& pos, const ImVec2& size)
{
    SetCursorScreenPos(pos);
    bool clicked = InvisibleButton(id, size);
    bool hovered = IsItemHovered();

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);
    dl->AddRectFilled(pos, max, hovered ? IM_COL32(0x13, 0x1A, 0x28, 0xFF) : UiCol::Bg1, 10.f);
    dl->AddRect(pos, max, hovered ? UiCol::HoverStroke : UiCol::Stroke, 10.f);
    return clicked;
}

void ImGui::UiChipLabel(const ImVec2& pos, const char* text)
{
    AddTrackedText(GetWindowDrawList(), UiFonts::Semi, 12.f, pos, UiCol::Mute, text, 1.5f);
}

bool ImGui::UiEnablePill(const char* id, const ImVec2& pos, const ImVec2& size, bool enabled)
{
    SetCursorScreenPos(pos);
    bool clicked = InvisibleButton(id, size);
    bool hovered = IsItemHovered();

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);
    const ImU32 accent = enabled ? UiCol::Ok : UiCol::Danger;
    const ImU32 fill = enabled ? UiCol::OkFill : UiCol::DangerFill;
    const char* text = enabled ? "ENABLED" : "DISABLED";

    if (enabled) AddGlow(dl, pos, max, accent, 10.f, 8, 0.18f);
    dl->AddRectFilled(pos, max, fill, 10.f);
    dl->AddRect(pos, max, WithAlpha(accent, hovered ? 1.f : 0.9f), 10.f, 0, hovered ? 1.5f : 1.f);

    ImVec2 textSize = CalcTrackedTextSize(UiFonts::Bold, 19.f, text, 1.5f);
    const float contentW = 8.f + 10.f + textSize.x;
    const float x = pos.x + (size.x - contentW) * 0.5f;
    const float cy = pos.y + size.y * 0.5f;
    AddStatusDot(dl, ImVec2(x + 4.f, cy), 4.f, accent, enabled);
    AddTrackedText(dl, UiFonts::Bold, 19.f, ImVec2(x + 18.f, cy - 9.5f), accent, text, 1.5f);
    return clicked;
}

bool ImGui::UiKey(const char* id, const ImVec2& pos, const ImVec2& size, const UiKeyDesc& desc)
{
    SetCursorScreenPos(pos);
    bool clicked = InvisibleButton(id, size);
    bool hovered = !desc.locked && IsItemHovered();

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);
    const float rounding = 9.f;

    ImU32 fill = UiCol::Bg2;
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

    const float dim = desc.inherited ? 0.55f : 1.f;
    if (accent) {
        stroke = WithAlpha(accent, dim);
        strokeW = 1.5f;
        labelCol = WithAlpha(labelCol, desc.inherited ? 0.7f : 1.f);
        if (!desc.inherited) AddGlow(dl, pos, max, accent, rounding, 6, 0.12f);
    }

    if (desc.pressed) {
        fill = fillPressed;
        if (!accent) {
            stroke = UiCol::HoverStroke;
            labelCol = UiCol::Text;
        }
    } else if (hovered && !accent) {
        fill = UiCol::HoverFill;
        stroke = UiCol::Stroke;
    }
    if (desc.locked) labelCol = UiCol::Locked;

    dl->AddRectFilled(pos, max, fill, rounding);
    dl->AddRect(pos, max, stroke, rounding, 0, strokeW);

    ImFont* font = accent ? UiFonts::Bold : UiFonts::Semi;
    float fontSize = multi ? 15.f : 20.f;
    float tracking = multi ? 1.f : 0.f;
    ImVec2 textSize = CalcTrackedTextSize(font, fontSize, desc.label, tracking);
    AddTrackedText(dl, font, fontSize, ImVec2(pos.x + (size.x - textSize.x) * 0.5f, pos.y + (size.y - fontSize) * 0.5f),
                   labelCol, desc.label, tracking);

    if (accent && !desc.inherited) dl->AddCircleFilled(ImVec2(max.x - 9.f, pos.y + 9.f), 2.5f, accent);

    if (desc.selected)
        dl->AddRect(ImVec2(pos.x - 2.f, pos.y - 2.f), ImVec2(max.x + 2.f, max.y + 2.f), WithAlpha(UiCol::Text, 0.7f),
                    rounding + 2.f, 0, 2.f);

    return clicked && !desc.locked;
}

bool ImGui::UiToggle(const char* id, const ImVec2& pos, bool on)
{
    const ImVec2 size = ImVec2(38.f, 20.f);
    SetCursorScreenPos(pos);
    bool clicked = InvisibleButton(id, size);

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);
    if (on) {
        AddGlow(dl, pos, max, UiCol::Spam, 10.f, 5, 0.2f);
        dl->AddRectFilled(pos, max, UiCol::Spam, 10.f);
        dl->AddCircleFilled(ImVec2(pos.x + 28.f, pos.y + 10.f), 7.f, UiCol::Bg0);
    } else {
        dl->AddRectFilled(pos, max, UiCol::Bg0, 10.f);
        dl->AddRect(pos, max, UiCol::Stroke, 10.f);
        dl->AddCircleFilled(ImVec2(pos.x + 10.f, pos.y + 10.f), 7.f, UiCol::Mute);
    }
    return clicked;
}

bool ImGui::UiMenuRow(const char* label, ImU32 dotCol, bool disabled, bool keepOpen)
{
    char id[64];
    snprintf(id, sizeof(id), "##row_%s", label);
    ImVec2 pos = GetCursorScreenPos();
    if (disabled) BeginDisabled();
    bool clicked = Selectable(id, false, keepOpen ? ImGuiSelectableFlags_NoAutoClosePopups : 0, ImVec2(0.f, 24.f));
    if (disabled) EndDisabled();

    ImDrawList* dl = GetWindowDrawList();
    float x = pos.x + 8.f;
    if (dotCol) {
        AddStatusDot(dl, ImVec2(pos.x + 12.f, pos.y + 12.f), 3.f, disabled ? WithAlpha(dotCol, 0.4f) : dotCol, false);
        x = pos.x + 24.f;
    }
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(x, pos.y + 3.f), disabled ? UiCol::Mute : UiCol::Text, label);
    return clicked;
}
