#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <boost/container/static_vector.hpp>
#include <span>

// Positions are absolute screen coordinates. A control drawn at a given position leaves its hit box as the last item, so
// IsItemHovered() and the like can be queried right after it.
namespace ImGui {
void Tip(const char* fmt, ...);

ImU32 UiFlash(ImU32 col, float periodSec, float minAlpha);

namespace UiCol {
inline constexpr ImU32 Chassis = IM_COL32(0xE2, 0xDE, 0xD6, 0xFF);
inline constexpr ImU32 Line = IM_COL32(0xCD, 0xC8, 0xBE, 0xFF);
inline constexpr ImU32 Well = IM_COL32(0xD3, 0xCE, 0xC4, 0xFF);
inline constexpr ImU32 WellEdge = IM_COL32(0xBD, 0xB7, 0xAC, 0xFF);
inline constexpr ImU32 Recess = IM_COL32(0xD7, 0xD2, 0xC9, 0xFF);
inline constexpr ImU32 RecessStroke = IM_COL32(0xC7, 0xC1, 0xB6, 0xFF);
inline constexpr ImU32 Panel = IM_COL32(0xF4, 0xF2, 0xED, 0xFF);
inline constexpr ImU32 PanelStroke = IM_COL32(0xD9, 0xD5, 0xCC, 0xFF);
inline constexpr ImU32 Divider = IM_COL32(0xE0, 0xDC, 0xD3, 0xFF);
inline constexpr ImU32 RowHover = IM_COL32(0xEC, 0xE9, 0xE2, 0xFF);
inline constexpr ImU32 KnobRing = IM_COL32(0xE6, 0xE2, 0xDA, 0xFF);

inline constexpr ImU32 Cap = IM_COL32(0xF7, 0xF5, 0xF0, 0xFF);
inline constexpr ImU32 CapLip = IM_COL32(0xC2, 0xBC, 0xB1, 0xFF);
inline constexpr ImU32 CapStroke = IM_COL32(0xE3, 0xDF, 0xD7, 0xFF);
inline constexpr ImU32 CapLocked = IM_COL32(0xCB, 0xC6, 0xBC, 0xFF);

inline constexpr ImU32 Ink = IM_COL32(0x17, 0x16, 0x15, 0xFF);
inline constexpr ImU32 Ink2 = IM_COL32(0x5C, 0x59, 0x52, 0xFF);
inline constexpr ImU32 Ink3 = IM_COL32(0x8E, 0x8A, 0x82, 0xFF);
inline constexpr ImU32 DotOff = IM_COL32(0xB3, 0xAD, 0xA2, 0xFF);

inline constexpr ImU32 Signal = IM_COL32(0xFF, 0x50, 0x00, 0xFF);
inline constexpr ImU32 SignalLip = IM_COL32(0xB0, 0x37, 0x00, 0xFF);
inline constexpr ImU32 Tap = IM_COL32(0x2B, 0x2A, 0x27, 0xFF);
inline constexpr ImU32 TapLip = IM_COL32(0x0E, 0x0E, 0x0D, 0xFF);
inline constexpr ImU32 Block = IM_COL32(0xD9, 0x3A, 0x2B, 0xFF);
inline constexpr ImU32 Same = IM_COL32(0xFC, 0xE3, 0xD4, 0xFF);
inline constexpr ImU32 SameLip = IM_COL32(0xE3, 0xB8, 0x9D, 0xFF);
inline constexpr ImU32 SameInk = IM_COL32(0xB0, 0x4A, 0x12, 0xFF);

inline constexpr ImU32 PausedAuto = IM_COL32(0xE9, 0xD2, 0xC3, 0xFF);
inline constexpr ImU32 PausedAutoLip = IM_COL32(0xCD, 0xB3, 0xA2, 0xFF);
inline constexpr ImU32 PausedAutoInk = IM_COL32(0x9C, 0x85, 0x76, 0xFF);
inline constexpr ImU32 PausedTap = IM_COL32(0xBD, 0xB8, 0xAE, 0xFF);
inline constexpr ImU32 PausedTapLip = IM_COL32(0xA3, 0x9E, 0x94, 0xFF);

inline constexpr ImU32 Tape = IM_COL32(0x1E, 0x1D, 0x1B, 0xFF);
inline constexpr ImU32 TapeInk = IM_COL32(0xF2, 0xEF, 0xE8, 0xFF);
inline constexpr ImU32 TapeInk2 = IM_COL32(0x9C, 0x98, 0x8F, 0xFF);
inline constexpr ImU32 White = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
inline constexpr ImU32 Shadow = IM_COL32(0x2A, 0x20, 0x16, 0xFF);
} // namespace UiCol

struct UiFonts {
    static ImFont* Light;
    static ImFont* Medium;
    static ImFont* SemiBold;
    static ImFont* Bold;
    static ImFont* Heavy;
};

enum UiGlyph {
    UiGlyph_Gear,
    UiGlyph_Minimize,
    UiGlyph_Close,
    UiGlyph_Plus,
    UiGlyph_Minus,
    UiGlyph_Search,
    UiGlyph_ExternalLink,
    UiGlyph_Power,
    UiGlyph_Padlock,
    UiGlyph_Folder,
};

enum UiCapKind {
    UiCapKind_Plain,
    UiCapKind_Signal,
    UiCapKind_Tap,
    UiCapKind_Same,
    UiCapKind_Locked,
    UiCapKind_PausedAuto,
    UiCapKind_PausedTap,
};

enum UiKeyKind {
    UiKeyKind_Normal,
    UiKeyKind_Auto,
    UiKeyKind_Tap,
    UiKeyKind_SameAuto,
    UiKeyKind_SameTap,
    UiKeyKind_Block,
    UiKeyKind_Locked,
    UiKeyKind_Fixed,
};

enum UiCorner {
    UiCorner_TopLeft,
    UiCorner_TopRight,
    UiCorner_BottomRight,
    UiCorner_BottomLeft,
    UiCorner_COUNT,
};

struct UiKeyDesc {
    const char* label;
    UiKeyKind kind;
    bool pressed;
    bool paused;
    bool centered;
    boost::container::static_vector<char, 3> marks;
    float radii[UiCorner_COUNT] = {};
};

void LoadUiFonts();
void LoadUiStyle();

ImU32 UiMixColor(ImU32 a, ImU32 b, float t);
constexpr ImU32 UiWithAlpha(ImU32 col, float alpha)
{
    const ImU32 a = (ImU32)(((col >> IM_COL32_A_SHIFT) & 0xFF) * alpha);
    return (col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}

inline constexpr float kUiPopupPad = 18.f;
bool UiBeginPopup(const char* str_id);
bool UiBeginModal(const char* str_id);
void UiEndPopup();

float UiTextWidth(ImFont* font, float size, const char* text, float tracking = 0.f);
void AddTrackedText(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 col, const char* text,
                    float tracking);
void AddSilk(ImDrawList* dl, const ImVec2& topLeft, const char* text, ImU32 col = UiCol::Ink3);
float SilkWidth(const char* text);

inline constexpr float kUiCapLipDepth = 4.f;
inline constexpr float kUiCapPressDepth = 3.f;
inline constexpr ImVec2 kUiToggleSize(46.f, 26.f);
// returns the face's top-left, lowered by the press
ImVec2 AddCap(ImDrawList* dl, const ImVec2& min, const ImVec2& size, UiCapKind kind, float rounding, float pressT = 0.f,
              float hoverT = 0.f);
ImVec2 AddCap(ImDrawList* dl, const ImVec2& min, const ImVec2& size, UiCapKind kind, const float radii[UiCorner_COUNT],
              float pressT = 0.f, float hoverT = 0.f);
void AddGlyph(ImDrawList* dl, const ImVec2& center, float size, UiGlyph glyph, ImU32 col);
void AddRecess(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float rounding);
void AddRateBadge(ImDrawList* dl, const ImVec2& pointerTip, const char* text, float alpha, float textScale);
float TapeWidth(const char* text);
void AddTape(ImDrawList* dl, const ImVec2& pos, const char* text);

struct UiCapHit {
    bool clicked = false;
    ImVec2 face;
    float hoverT = 0.f, pressT = 0.f;
};
UiCapHit UiCapFrame(const char* id, const ImVec2& pos, const ImVec2& size, UiCapKind kind, float rounding);

enum UiButtonKind {
    UiButtonKind_Primary,
    UiButtonKind_Secondary,
    UiButtonKind_Danger,
};
bool UiButton(const char* id, const ImVec2& pos, const ImVec2& size, const char* label, UiButtonKind kind);
bool UiCapIconButton(const char* id, const ImVec2& pos, float size, UiGlyph glyph);
bool UiIconButton(const char* id, const ImVec2& pos, float size, UiGlyph glyph);
bool UiOutlineButton(const char* id, const ImVec2& pos, const char* label);
bool UiBrowseButton(const char* id, const ImVec2& pos, const ImVec2& size, const char* label,
                    const char* hint = nullptr, ImU32 hintCol = UiCol::Ink3);
bool UiToggle(const char* id, const ImVec2& pos, bool on);
float TabWidth(const char* label);
bool UiTab(const char* id, const ImVec2& pos, const char* label, bool selected, bool lit, bool dim);
enum UiLabelCase {
    UiLabelCase_AsIs,
    UiLabelCase_Upper,
};
bool UiSegmented(const char* id, const ImVec2& pos, const ImVec2& size, std::span<const char* const> labels, int& index,
                 UiLabelCase labelCase = UiLabelCase_AsIs, std::span<const UiCapKind> chosenKinds = {});
struct UiFlagSegment {
    const char* label;
    unsigned flag;
};
bool UiSegmentedFlags(const char* id, const ImVec2& pos, const ImVec2& size, const char* noFlagsLabel,
                      std::span<const UiFlagSegment> flagSegments, unsigned& flags,
                      UiLabelCase labelCase = UiLabelCase_AsIs);
bool UiStepper(const char* id, const ImVec2& pos, const ImVec2& size, const char* value, int& index, int count);
bool UiPowerButton(const char* id, const ImVec2& center, bool on);
bool UiKnob(const char* id, const ImVec2& center, float& t, const char* minLabel, const char* maxLabel, bool dim);
bool UiKey(const char* id, const ImVec2& pos, const ImVec2& size, const UiKeyDesc& desc);
void AddKeySwatch(ImDrawList* dl, const ImVec2& pos, UiKeyKind kind);

bool UiToggleRow(const char* id, const char* label, bool on, float width);
bool UiStepperRow(const char* id, const char* label, const char* value, int& index, int count, float width);
enum UiListRowFlags_ {
    UiListRowFlags_None = 0,
    UiListRowFlags_HintAlways = 1 << 0,
    UiListRowFlags_Disabled = 1 << 1,
    UiListRowFlags_Bold = 1 << 2,
};
using UiListRowFlags = int;
bool UiListRow(const char* id, const char* label, const char* hint, UiListRowFlags rowFlags = UiListRowFlags_None,
               ImU32 dotCol = 0);
void UiSectionLabel(const char* text);
void UiDivider(float width);
bool UiSearchField(const char* id, char* buf, size_t bufSize, float width, const char* hint, bool focus);
} // namespace ImGui
