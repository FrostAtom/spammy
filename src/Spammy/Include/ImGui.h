#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#define _USE_MATH_DEFINES
#include <cmath>

namespace ImGui {
void Tip(const char* fmt, ...);

// Flashes alpha every f seconds
ImVec4 FlashColor(float r, float g, float b, float f = 8.f, float min = 0.f, float max = 1.f);

void PushStyleColorTriplet(ImGuiCol idx, ImVec4 col);

inline void PopStyleColorTriplet()
{
    ImGui::PopStyleColor(3);
}

namespace UiCol {
inline constexpr ImU32 Bg0 = IM_COL32(0x0A, 0x0D, 0x13, 0xFF);
inline constexpr ImU32 Bg1 = IM_COL32(0x10, 0x15, 0x1E, 0xFF);
inline constexpr ImU32 Bg2 = IM_COL32(0x15, 0x1B, 0x26, 0xFF);
inline constexpr ImU32 Stroke = IM_COL32(0x22, 0x2B, 0x3A, 0xFF);
inline constexpr ImU32 StrokeSoft = IM_COL32(0x1A, 0x22, 0x30, 0xFF);
inline constexpr ImU32 Text = IM_COL32(0xE8, 0xED, 0xF5, 0xFF);
inline constexpr ImU32 Sub = IM_COL32(0x8A, 0x94, 0xA6, 0xFF);
inline constexpr ImU32 Mute = IM_COL32(0x52, 0x5D, 0x6E, 0xFF);
inline constexpr ImU32 Service = IM_COL32(0x6B, 0x76, 0x88, 0xFF);
inline constexpr ImU32 Locked = IM_COL32(0x3A, 0x44, 0x54, 0xFF);
inline constexpr ImU32 Spam = IM_COL32(0xFF, 0x6B, 0x1A, 0xFF);
inline constexpr ImU32 SpamFill = IM_COL32(0x2A, 0x1D, 0x17, 0xFF);
inline constexpr ImU32 SpamText = IM_COL32(0xFF, 0x8B, 0x47, 0xFF);
inline constexpr ImU32 Speedy = IM_COL32(0x22, 0xD3, 0xEE, 0xFF);
inline constexpr ImU32 SpeedyFill = IM_COL32(0x0F, 0x2A, 0x34, 0xFF);
inline constexpr ImU32 SpeedyText = IM_COL32(0x5C, 0xE1, 0xF2, 0xFF);
inline constexpr ImU32 Ok = IM_COL32(0x2E, 0xE5, 0x8A, 0xFF);
inline constexpr ImU32 OkFill = IM_COL32(0x0D, 0x20, 0x18, 0xFF);
inline constexpr ImU32 Danger = IM_COL32(0xFF, 0x4D, 0x5E, 0xFF);
inline constexpr ImU32 DangerFill = IM_COL32(0x2A, 0x15, 0x19, 0xFF);
inline constexpr ImU32 DangerText = IM_COL32(0xFF, 0x7A, 0x87, 0xFF);
inline constexpr ImU32 PanelTop = IM_COL32(0x11, 0x17, 0x22, 0xFF);
inline constexpr ImU32 PanelBottom = IM_COL32(0x0D, 0x11, 0x19, 0xFF);
inline constexpr ImU32 HoverFill = IM_COL32(0x1A, 0x21, 0x30, 0xFF);
inline constexpr ImU32 HoverStroke = IM_COL32(0x2E, 0x3A, 0x4E, 0xFF);
inline constexpr ImU32 PressFill = IM_COL32(0x20, 0x2B, 0x3C, 0xFF);
inline constexpr ImU32 KeyCap = IM_COL32(0x1E, 0x27, 0x36, 0xFF);
inline constexpr ImU32 KeyCapStroke = IM_COL32(0x2C, 0x37, 0x4A, 0xFF);
inline constexpr ImU32 ModFill = IM_COL32(0x28, 0x34, 0x48, 0xFF);
inline constexpr ImU32 ModStroke = IM_COL32(0x3A, 0x48, 0x60, 0xFF);
inline constexpr ImU32 ModText = IM_COL32(0x6A, 0x78, 0x8E, 0xFF);
} // namespace UiCol

struct UiFonts {
    static ImFont* Semi;
    static ImFont* Bold;
    static ImFont* Mono;
};

enum UiGlyph {
    UiGlyph_Gear,
    UiGlyph_Minimize,
    UiGlyph_Close,
};

enum UiKeyStyle {
    UiKeyStyle_None,
    UiKeyStyle_Spam,
    UiKeyStyle_Speedy,
    UiKeyStyle_Blocked,
};

struct UiKeyDesc {
    const char* label;
    UiKeyStyle style;
    ImU32 tint;
    bool inherited;
    bool pressed;
    bool selected;
    bool locked;
};

void LoadUiFonts();
void LoadUiStyle();

float UiAnim(ImGuiID id, float target, float speed = 14.f, float initial = FLT_MAX);
ImU32 UiMixColor(ImU32 a, ImU32 b, float t);
bool UiBeginPopup(const char* str_id);
void UiEndPopup();

ImVec2 CalcTrackedTextSize(ImFont* font, float size, const char* text, float tracking);
void AddTrackedText(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 col, const char* text,
                    float tracking);
void AddGlow(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 col, float rounding, int spread, float alpha);
void AddLogoMark(ImDrawList* dl, const ImVec2& pos, float size);
void AddStatusDot(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, bool glow);
void AddAccentHairline(ImDrawList* dl, const ImVec2& pos, float width, float height);
void AddPanel(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float rounding);
void AddChevronDown(ImDrawList* dl, const ImVec2& center, ImU32 col, float flip = 0.f);
void AddKeycap(ImDrawList* dl, const ImVec2& min, const ImVec2& max, const char* text, ImU32 textCol);

bool UiGhostButton(const char* id, const ImVec2& pos, float size, UiGlyph glyph);
bool UiChipFrame(const char* id, const ImVec2& pos, const ImVec2& size);
void UiChipLabel(const ImVec2& pos, const char* text);
bool UiEnablePill(const char* id, const ImVec2& pos, const ImVec2& size, bool enabled);
bool UiKey(const char* id, const ImVec2& pos, const ImVec2& size, const UiKeyDesc& desc);
bool UiToggle(const char* id, const ImVec2& pos, bool on);
bool UiStepper(const char* id, const ImVec2& pos, float width, int count, int& value);
bool UiMenuRow(const char* label, ImU32 dotCol = 0, bool disabled = false, bool keepOpen = false);
} // namespace ImGui
