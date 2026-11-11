#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <array>
#include <boost/container/static_vector.hpp>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// Positions are absolute screen coordinates; a control leaves its hit box as the last item for IsItemHovered()
namespace ImGui {
void Tip(const char* text);
void UiToast(const ImVec2& bottomRight, const char* text, bool shown);

void UiRequestFrameIn(float seconds);
std::optional<float> UiTakeFrameRequest();
void UiFocusNextItem();

ImU32 UiFlash(ImU32 col, float periodSec, float minAlpha);

class UiUpperCased {
    std::array<std::byte, 256> _stack = {};
    std::pmr::monotonic_buffer_resource _stackThenHeap{_stack.data(), _stack.size()};
    std::pmr::string _text;

public:
    explicit UiUpperCased(std::string_view text);
    [[nodiscard]] const char* c_str() const { return _text.c_str(); }
};

struct UiPalette {
    ImU32 Chassis, Line, Well, WellEdge, Recess, RecessStroke, RecessHover, Panel, PanelStroke, Divider, RowHover,
        KnobRing;
    ImU32 Cap, CapLip, CapStroke, CapLocked;
    ImU32 Ink, Ink2, Ink3, InkInverse, DotOff;
    ImU32 Signal, SignalLip, Tap, TapLip, Block, Same, SameLip, SameInk;
    ImU32 PausedAuto, PausedAutoLip, PausedAutoInk, PausedTap, PausedTapLip;
    ImU32 Tape, TapeInk, TapeInk2, White, Shadow;
};

extern UiPalette UiCol;

struct UiFonts {
    static ImFont* Light;
    static ImFont* Medium;
    static ImFont* SemiBold;
    static ImFont* Bold;
    static ImFont* Heavy;
};

enum UiGlyph : std::uint8_t {
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
    UiGlyph_COUNT,
};

enum UiCapKind : std::uint8_t {
    UiCapKind_Plain,
    UiCapKind_Signal,
    UiCapKind_Tap,
    UiCapKind_Same,
    UiCapKind_Locked,
    UiCapKind_PausedAuto,
    UiCapKind_PausedTap,
    UiCapKind_COUNT,
};

enum UiKeyKind : std::uint8_t {
    UiKeyKind_Normal,
    UiKeyKind_Auto,
    UiKeyKind_Tap,
    UiKeyKind_SameAuto,
    UiKeyKind_SameTap,
    UiKeyKind_Block,
    UiKeyKind_Locked,
    UiKeyKind_Fixed,
};

enum UiCorner : std::uint8_t {
    UiCorner_TopLeft,
    UiCorner_TopRight,
    UiCorner_BottomRight,
    UiCorner_BottomLeft,
    UiCorner_COUNT,
};

inline constexpr float kUiKeyRadii[UiCorner_COUNT] = {7.f, 7.f, 7.f, 7.f};
inline constexpr float kUiSmallKeyRadii[UiCorner_COUNT] = {5.f, 5.f, 5.f, 5.f};

struct UiKeyDesc {
    const char* label;
    UiKeyKind kind;
    bool pressed;
    bool paused;
    bool centered;
    boost::container::static_vector<char, 3> marks;
    float radii[UiCorner_COUNT];
};

void LoadUiFonts();
void LoadUiStyle(bool dark);

ImU32 UiMixColor(ImU32 a, ImU32 b, float t);
constexpr ImU32 UiWithAlpha(ImU32 col, float alpha)
{
    const auto a = (ImU32)(((col >> IM_COL32_A_SHIFT) & 0xFF) * alpha);
    return (col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}

inline constexpr float kUiPopupPad = 18.f;
bool UiBeginPopup(const char* str_id);
bool UiBeginModal(const char* str_id);
void UiEndPopup();

float UiTextWidth(ImFont* font, float size, const char* text, float tracking = 0.f);
void AddTrackedText(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 col, const char* text,
                    float tracking);
void AddTextCentered(ImDrawList* dl, ImFont* font, float size, const ImVec2& center, ImU32 col, const char* text,
                     float tracking = 0.f);
void AddSilk(ImDrawList* dl, const ImVec2& topLeft, const char* text, ImU32 col = UiCol.Ink3);
float SilkWidth(const char* text);
void AddLabelWithNote(ImDrawList* dl, const ImVec2& pos, ImU32 labelCol, const char* label, const char* note);
void AddDivider(ImDrawList* dl, const ImVec2& pos, float width);
void AddWell(ImDrawList* dl, const ImVec2& min, const ImVec2& max);

inline constexpr float kUiCapLipDepth = 4.f;
inline constexpr ImVec2 kUiToggleSize(46.f, 26.f);
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
    float hoverT = 0.f;
};
UiCapHit UiCapFrame(const char* id, const ImVec2& pos, const ImVec2& size, UiCapKind kind, float rounding);

enum UiButtonKind : std::uint8_t {
    UiButtonKind_Primary,
    UiButtonKind_Secondary,
    UiButtonKind_Danger,
    UiButtonKind_COUNT,
};
bool UiButton(const char* id, const ImVec2& pos, const ImVec2& size, const char* label, UiButtonKind kind);
bool UiCapIconButton(const char* id, const ImVec2& pos, float size, UiGlyph glyph);
bool UiIconButton(const char* id, const ImVec2& pos, float size, UiGlyph glyph);
bool UiOutlineButton(const char* id, const ImVec2& pos, const char* label);
UiCapHit UiBrowseButton(const char* id, const ImVec2& pos, const ImVec2& size, const char* label);
void AddBrowseHint(ImDrawList* dl, const ImVec2& buttonFace, const ImVec2& buttonSize, const char* hint);
bool UiToggle(const char* id, const ImVec2& pos, bool on);
float TabWidth(const char* label);
bool UiTab(const char* id, const ImVec2& pos, const char* label, bool selected, ImU32 dotCol);
struct UiSegment {
    const char* label;
    UiCapKind chosenKind;
};
bool UiSegmented(const char* id, const ImVec2& pos, const ImVec2& size, std::span<const UiSegment> segments,
                 int& index);
struct UiFlagSegment {
    const char* label;
    unsigned flag;
};
bool UiSegmentedFlags(const char* id, const ImVec2& pos, const ImVec2& size, const char* noFlagsLabel,
                      std::span<const UiFlagSegment> flagSegments, unsigned& flags);
bool UiPowerButton(const char* id, const ImVec2& center, bool on);
bool UiKnob(const char* id, const ImVec2& center, float& t, const char* minLabel, const char* maxLabel, bool dim);
bool UiKey(const char* id, const ImVec2& pos, const ImVec2& size, const UiKeyDesc& desc);
void AddKeySwatch(ImDrawList* dl, const ImVec2& pos, UiKeyKind kind);

bool UiToggleRow(const char* id, const char* label, bool on, float width);
bool UiStepperRow(const char* id, const char* label, const char* value, int& index, int count, float width);
bool UiListRow(const char* id, const char* label, const char* hint, bool enabled);
bool UiMenuRow(const char* id, const char* label, ImU32 labelCol);
void UiListNote(const char* text);
void UiDotListRow(const char* id, const char* label, ImU32 dotCol);
void UiSectionLabel(const char* text);
void UiDivider(float width);
bool UiSearchField(const char* id, char* buf, size_t bufSize, float width, const char* hint, bool focus);
} // namespace ImGui
