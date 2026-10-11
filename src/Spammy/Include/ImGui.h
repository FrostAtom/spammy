#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <boost/container/static_vector.hpp>

// "Hardware" look: a light device panel with physical caps. Every raised control is a face sitting on a darker lip
// 4px below it; pressing sinks the face onto the lip. All positions are absolute screen coordinates, and every
// widget leaves its hit box as the last item, so callers may query IsItemHovered() / IsMouseClicked(right) after it.
namespace ImGui {
void Tip(const char* fmt, ...);

// col with its alpha pulsing between minAlpha and 1
ImU32 UiFlash(ImU32 col, float periodSec, float minAlpha);

namespace UiCol {
// chassis and recesses
inline constexpr ImU32 Chassis = IM_COL32(0xE2, 0xDE, 0xD6, 0xFF);    // window background
inline constexpr ImU32 Line = IM_COL32(0xCD, 0xC8, 0xBE, 0xFF);       // hairlines between areas
inline constexpr ImU32 Well = IM_COL32(0xD3, 0xCE, 0xC4, 0xFF);       // keyboard well, list wells
inline constexpr ImU32 WellEdge = IM_COL32(0xBD, 0xB7, 0xAC, 0xFF);   // 2px inner shade along a well's top edge
inline constexpr ImU32 Recess = IM_COL32(0xD7, 0xD2, 0xC9, 0xFF);     // tracks: segmented, stepper, off tumbler
inline constexpr ImU32 RecessStroke = IM_COL32(0xC7, 0xC1, 0xB6, 0xFF);
inline constexpr ImU32 Panel = IM_COL32(0xF4, 0xF2, 0xED, 0xFF);      // popup panels
inline constexpr ImU32 PanelStroke = IM_COL32(0xD9, 0xD5, 0xCC, 0xFF);
inline constexpr ImU32 Divider = IM_COL32(0xE0, 0xDC, 0xD3, 0xFF);    // separators inside panels
inline constexpr ImU32 RowHover = IM_COL32(0xEC, 0xE9, 0xE2, 0xFF);   // list row hover inside panels
inline constexpr ImU32 KnobRing = IM_COL32(0xE6, 0xE2, 0xDA, 0xFF);
// caps
inline constexpr ImU32 Cap = IM_COL32(0xF7, 0xF5, 0xF0, 0xFF);
inline constexpr ImU32 CapLip = IM_COL32(0xC2, 0xBC, 0xB1, 0xFF);
inline constexpr ImU32 CapStroke = IM_COL32(0xE3, 0xDF, 0xD7, 0xFF);
inline constexpr ImU32 CapLocked = IM_COL32(0xCB, 0xC6, 0xBC, 0xFF);
// ink
inline constexpr ImU32 Ink = IM_COL32(0x17, 0x16, 0x15, 0xFF);
inline constexpr ImU32 Ink2 = IM_COL32(0x5C, 0x59, 0x52, 0xFF);
inline constexpr ImU32 Ink3 = IM_COL32(0x8E, 0x8A, 0x82, 0xFF);
inline constexpr ImU32 DotOff = IM_COL32(0xB3, 0xAD, 0xA2, 0xFF);     // unlit tab dot
// signal: orange always means "Spammy changes this key"
inline constexpr ImU32 Signal = IM_COL32(0xFF, 0x50, 0x00, 0xFF);
inline constexpr ImU32 SignalLip = IM_COL32(0xB0, 0x37, 0x00, 0xFF);
inline constexpr ImU32 Tap = IM_COL32(0x2B, 0x2A, 0x27, 0xFF);
inline constexpr ImU32 TapLip = IM_COL32(0x0E, 0x0E, 0x0D, 0xFF);
inline constexpr ImU32 Block = IM_COL32(0xD9, 0x3A, 0x2B, 0xFF);
inline constexpr ImU32 Same = IM_COL32(0xFC, 0xE3, 0xD4, 0xFF);       // key on a modifier layer acting as with no modifier
inline constexpr ImU32 SameLip = IM_COL32(0xE3, 0xB8, 0x9D, 0xFF);
inline constexpr ImU32 SameInk = IM_COL32(0xB0, 0x4A, 0x12, 0xFF);
// paused: bound keys fade
inline constexpr ImU32 PausedAuto = IM_COL32(0xE9, 0xD2, 0xC3, 0xFF);
inline constexpr ImU32 PausedAutoLip = IM_COL32(0xCD, 0xB3, 0xA2, 0xFF);
inline constexpr ImU32 PausedAutoInk = IM_COL32(0x9C, 0x85, 0x76, 0xFF);
inline constexpr ImU32 PausedTap = IM_COL32(0xBD, 0xB8, 0xAE, 0xFF);
inline constexpr ImU32 PausedTapLip = IM_COL32(0xA3, 0x9E, 0x94, 0xFF);
// tape labels and the cartridge illustration
inline constexpr ImU32 Tape = IM_COL32(0x1E, 0x1D, 0x1B, 0xFF);
inline constexpr ImU32 TapeInk = IM_COL32(0xF2, 0xEF, 0xE8, 0xFF);
inline constexpr ImU32 TapeInk2 = IM_COL32(0x9C, 0x98, 0x8F, 0xFF);
inline constexpr ImU32 White = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
inline constexpr ImU32 Shadow = IM_COL32(0x2A, 0x20, 0x16, 0xFF);
} // namespace UiCol

// Archivo, one face per weight; sizes are picked per call (ImGui 1.92 dynamic fonts)
struct UiFonts {
    static ImFont* Light;    // 300: the big rate numerals
    static ImFont* Medium;   // 500: body text, key labels
    static ImFont* SemiBold; // 600: labels, silkscreen
    static ImFont* Bold;     // 700: emphasized labels, tapes, buttons
    static ImFont* Heavy;    // 800: wordmark, headings
};

enum UiGlyph {
    UiGlyph_Gear,     // settings: a ring with a dot in the middle
    UiGlyph_Minimize,
    UiGlyph_Close,    // an X
    UiGlyph_Plus,
    UiGlyph_Minus,
    UiGlyph_Search,   // magnifier
    UiGlyph_Arrow,    // ↗ external link
    UiGlyph_Power,    // power symbol
    UiGlyph_Padlock,
    UiGlyph_ChevronLeft,
    UiGlyph_ChevronRight,
};

// what a cap's face looks like; a few presets cover every control in the design
enum UiCapKind {
    UiCapKind_Plain,   // Cap face, CapLip, CapStroke outline
    UiCapKind_Signal,  // orange face, SignalLip
    UiCapKind_Tap,     // charcoal face, TapLip
    UiCapKind_Same,    // pale orange face, SameLip
    UiCapKind_Locked,  // CapLocked face, CapLip, no outline
    UiCapKind_PausedAuto,
    UiCapKind_PausedTap,
};

// a keyboard / mouse key as the editor shows it
enum UiKeyKind {
    UiKeyKind_Normal, // unbound
    UiKeyKind_Auto,   // autofire while held: orange, 3 dots
    UiKeyKind_Tap,    // one tap, release ignored: charcoal, 1 dot
    UiKeyKind_SameAuto, // modifier layer falling back to an autofire NoMod binding: pale orange, 3 dots
    UiKeyKind_SameTap,  // same, falling back to one tap: pale orange, 1 dot
    UiKeyKind_Block,  // swallowed: plain cap covered by red 45deg hatching, red label on a small cap-colored patch
    UiKeyKind_Locked, // Win while "Block Win key" is on: CapLocked face, Ink3 label, padlock right of the label
    UiKeyKind_Fixed,  // Shift/Ctrl/Alt/Win (unlocked): looks Normal but has no hover and can't be clicked
};

struct UiKeyDesc {
    const char* label;
    UiKeyKind kind;
    bool pressed;       // physically held: the face sinks 3px
    bool paused;        // spamming is off: Auto/Same keys use the Paused* colors, Tap uses PausedTap, marks go Ink3
    bool centered;      // mouse buttons: label centered (600/11, or 600/9 when the key is under 30px wide)
    // a 15px black circle with a white 700/9 letter per modifier that has its own binding on this key ('S','C','A'),
    // top-right corner of the face, right to left
    boost::container::static_vector<char, 3> marks;
    // per-corner radii (top-left, top-right, bottom-right, bottom-left) for the mouse shell; all 0 = the default
    // r7, or r5 when the key is under 30px wide
    float radii[4] = {};
};

void LoadUiFonts();
void LoadUiStyle();

float UiAnim(ImGuiID id, float target, float speed, float initial = FLT_MAX);
ImU32 UiMixColor(ImU32 a, ImU32 b, float t);
constexpr ImU32 UiWithAlpha(ImU32 col, float alpha)
{
    const ImU32 a = (ImU32)(((col >> IM_COL32_A_SHIFT) & 0xFF) * alpha);
    return (col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}

// popups are Panel-colored, r=14, PanelStroke outline, soft drop shadow (0,14 blur 36, Shadow @0.22), fade+slide in
bool UiBeginPopup(const char* str_id);
bool UiBeginModal(const char* str_id); // dims the window with Chassis @0.6
void UiEndPopup();                     // closes both UiBeginPopup and UiBeginModal

// ---- text
float UiTextWidth(ImFont* font, float size, const char* text, float tracking = 0.f);
ImVec2 CalcTrackedTextSize(ImFont* font, float size, const char* text, float tracking);
void AddTrackedText(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 col, const char* text,
                    float tracking);
// silkscreen section label: SemiBold 9px, tracking 1.6, uppercased here; pos is the text's top-left
void AddSilk(ImDrawList* dl, const ImVec2& pos, const char* text, ImU32 col = UiCol::Ink3);
float SilkWidth(const char* text);

// ---- drawing primitives
// a raised cap: face at min..min+size, lip of the same size 4px lower (drawn first), pressT 0..1 sinks the face 3px.
// Returns the face's actual top-left after the press offset
ImVec2 AddCap(ImDrawList* dl, const ImVec2& min, const ImVec2& size, UiCapKind kind, float rounding, float pressT = 0.f,
              float hoverT = 0.f);
// same with per-corner radii (top-left, top-right, bottom-right, bottom-left)
ImVec2 AddCap(ImDrawList* dl, const ImVec2& min, const ImVec2& size, UiCapKind kind, const float radii[4],
              float pressT = 0.f, float hoverT = 0.f);
void AddRoundRectFilled(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 col, const float radii[4]);
void AddRoundRect(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 col, const float radii[4],
                  float thickness);
void AddHatch(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 col, float spacing, float thickness);
void AddGlyph(ImDrawList* dl, const ImVec2& center, float size, UiGlyph glyph, ImU32 col);
// recessed track (segmented controls, steppers, search field, off tumbler): Recess fill, RecessStroke 1px
void AddRecess(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float rounding);
// the live test badge: black r5 60x22 box (wider for longer text) with a 10x5 pointer under it, white Bold 11 text,
// anchored so the pointer tip touches anchor (the top-center of the key)
void AddRateBadge(ImDrawList* dl, const ImVec2& anchor, const char* text, float alpha, float pop);
// dark Dymo-style label: Tape fill r2, 24 tall, TapeInk Bold 10 uppercase tracking 1.2, 10px side padding
float TapeWidth(const char* text);
void AddTape(ImDrawList* dl, const ImVec2& pos, const char* text);

// ---- controls (all return true when clicked / changed)
// a clickable raised cap of any kind; the caller draws its content on the returned face position
struct UiCapHit {
    bool clicked = false;
    ImVec2 face;
    float hoverT = 0.f, pressT = 0.f;
};
UiCapHit UiCapFrame(const char* id, const ImVec2& pos, const ImVec2& size, UiCapKind kind, float rounding);

enum UiButtonKind {
    UiButtonKind_Primary,   // Signal cap, white Bold 13
    UiButtonKind_Secondary, // Plain cap, Ink Bold 13
    UiButtonKind_Danger,    // Plain cap, Block-colored Bold 13
};
bool UiButton(const char* id, const ImVec2& pos, const ImVec2& size, const char* label, UiButtonKind kind);
// small plain cap with a glyph (the rate -/+, 30x30 r6)
bool UiCapIconButton(const char* id, const ImVec2& pos, float size, UiGlyph glyph);
// flat icon button for the title bar: Ink2 glyph, Recess rounded square behind it on hover, Block glyph for Close
bool UiIconButton(const char* id, const ImVec2& pos, float size, UiGlyph glyph);
// outlined text button ("+ APP"): Ink3 1px outline r2, Ink2 Bold 10 uppercase tracking 1.2, 24 tall
float OutlineButtonWidth(const char* label);
bool UiOutlineButton(const char* id, const ImVec2& pos, const char* label);
// tumbler 46x26: on = Signal track, white 20px knob right; off = Recess track, Plain-cap knob (with a 2px lip) left
bool UiToggle(const char* id, const ImVec2& pos, bool on, bool dim = false);
// game tab, 34 tall: selected = raised Plain cap, Ink Bold 13; others = Recess, Ink2 SemiBold 13, flat.
// A 6px dot sits at x+13: Signal when lit, DotOff otherwise; text starts at x+28
float TabWidth(const char* label);
bool UiTab(const char* id, const ImVec2& pos, const char* label, bool selected, bool lit, bool dim = false);
// segmented slide switch: Recess track; the chosen segment is a small raised cap 3px inset, Ink Bold, others Ink2
// SemiBold. chosenKinds (optional, one per segment) colors the chosen cap: Signal -> white text, Tap -> white text.
// uppercase: labels drawn uppercased at 11px tracking 0.6 (the MODIFIERS switch); otherwise 12px as given.
// Returns true when index changed.
bool UiSegmented(const char* id, const ImVec2& pos, const ImVec2& size, std::span<const char* const> labels, int& index,
                 bool uppercase = false, const UiCapKind* chosenKinds = nullptr, bool dim = false);
// "‹ value ›" stepper, Recess r7: clicking the left or right half steps the index, wrapping is off
bool UiStepper(const char* id, const ImVec2& pos, const ImVec2& size, const char* value, int& index, int count);
// round power button, 74px: on = Signal face with a white power glyph, off = Plain face with an Ink3 glyph
bool UiPowerButton(const char* id, const ImVec2& center, bool on);
// rotary knob, 72px Plain cap with a KnobRing inner circle and a Signal pointer, 21 ticks over 270deg around it
// (every 5th long and 1.6 thick; ticks up to the value Ink2, the rest Ink3), "min"/"max" captions under the ends.
// t is 0..1; dragging up/right or the mouse wheel turns it. Returns true when t changed
bool UiKnob(const char* id, const ImVec2& center, float& t, const char* minLabel, const char* maxLabel, bool dim = false);
// keyboard / mouse key; returns true on left click (never for Fixed/Locked)
bool UiKey(const char* id, const ImVec2& pos, const ImVec2& size, const UiKeyDesc& desc);
// small legend swatch 24x21 r5 drawn like a key of that kind (no label, 3px dots)
void AddKeySwatch(ImDrawList* dl, const ImVec2& pos, UiKeyKind kind);

// ---- panel rows, laid out from the cursor inside popups (width = row width)
// row 36 tall: SemiBold 13 Ink label at the left, tumbler right-aligned
bool UiToggleRow(const char* id, const char* label, bool on, float width);
// row 36 tall: label at the left, a 112x28 stepper right-aligned
bool UiStepperRow(const char* id, const char* label, const char* value, int& index, int count, float width);
// list row 30 tall, RowHover r6 on hover; Medium 13 Ink label (Bold when bold, Ink3 when disabled), optional 6px dot
// at the left (label then starts 18px further), optional right-aligned Ink3 10.5 hint (shown always when
// hintAlways, otherwise only while hovered). Returns true on click (never when disabled)
bool UiListRow(const char* id, const char* label, const char* hint, bool hintAlways, bool disabled, bool bold = false,
               ImU32 dotCol = 0, bool keepOpen = true);
// silkscreen section caption inside a panel, followed by 6px spacing
void UiSectionLabel(const char* text);
void UiDivider(float width); // 1px Divider line with 8px spacing above and below
// recessed search field: magnifier at the left, Ink3 Medium 13 hint, Ink text
bool UiSearchField(const char* id, char* buf, size_t bufSize, float width, const char* hint, bool focus);
} // namespace ImGui
