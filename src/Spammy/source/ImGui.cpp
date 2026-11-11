#include "ImGui.h"
#include <boost/container/small_vector.hpp>
#include <concepts>
#include <cstring>
#include <initializer_list>
#include <numbers>
#include <optional>
#include <uni_algo/case.h>

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

// ImVector's constructor only zeroes its fields, it just isn't declared noexcept
// NOLINTBEGIN(bugprone-throwing-static-initialization)
static ImGuiStorage s_animStorage;
static ImVector<int> s_popupContentVtxStart;
// NOLINTEND(bugprone-throwing-static-initialization)
static std::optional<float> s_frameRequestSec;

namespace {
constexpr float kMaxAnimStepSec = 1.f / 30.f;

constexpr UiPalette kLightPalette = {
    .Chassis = IM_COL32(0xE2, 0xDE, 0xD6, 0xFF),
    .Line = IM_COL32(0xCD, 0xC8, 0xBE, 0xFF),
    .Well = IM_COL32(0xD3, 0xCE, 0xC4, 0xFF),
    .WellEdge = IM_COL32(0xBD, 0xB7, 0xAC, 0xFF),
    .Recess = IM_COL32(0xD7, 0xD2, 0xC9, 0xFF),
    .RecessStroke = IM_COL32(0xC7, 0xC1, 0xB6, 0xFF),
    .RecessHover = IM_COL32(0xCF, 0xC9, 0xBF, 0xFF),
    .Panel = IM_COL32(0xF4, 0xF2, 0xED, 0xFF),
    .PanelStroke = IM_COL32(0xD9, 0xD5, 0xCC, 0xFF),
    .Divider = IM_COL32(0xE0, 0xDC, 0xD3, 0xFF),
    .RowHover = IM_COL32(0xEC, 0xE9, 0xE2, 0xFF),
    .KnobRing = IM_COL32(0xE6, 0xE2, 0xDA, 0xFF),

    .Cap = IM_COL32(0xF7, 0xF5, 0xF0, 0xFF),
    .CapLip = IM_COL32(0xC2, 0xBC, 0xB1, 0xFF),
    .CapStroke = IM_COL32(0xE3, 0xDF, 0xD7, 0xFF),
    .CapLocked = IM_COL32(0xCB, 0xC6, 0xBC, 0xFF),

    .Ink = IM_COL32(0x17, 0x16, 0x15, 0xFF),
    .Ink2 = IM_COL32(0x5C, 0x59, 0x52, 0xFF),
    .Ink3 = IM_COL32(0x8E, 0x8A, 0x82, 0xFF),
    .InkInverse = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF),
    .DotOff = IM_COL32(0xB3, 0xAD, 0xA2, 0xFF),

    .Signal = IM_COL32(0xFF, 0x50, 0x00, 0xFF),
    .SignalLip = IM_COL32(0xB0, 0x37, 0x00, 0xFF),
    .Tap = IM_COL32(0x2B, 0x2A, 0x27, 0xFF),
    .TapLip = IM_COL32(0x0E, 0x0E, 0x0D, 0xFF),
    .Block = IM_COL32(0xD9, 0x3A, 0x2B, 0xFF),
    .Same = IM_COL32(0xFC, 0xE3, 0xD4, 0xFF),
    .SameLip = IM_COL32(0xE3, 0xB8, 0x9D, 0xFF),
    .SameInk = IM_COL32(0xB0, 0x4A, 0x12, 0xFF),

    .PausedAuto = IM_COL32(0xE9, 0xD2, 0xC3, 0xFF),
    .PausedAutoLip = IM_COL32(0xCD, 0xB3, 0xA2, 0xFF),
    .PausedAutoInk = IM_COL32(0x9C, 0x85, 0x76, 0xFF),
    .PausedTap = IM_COL32(0xBD, 0xB8, 0xAE, 0xFF),
    .PausedTapLip = IM_COL32(0xA3, 0x9E, 0x94, 0xFF),

    .Tape = IM_COL32(0x1E, 0x1D, 0x1B, 0xFF),
    .TapeInk = IM_COL32(0xF2, 0xEF, 0xE8, 0xFF),
    .TapeInk2 = IM_COL32(0x9C, 0x98, 0x8F, 0xFF),
    .White = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF),
    .Shadow = IM_COL32(0x2A, 0x20, 0x16, 0x38),
};

constexpr UiPalette kDarkPalette = {
    .Chassis = IM_COL32(0x1B, 0x1A, 0x18, 0xFF),
    .Line = IM_COL32(0x32, 0x2F, 0x2B, 0xFF),
    .Well = IM_COL32(0x13, 0x12, 0x11, 0xFF),
    .WellEdge = IM_COL32(0x2B, 0x29, 0x26, 0xFF),
    .Recess = IM_COL32(0x15, 0x14, 0x13, 0xFF),
    .RecessStroke = IM_COL32(0x2D, 0x2B, 0x28, 0xFF),
    .RecessHover = IM_COL32(0x1E, 0x1C, 0x1A, 0xFF),
    .Panel = IM_COL32(0x25, 0x23, 0x21, 0xFF),
    .PanelStroke = IM_COL32(0x36, 0x33, 0x2F, 0xFF),
    .Divider = IM_COL32(0x2E, 0x2B, 0x28, 0xFF),
    .RowHover = IM_COL32(0x2F, 0x2C, 0x29, 0xFF),
    .KnobRing = IM_COL32(0x34, 0x31, 0x2D, 0xFF),

    .Cap = IM_COL32(0x33, 0x31, 0x2D, 0xFF),
    .CapLip = IM_COL32(0x20, 0x1E, 0x1B, 0xFF),
    .CapStroke = IM_COL32(0x40, 0x3D, 0x38, 0xFF),
    .CapLocked = IM_COL32(0x24, 0x22, 0x1F, 0xFF),

    .Ink = IM_COL32(0xED, 0xE9, 0xE1, 0xFF),
    .Ink2 = IM_COL32(0xA9, 0xA4, 0x9A, 0xFF),
    .Ink3 = IM_COL32(0x77, 0x73, 0x6B, 0xFF),
    .InkInverse = IM_COL32(0x1B, 0x1A, 0x18, 0xFF),
    .DotOff = IM_COL32(0x46, 0x43, 0x3E, 0xFF),

    .Signal = IM_COL32(0xFF, 0x50, 0x00, 0xFF),
    .SignalLip = IM_COL32(0x8C, 0x2B, 0x00, 0xFF),
    .Tap = IM_COL32(0xE3, 0xDF, 0xD7, 0xFF),
    .TapLip = IM_COL32(0x8F, 0x8A, 0x80, 0xFF),
    .Block = IM_COL32(0xF0, 0x50, 0x3E, 0xFF),
    .Same = IM_COL32(0x4A, 0x2A, 0x19, 0xFF),
    .SameLip = IM_COL32(0x2A, 0x17, 0x0D, 0xFF),
    .SameInk = IM_COL32(0xFF, 0x9A, 0x63, 0xFF),

    .PausedAuto = IM_COL32(0x3A, 0x2E, 0x27, 0xFF),
    .PausedAutoLip = IM_COL32(0x21, 0x1A, 0x16, 0xFF),
    .PausedAutoInk = IM_COL32(0x8E, 0x79, 0x6B, 0xFF),
    .PausedTap = IM_COL32(0x6C, 0x68, 0x61, 0xFF),
    .PausedTapLip = IM_COL32(0x4A, 0x47, 0x42, 0xFF),

    .Tape = IM_COL32(0xE3, 0xDF, 0xD7, 0xFF),
    .TapeInk = IM_COL32(0x1B, 0x1A, 0x18, 0xFF),
    .TapeInk2 = IM_COL32(0x6E, 0x6A, 0x62, 0xFF),
    .White = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF),
    .Shadow = IM_COL32(0x00, 0x00, 0x00, 0x7A),
};
} // namespace

UiPalette ImGui::UiCol = kLightPalette;

namespace {
constexpr float kSilkFontSize = 9.f, kSilkTracking = 1.6f;
constexpr float kTapeFontSize = 10.f, kTapeTracking = 1.2f, kTapeHeight = 24.f;
constexpr ImVec2 kTapeTextOffset(10.f, 7.f);
constexpr float kTabHeight = 34.f, kTabTextX = 28.f, kTabPadRight = 21.f;
constexpr float kPanelRowHeight = 36.f;
constexpr float kDividerThickness = 1.f;
constexpr float kCapPressDepth = 3.f;
constexpr float kAppearSlide = 8.f;
constexpr float kBrowsePadX = 16.f;

template <class Row, size_t N>
constexpr bool RowsFollowKindOrder(const Row (&rows)[N])
{
    for (size_t i = 0; i < N; i++)
        if ((size_t)rows[i].kind != i) return false;
    return true;
}

enum AnimSlot : std::uint8_t {
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

float StepAnim(float& value, float target, float speed)
{
    const float step = ImMin(GImGui->IO.DeltaTime, kMaxAnimStepSec);
    value += (target - value) * (1.f - expf(-speed * step));
    if (fabsf(target - value) < 0.001f)
        value = target;
    else
        UiRequestFrameIn(0.f);
    return value;
}

float UiAnim(ImGuiID id, float target, float speed)
{
    return StepAnim(*s_animStorage.GetFloatRef(id, target), target, speed);
}

float AppearAnim(ImGuiID id, bool shown, float speed)
{
    const float t = StepAnim(*s_animStorage.GetFloatRef(id, 0.f), shown ? 1.f : 0.f, speed);
    const float left = 1.f - t;
    return 1.f - left * left;
}

struct HitBox {
    bool clicked;
    ImGuiID id;
    bool hovered, active;

    [[nodiscard]] float Anim(AnimSlot slot, bool target, float speed) const
    {
        return UiAnim(AnimId(id, slot), target ? 1.f : 0.f, speed);
    }
    [[nodiscard]] float HoverAnim() const { return Anim(AnimSlot_Hover, hovered, 18.f); }
    [[nodiscard]] float PressAnim() const { return Anim(AnimSlot_Press, active, 28.f); }
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

void AddTapeText(ImDrawList* dl, const ImVec2& tapePos, ImU32 col, const char* text)
{
    AddTrackedText(dl, UiFonts::Bold, kTapeFontSize, tapePos + kTapeTextOffset, col, UiUpperCased(text).c_str(),
                   kTapeTracking);
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

struct CapPaletteFields {
    UiCapKind kind;
    ImU32 UiPalette::*face;
    ImU32 UiPalette::*lip;
    ImU32 UiPalette::*ink;
};
constexpr CapPaletteFields kCapPaletteFields[] = {
    {UiCapKind_Plain, &UiPalette::Cap, &UiPalette::CapLip, &UiPalette::Ink},
    {UiCapKind_Signal, &UiPalette::Signal, &UiPalette::SignalLip, &UiPalette::White},
    {UiCapKind_Tap, &UiPalette::Tap, &UiPalette::TapLip, &UiPalette::InkInverse},
    {UiCapKind_Same, &UiPalette::Same, &UiPalette::SameLip, &UiPalette::SameInk},
    {UiCapKind_Locked, &UiPalette::CapLocked, &UiPalette::CapLip, &UiPalette::Ink},
    {UiCapKind_PausedAuto, &UiPalette::PausedAuto, &UiPalette::PausedAutoLip, &UiPalette::PausedAutoInk},
    {UiCapKind_PausedTap, &UiPalette::PausedTap, &UiPalette::PausedTapLip, &UiPalette::TapeInk},
};
static_assert(std::size(kCapPaletteFields) == UiCapKind_COUNT && RowsFollowKindOrder(kCapPaletteFields));

CapColors CapPalette(UiCapKind kind)
{
    const CapPaletteFields& fields = kCapPaletteFields[kind];
    return {UiCol.*fields.face, UiCol.*fields.lip, kind == UiCapKind_Plain ? UiCol.CapStroke : 0};
}

ImU32 CapInk(UiCapKind kind)
{
    return UiCol.*kCapPaletteFields[kind].ink;
}

ImVec2 DrawCap(ImDrawList* dl, const ImVec2& min, const ImVec2& size, const float radii[UiCorner_COUNT],
               const CapColors& colors, float pressT)
{
    const ImVec2 lip = min + ImVec2(0.f, kUiCapLipDepth);
    const ImVec2 face = min + ImVec2(0.f, kCapPressDepth * pressT);
    AddRoundRectFilled(dl, lip, lip + size, colors.lip, radii);
    AddRoundRectFilled(dl, face, face + size, colors.face, radii);
    if (colors.stroke) AddRoundRect(dl, face, face + size, colors.stroke, radii, 1.f);
    return face;
}

void AddHatchRounded(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 col, float spacing, float thickness,
                     const float radii[UiCorner_COUNT])
{
    const float halfStroke = thickness * 0.5f;
    const ImVec2 a = min + ImVec2(halfStroke, halfStroke);
    const ImVec2 b = max - ImVec2(halfStroke, halfStroke);
    if (b.x <= a.x || b.y <= a.y || spacing <= 0.f) return;
    const ImVec2 mid = (a + b) * 0.5f;
    const float limit = ImMin(b.x - a.x, b.y - a.y) * 0.5f;
    float r[UiCorner_COUNT];
    for (int i = 0; i < UiCorner_COUNT; i++)
        r[i] = ImClamp(radii[i] - halfStroke, 0.f, limit);

    auto clipIntoCornerArc = [&](const ImVec2& end, const ImVec2& toward) -> std::optional<ImVec2> {
        static constexpr UiCorner kCornerByTopThenLeft[2][2] = {{UiCorner_BottomRight, UiCorner_BottomLeft},
                                                                {UiCorner_TopRight, UiCorner_TopLeft}};
        const bool left = end.x < mid.x;
        const bool top = end.y < mid.y;
        const float rad = r[kCornerByTopThenLeft[top][left]];
        if (rad <= 0.f) return end;
        const ImVec2 c(left ? a.x + rad : b.x - rad, top ? a.y + rad : b.y - rad);
        const bool inCornerSquare = (left ? end.x < c.x : end.x > c.x) && (top ? end.y < c.y : end.y > c.y);
        if (!inCornerSquare || ImLengthSqr(end - c) <= rad * rad) return end;
        const ImVec2 d = toward - end;
        const ImVec2 f = end - c;
        const float qa = ImLengthSqr(d);
        const float qb = 2.f * (f.x * d.x + f.y * d.y);
        const float qc = ImLengthSqr(f) - rad * rad;
        const float disc = qb * qb - 4.f * qa * qc;
        if (qa <= 0.f || disc < 0.f) return std::nullopt;
        const float nearestHitT = (-qb - sqrtf(disc)) / (2.f * qa);
        if (nearestHitT < 0.f || nearestHitT > 1.f) return std::nullopt;
        return end + d * nearestHitT;
    };

    auto addLineAlong = [&](float xPlusY) {
        const float x0 = ImMax(a.x, xPlusY - b.y);
        const float x1 = ImMin(b.x, xPlusY - a.y);
        if (x1 - x0 < 0.5f) return;
        const std::optional<ImVec2> p0 = clipIntoCornerArc(ImVec2(x0, xPlusY - x0), ImVec2(x1, xPlusY - x1));
        if (!p0) return;
        const std::optional<ImVec2> p1 = clipIntoCornerArc(ImVec2(x1, xPlusY - x1), *p0);
        if (!p1) return;
        dl->AddLine(*p0, *p1, col, thickness);
    };
    const float xPlusYStep = spacing * std::numbers::sqrt2_v<float>;
    float xPlusY = a.x + a.y + xPlusYStep * 0.5f;
    while (xPlusY < b.x + b.y) {
        addLineAlong(xPlusY);
        xPlusY += xPlusYStep;
    }
}

struct GlyphDesign {
    UiGlyph kind;
    float size;
};
constexpr GlyphDesign kGlyphDesigns[] = {
    {UiGlyph_Gear, 14.f},
    {UiGlyph_Minimize, 12.f},
    {UiGlyph_Close, 12.f},
    {UiGlyph_Plus, 12.f},
    {UiGlyph_Minus, 12.f},
    {UiGlyph_Search, 13.f},
    {UiGlyph_ExternalLink, 9.f},
    {UiGlyph_Power, 28.f},
    {UiGlyph_Padlock, 14.f},
    {UiGlyph_Folder, 14.f},
};
static_assert(std::size(kGlyphDesigns) == UiGlyph_COUNT && RowsFollowKindOrder(kGlyphDesigns));

constexpr float GlyphSize(UiGlyph glyph)
{
    return kGlyphDesigns[glyph].size;
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
    const ImVec2 c = (min + max) * 0.5f;
    const ImVec2 half = (max - min) * 0.5f;
    const float qx = fabsf(p.x - c.x) - (half.x - r);
    const float qy = fabsf(p.y - c.y) - (half.y - r);
    const float ox = ImMax(qx, 0.f);
    const float oy = ImMax(qy, 0.f);
    return sqrtf(ox * ox + oy * oy) + ImMin(ImMax(qx, qy), 0.f) - r;
}

void AddOuterBoxShadow(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float rounding, const ImVec2& offset,
                       float blur, ImU32 col)
{
    constexpr int kCornerSegs = 8;
    constexpr int kPoints = (kCornerSegs + 1) * 4;
    constexpr int kRings = 10;
    constexpr float kRingsBunchingAtEdge = 1.5f;
    const ImVec2 centers[4] = {min + ImVec2(rounding, rounding), ImVec2(max.x - rounding, min.y + rounding),
                               max - ImVec2(rounding, rounding), ImVec2(min.x + rounding, max.y - rounding)};
    ImVec2 dirs[kPoints];
    for (int j = 0; j < kPoints; j++) {
        const int corner = j / (kCornerSegs + 1);
        const int seg = j % (kCornerSegs + 1);
        const float a = IM_PI * (1.f + 0.5f * (float)corner + 0.5f * (float)seg / kCornerSegs);
        dirs[j] = ImVec2(cosf(a), sinf(a));
    }

    const float reachX = blur + fabsf(offset.x);
    const float reachUp = ImMax(blur - offset.y, 2.f);
    const float reachDown = ImMax(blur + offset.y, 2.f);
    const ImVec2 shadowMin = min + offset;
    const ImVec2 shadowMax = max + offset;
    const float sigma = ImMax(blur * 0.5f, 0.5f);

    dl->PushClipRect(min - ImVec2(reachX, reachUp), max + ImVec2(reachX, reachDown), false);
    const ImVec2 uv = dl->_Data->TexUvWhitePixel;
    dl->PrimReserve(kRings * kPoints * 6, (kRings + 1) * kPoints);
    const unsigned base = dl->_VtxCurrentIdx;
    for (int i = 0; i <= kRings; i++) {
        const float t = powf((float)i / kRings, kRingsBunchingAtEdge);
        for (int j = 0; j < kPoints; j++) {
            const ImVec2 d = dirs[j];
            const ImVec2 at = centers[j / (kCornerSegs + 1)];
            const ImVec2 p(at.x + d.x * (rounding + t * reachX),
                           at.y + d.y * (rounding + t * (d.y > 0.f ? reachDown : reachUp)));
            const float dist = RoundRectDistance(p, shadowMin, shadowMax, rounding);
            const float k = i == kRings ? 0.f : 0.5f * std::erfc(dist / (sigma * std::numbers::sqrt2_v<float>));
            dl->PrimWriteVtx(p, uv, UiWithAlpha(col, k));
        }
    }
    for (int i = 0; i < kRings; i++)
        for (int j = 0; j < kPoints; j++) {
            const auto a = (ImDrawIdx)(base + i * kPoints + j);
            const auto b = (ImDrawIdx)(base + i * kPoints + (j + 1) % kPoints);
            const auto c = (ImDrawIdx)(a + kPoints);
            const auto d = (ImDrawIdx)(b + kPoints);
            for (const ImDrawIdx idx : {a, b, d, a, d, c})
                dl->PrimWriteIdx(idx);
        }
    dl->PopClipRect();
}

void PushAppearingPopupStyle(const char* str_id)
{
    const ImGuiID animId = AnimId(GetID(str_id), AnimSlot_Popup);
    float eased = 1.f;
    if (!IsPopupOpen(str_id)) {
        s_animStorage.SetFloat(animId, 0.f);
    } else {
        eased = AppearAnim(animId, true, 18.f);
        ImGuiContext& g = *GImGui;
        if (g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasPos)
            g.NextWindowData.PosVal.y -= kAppearSlide * (1.f - eased);
    }

    PushStyleVar(ImGuiStyleVar_Alpha, eased);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kUiPopupPad, kUiPopupPad));
    // modals take the window rounding and border, popups the popup ones
    PushStyleVar(ImGuiStyleVar_WindowRounding, 14.f);
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
    PushStyleColor(ImGuiCol_WindowBg, UiCol.Panel);
}

void PopPopupStyle()
{
    PopStyleColor();
    PopStyleVar(4);
}

bool KeepPopupStyleWhileOpen(bool open)
{
    if (!open) {
        PopPopupStyle();
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

struct SegmentStrip {
    HitBox hit = {};
    ImVec2 pos, size;
    float segW = 0.f;
    int hovered = -1;

    [[nodiscard]] float CapH() const { return size.y - 9.f; }
    [[nodiscard]] ImVec2 CapMin(float segmentsFromLeft) const
    {
        return {pos.x + segmentsFromLeft * segW + 3.f, pos.y + 3.f};
    }
    [[nodiscard]] ImVec2 CapSize() const { return {segW - 6.f, CapH()}; }
    [[nodiscard]] float LabelCenterY() const { return pos.y + 3.f + CapH() * 0.5f; }
    [[nodiscard]] float HoverAnim(int segment) const
    {
        return UiAnim(AnimId(hit.id, AnimSlot_Hover, segment), hovered == segment ? 1.f : 0.f, 18.f);
    }
    void AddTrack(ImDrawList* dl) const { AddRecess(dl, pos, pos + size, size.y <= 32.f ? 8.f : 9.f); }
};

SegmentStrip PlaceSegmentStrip(const char* id, const ImVec2& pos, const ImVec2& size, int count)
{
    const HitBox hit = PlaceInvisibleButton(id, pos, size);
    const float segW = size.x / (float)count;
    const int hovered = hit.hovered ? ImClamp((int)((GetMousePos().x - pos.x) / segW), 0, count - 1) : -1;
    return {hit, pos, size, segW, hovered};
}

ImU32 SegmentLabelCol(float hoverT, ImU32 capInk, float capUnderText)
{
    return UiMixColor(UiMixColor(UiCol.Ink2, UiCol.Ink, hoverT), capInk, capUnderText);
}

struct ListRow {
    ImVec2 pos;
    float width = 0.f;
    bool clicked = false;
    float hoverT = 0.f;
};

constexpr float kListRowHeight = 30.f;
constexpr float kListRowLabelY = 8.f;

ListRow PlaceListRow(const char* id, float fitWidth, bool enabled)
{
    const ImVec2 pos = GetCursorScreenPos();
    const float width = GetContentRegionAvail().x;
    const int hiddenHighlights =
        PushTransparentColors({ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive});
    const ImGuiSelectableFlags flags = ImGuiSelectableFlags_NoAutoClosePopups | ImGuiSelectableFlags_AllowOverlap |
                                       ImGuiSelectableFlags_SpanAvailWidth |
                                       (enabled ? 0 : ImGuiSelectableFlags_Disabled);
    PushID(id);
    const HitBox hit = LastItemHitBox(Selectable("##row", false, flags, ImVec2(fitWidth, kListRowHeight)));
    PopID();
    PopStyleColor(hiddenHighlights);

    const float hoverT = hit.Anim(AnimSlot_Hover, enabled && hit.hovered, 18.f);
    const float pressT = hit.Anim(AnimSlot_Press, enabled && hit.active, 28.f);
    ImDrawList* dl = GetWindowDrawList();
    if (hoverT > 0.01f)
        dl->AddRectFilled(ImVec2(pos.x - 10.f, pos.y), ImVec2(pos.x + width + 10.f, pos.y + kListRowHeight),
                          UiWithAlpha(UiMixColor(UiCol.RowHover, UiCol.Recess, 0.5f * pressT), hoverT), 6.f);
    return {pos, width, hit.clicked && enabled, hoverT};
}

bool Stepper(const char* id, const ImVec2& pos, const ImVec2& size, const char* value, int& index, int count)
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
} // namespace

void ImGui::UiRequestFrameIn(float seconds)
{
    const float fromNowSec = ImMax(seconds, 0.f);
    s_frameRequestSec = ImMin(s_frameRequestSec.value_or(fromNowSec), fromNowSec);
}

std::optional<float> ImGui::UiTakeFrameRequest()
{
    return std::exchange(s_frameRequestSec, std::nullopt);
}

// SetKeyboardFocusHere takes effect on the next frame
void ImGui::UiFocusNextItem()
{
    SetKeyboardFocusHere();
    UiRequestFrameIn(0.f);
}

void ImGui::Tip(const char* text)
{
    constexpr float kHoverDelaySec = 1.f;
    if (!IsItemHovered()) return;
    const float hoveredSec = GImGui->HoveredIdTimer;
    if (hoveredSec <= kHoverDelaySec) {
        UiRequestFrameIn(kHoverDelaySec - hoveredSec);
        return;
    }
    // a tooltip stays hidden on the frame it appears
    const bool appearsThisFrame = hoveredSec - GImGui->IO.DeltaTime <= kHoverDelaySec;
    if (appearsThisFrame) UiRequestFrameIn(0.f);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 7.f));
    PushStyleVar(ImGuiStyleVar_PopupRounding, 6.f);
    SetTooltip("%s", text);
    PopStyleVar(2);
}

void ImGui::UiToast(const ImVec2& bottomRight, const char* text, bool shown)
{
    constexpr float kHeight = 36.f;
    constexpr float kRounding = 10.f;
    constexpr float kPadX = 14.f;
    constexpr float kDotR = 3.f;
    constexpr float kDotGap = 9.f;
    constexpr float kFontSize = 12.f;
    const float eased = AppearAnim(AnimId(GetID("##toast"), AnimSlot_Popup), shown, 14.f);
    if (eased <= 0.f) return;

    const float textX = kPadX + kDotR * 2.f + kDotGap;
    const float width = textX + UiTextWidth(UiFonts::SemiBold, kFontSize, text) + kPadX;
    const ImVec2 max = bottomRight + ImVec2(0.f, kAppearSlide * (1.f - eased));
    const ImVec2 min = max - ImVec2(width, kHeight);
    const float centerY = min.y + kHeight * 0.5f;
    ImDrawList* dl = GetForegroundDrawList();
    AddOuterBoxShadow(dl, min, max, kRounding, ImVec2(0.f, 8.f), 24.f, UiWithAlpha(UiCol.Shadow, eased));
    dl->AddRectFilled(min, max, UiWithAlpha(UiCol.Panel, eased), kRounding);
    dl->AddRect(min, max, UiWithAlpha(UiCol.PanelStroke, eased), kRounding);
    dl->AddCircleFilled(ImVec2(min.x + kPadX + kDotR, centerY), kDotR, UiWithAlpha(UiCol.Block, eased));
    dl->AddText(UiFonts::SemiBold, kFontSize, ImVec2(min.x + textX, centerY - kFontSize * 0.5f),
                UiWithAlpha(UiCol.Ink, eased), text);
}

ImU32 ImGui::UiFlash(ImU32 col, float periodSec, float minAlpha)
{
    UiRequestFrameIn(0.f);
    const float phase = fmodf((float)GetTime() / periodSec, 1.f);
    const float alpha = minAlpha + (1.f - minAlpha) * fabsf(sinf(2.f * IM_PI * phase));
    return (col & ~IM_COL32_A_MASK) | ((ImU32)IM_F32_TO_INT8_SAT(alpha * GetStyle().Alpha) << IM_COL32_A_SHIFT);
}

ImGui::UiUpperCased::UiUpperCased(std::string_view text)
    : _text(una::cases::to_uppercase_utf8<char, std::pmr::polymorphic_allocator<char>>(text, &_stackThenHeap))
{}

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
    UiCol = dark ? kDarkPalette : kLightPalette;
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
        const auto ca = (float)((a >> shift) & 0xFF);
        const auto cb = (float)((b >> shift) & 0xFF);
        out |= (ImU32)IM_ROUND(ca + (cb - ca) * t) << shift;
    }
    return out;
}

bool ImGui::UiBeginPopup(const char* str_id)
{
    PushAppearingPopupStyle(str_id);
    return KeepPopupStyleWhileOpen(BeginPopup(str_id));
}

bool ImGui::UiBeginModal(const char* str_id)
{
    constexpr ImGuiWindowFlags kModalFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                                             ImGuiWindowFlags_NoSavedSettings;
    PushAppearingPopupStyle(str_id);
    return KeepPopupStyleWhileOpen(BeginPopupModal(str_id, nullptr, kModalFlags));
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
    PopPopupStyle();
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

// Archivo's capitals sit centered in its line box
void ImGui::AddTextCentered(ImDrawList* dl, ImFont* font, float size, const ImVec2& center, ImU32 col,
                            const char* text, float tracking)
{
    const float width = UiTextWidth(font, size, text, tracking);
    AddTrackedText(dl, font, size, ImVec2(center.x - width * 0.5f, center.y - size * 0.5f), col, text, tracking);
}

void ImGui::AddSilk(ImDrawList* dl, const ImVec2& topLeft, const char* text, ImU32 col)
{
    AddTrackedText(dl, UiFonts::SemiBold, kSilkFontSize, topLeft, col, UiUpperCased(text).c_str(), kSilkTracking);
}

float ImGui::SilkWidth(const char* text)
{
    return UiTextWidth(UiFonts::SemiBold, kSilkFontSize, UiUpperCased(text).c_str(), kSilkTracking);
}

void ImGui::AddLabelWithNote(ImDrawList* dl, const ImVec2& pos, ImU32 labelCol, const char* label, const char* note)
{
    dl->AddText(UiFonts::SemiBold, 13.f, pos, labelCol, label);
    dl->AddText(UiFonts::Medium, 10.f, pos + ImVec2(0.f, 16.f), UiCol.Ink3, note);
}

void ImGui::AddDivider(ImDrawList* dl, const ImVec2& pos, float width)
{
    dl->AddRectFilled(pos, pos + ImVec2(width, kDividerThickness), UiCol.Divider);
}

void ImGui::AddWell(ImDrawList* dl, const ImVec2& min, const ImVec2& max)
{
    constexpr float kRounding = 16.f;
    dl->AddRectFilled(min, max, UiCol.Well, kRounding);
    dl->AddLine(ImVec2(min.x + kRounding, min.y + 1.f), ImVec2(max.x - kRounding, min.y + 1.f), UiCol.WellEdge, 2.f);
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
        const float px = size / GlyphSize(UiGlyph_Search);
        const float t = 1.6f * px;
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
        const float t = size * (3.f / 28.f);
        const float r = h - t * 0.5f;
        const float halfGap = 0.62f;
        auto addRoundCap = [&](const ImVec2& p) { dl->AddCircleFilled(p, t * 0.5f, col); };
        const float a0 = -IM_PI * 0.5f + halfGap;
        const float a1 = IM_PI * 1.5f - halfGap;
        dl->PathArcTo(c, r, a0, a1);
        dl->PathStroke(col, 0, t);
        addRoundCap(c + ImVec2(cosf(a0), sinf(a0)) * r);
        addRoundCap(c + ImVec2(cosf(a1), sinf(a1)) * r);
        const ImVec2 top(c.x, c.y - h);
        const ImVec2 bottom(c.x, c.y - h * 0.08f);
        dl->AddLine(top, bottom, col, t);
        addRoundCap(top);
        addRoundCap(bottom);
        break;
    }
    case UiGlyph_Padlock: {
        const float t = size * 0.11f;
        const float bodyHalfW = size * 0.36f;
        const float shackleR = size * 0.22f;
        const float bodyTop = c.y - size * 0.04f;
        const float bottom = c.y + h;
        dl->AddRectFilled(ImVec2(c.x - bodyHalfW, bodyTop), ImVec2(c.x + bodyHalfW, bottom), col, size * 0.12f);
        const float shackleArcY = c.y - h + shackleR + t * 0.5f;
        dl->PathLineTo(ImVec2(c.x - shackleR, c.y));
        dl->PathArcTo(ImVec2(c.x, shackleArcY), shackleR, IM_PI, IM_PI * 2.f);
        dl->PathLineTo(ImVec2(c.x + shackleR, c.y));
        dl->PathStroke(col, 0, t);
        break;
    }
    case UiGlyph_Folder: {
        const float r = size * 0.08f;
        const float top = c.y - size * 0.4f;
        const float bodyTop = c.y - size * 0.24f;
        const float bottom = c.y + size * 0.4f;
        dl->AddRectFilled(ImVec2(c.x - h, top), ImVec2(c.x - h * 0.1f, bodyTop + r), col, r);
        dl->AddRectFilled(ImVec2(c.x - h, bodyTop), ImVec2(c.x + h, bottom), col, r);
        break;
    }
    case UiGlyph_COUNT: break;
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
    constexpr float kPointerHalfW = 5.f;
    constexpr float kPointerH = 5.f;
    constexpr float kSeamOverlap = 0.5f;
    const float width = ImMax(60.f, UiTextWidth(UiFonts::Bold, 11.f, text) + 20.f);
    const ImVec2 max(pointerTip.x + width * 0.5f, pointerTip.y - kPointerH);
    const ImVec2 min(pointerTip.x - width * 0.5f, max.y - 22.f);
    const ImU32 fill = UiWithAlpha(UiCol.Ink, alpha);
    dl->AddRectFilled(min, max, fill, 5.f);
    const float pointerBaseY = max.y - kSeamOverlap;
    // clockwise, or ImGui's anti-aliasing fringe goes inward
    dl->AddTriangleFilled(ImVec2(pointerTip.x - kPointerHalfW, pointerBaseY),
                          ImVec2(pointerTip.x + kPointerHalfW, pointerBaseY), pointerTip, fill);
    AddTextCentered(dl, UiFonts::Bold, 11.f * textScale, (min + max) * 0.5f, UiWithAlpha(UiCol.InkInverse, alpha),
                    text);
}

float ImGui::TapeWidth(const char* text)
{
    return UiTextWidth(UiFonts::Bold, kTapeFontSize, UiUpperCased(text).c_str(), kTapeTracking) +
           kTapeTextOffset.x * 2.f;
}

void ImGui::AddTape(ImDrawList* dl, const ImVec2& pos, const char* text)
{
    dl->AddRectFilled(pos, pos + ImVec2(TapeWidth(text), kTapeHeight), UiCol.Tape, 2.f);
    AddTapeText(dl, pos, UiCol.TapeInk, text);
}

UiCapHit ImGui::UiCapFrame(const char* id, const ImVec2& pos, const ImVec2& size, UiCapKind kind, float rounding)
{
    const HitBox hit = PlaceInvisibleButton(id, pos, size + ImVec2(0.f, kUiCapLipDepth));
    const float hoverT = hit.HoverAnim();
    const float pressT = hit.PressAnim();
    return {hit.clicked, AddCap(GetWindowDrawList(), pos, size, kind, rounding, pressT, hoverT), hoverT};
}

bool ImGui::UiButton(const char* id, const ImVec2& pos, const ImVec2& size, const char* label, UiButtonKind kind)
{
    struct ButtonLook {
        UiButtonKind kind;
        UiCapKind cap;
        ImU32 UiPalette::*ink;
    };
    static constexpr ButtonLook kLooks[] = {
        {UiButtonKind_Primary, UiCapKind_Signal, &UiPalette::White},
        {UiButtonKind_Secondary, UiCapKind_Plain, &UiPalette::Ink},
        {UiButtonKind_Danger, UiCapKind_Plain, &UiPalette::Block},
    };
    static_assert(std::size(kLooks) == UiButtonKind_COUNT && RowsFollowKindOrder(kLooks));
    const ButtonLook& look = kLooks[kind];
    const UiCapHit hit = UiCapFrame(id, pos, size, look.cap, 8.f);
    AddTextCentered(GetWindowDrawList(), UiFonts::Bold, 13.f, hit.face + size * 0.5f, UiCol.*look.ink, label);
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

UiCapHit ImGui::UiBrowseButton(const char* id, const ImVec2& pos, const ImVec2& size, const char* label)
{
    constexpr float kGlyphSize = 14.f;
    constexpr float kGlyphGap = 10.f;
    constexpr float kFontSize = 13.f;
    const UiCapHit hit = UiCapFrame(id, pos, size, UiCapKind_Plain, 8.f);
    ImDrawList* dl = GetWindowDrawList();
    const float centerY = hit.face.y + size.y * 0.5f;
    AddGlyph(dl, ImVec2(hit.face.x + kBrowsePadX + kGlyphSize * 0.5f, centerY), kGlyphSize, UiGlyph_Folder,
             UiMixColor(UiCol.Ink2, UiCol.Signal, hit.hoverT));
    const float labelX = hit.face.x + kBrowsePadX + kGlyphSize + kGlyphGap;
    dl->AddText(UiFonts::Bold, kFontSize, ImVec2(labelX, ImFloor(centerY - kFontSize * 0.5f)), UiCol.Ink, label);
    return hit;
}

void ImGui::AddBrowseHint(ImDrawList* dl, const ImVec2& buttonFace, const ImVec2& buttonSize, const char* hint)
{
    constexpr float kFontSize = 12.f;
    const float x = buttonFace.x + buttonSize.x - kBrowsePadX - UiTextWidth(UiFonts::Medium, kFontSize, hint);
    const float centerY = buttonFace.y + buttonSize.y * 0.5f;
    dl->AddText(UiFonts::Medium, kFontSize, ImVec2(x, ImFloor(centerY - kFontSize * 0.5f)), UiCol.Ink3, hint);
}

bool ImGui::UiToggle(const char* id, const ImVec2& pos, bool on)
{
    constexpr float kTrackR = kUiToggleSize.y * 0.5f;
    constexpr float kKnobR = 10.f;
    constexpr float kKnobTravel = 20.f;
    constexpr float kKnobLipDepth = 2.f;
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

bool ImGui::UiTab(const char* id, const ImVec2& pos, const char* label, bool selected, ImU32 dotCol)
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

    dl->AddCircleFilled(face + ImVec2(16.f, 17.f), 3.f, dotCol);
    dl->AddText(selected ? UiFonts::Bold : UiFonts::SemiBold, 13.f, face + ImVec2(kTabTextX, 9.f),
                selected ? UiCol.Ink : UiMixColor(UiCol.Ink2, UiCol.Ink, hoverT), label);
    return hit.clicked;
}

bool ImGui::UiSegmented(const char* id, const ImVec2& pos, const ImVec2& size, std::span<const UiSegment> segments,
                        int& index)
{
    const int count = (int)segments.size();
    const SegmentStrip strip = PlaceSegmentStrip(id, pos, size, count);
    if (count == 0) return false;
    const HitBox& hit = strip.hit;

    bool changed = false;
    if (hit.clicked && strip.hovered >= 0 && strip.hovered != index) {
        index = strip.hovered;
        changed = true;
    }
    const int chosen = ImClamp(index, 0, count - 1);
    const float slide = UiAnim(AnimId(hit.id, AnimSlot_State), (float)chosen, 20.f);
    const float pressT = hit.Anim(AnimSlot_Press, hit.active && strip.hovered == chosen, 28.f);

    ImDrawList* dl = GetWindowDrawList();
    strip.AddTrack(dl);
    const UiCapKind kind = segments[chosen].chosenKind;
    AddCap(dl, strip.CapMin(slide), strip.CapSize(), kind, 6.f, pressT);

    const float centerY = strip.LabelCenterY();
    for (int i = 0; i < count; i++) {
        const float capUnderText = ImSaturate(1.f - fabsf(slide - (float)i));
        const bool isChosen = i == chosen;
        AddTextCentered(dl, isChosen ? UiFonts::Bold : UiFonts::SemiBold, 12.f,
                        ImVec2(pos.x + strip.segW * ((float)i + 0.5f), centerY + (isChosen ? 3.f * pressT : 0.f)),
                        SegmentLabelCol(strip.HoverAnim(i), CapInk(kind), capUnderText), segments[i].label);
    }
    return changed;
}

bool ImGui::UiSegmentedFlags(const char* id, const ImVec2& pos, const ImVec2& size, const char* noFlagsLabel,
                             std::span<const UiFlagSegment> flagSegments, unsigned& flags)
{
    boost::container::small_vector<UiFlagSegment, 8> segments = {{noFlagsLabel, 0u}};
    segments.insert(segments.end(), flagSegments.begin(), flagSegments.end());
    const int count = (int)segments.size();
    const SegmentStrip strip = PlaceSegmentStrip(id, pos, size, count);
    const HitBox& hit = strip.hit;

    const unsigned before = flags;
    if (hit.clicked && strip.hovered >= 0) {
        const unsigned clickedFlag = segments[strip.hovered].flag;
        flags = clickedFlag ? flags ^ clickedFlag : 0u;
    }

    ImDrawList* dl = GetWindowDrawList();
    strip.AddTrack(dl);
    const float centerY = strip.LabelCenterY();
    for (int i = 0; i < count; i++) {
        const UiFlagSegment& segment = segments[i];
        const bool lit = segment.flag ? (flags & segment.flag) != 0 : flags == 0;
        const float litT = UiAnim(AnimId(hit.id, AnimSlot_State, i), lit ? 1.f : 0.f, 20.f);
        const float hoverT = strip.HoverAnim(i);
        const float pressT =
            UiAnim(AnimId(hit.id, AnimSlot_Press, i), hit.active && strip.hovered == i ? 1.f : 0.f, 28.f);
        if (lit) AddCap(dl, strip.CapMin((float)i), strip.CapSize(), UiCapKind_Plain, 6.f, pressT);

        const float segX = pos.x + strip.segW * (float)i;
        AddTextCentered(dl, lit ? UiFonts::Bold : UiFonts::SemiBold, 11.f,
                        ImVec2(segX + strip.segW * 0.5f, centerY + (lit ? 3.f * pressT : 0.f)),
                        SegmentLabelCol(hoverT, CapInk(UiCapKind_Plain), litT), UiUpperCased(segment.label).c_str(),
                        0.6f);
    }
    return flags != before;
}

bool ImGui::UiPowerButton(const char* id, const ImVec2& center, bool on)
{
    constexpr float r = 37.f;
    const ImVec2 min = center - ImVec2(r, r);
    const ImVec2 size(r * 2.f, r * 2.f);
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
    const ImVec2 a = center + d * 30.f;
    const ImVec2 b = center + d * 10.f;
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

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 face = AddCap(dl, pos, size, look.cap, desc.radii, pressT, block ? 0.f : hoverT);
    if (block) {
        AddHatchRounded(dl, face, face + size, UiCol.Block, 7.f, 1.2f, desc.radii);
        if (hoverT > 0.01f) AddRoundRect(dl, face, face + size, UiWithAlpha(UiCol.Ink2, hoverT), desc.radii, 1.5f);
    }

    const char* label = desc.label ? desc.label : "";
    if (narrow || desc.centered) {
        const float fontSize = narrow ? 9.f : 11.f;
        const float dotR = narrow ? 1.5f : 2.f;
        const float dotGap = narrow ? 5.f : 7.f;
        const float liftForDots = narrow ? 3.f : 5.f;
        const float cx = face.x + size.x * 0.5f;
        const float cy = face.y + size.y * 0.5f - (look.dots ? liftForDots : 0.f);
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

    for (size_t i = 0; i < desc.marks.size(); i++) {
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
    const KeyLook look = GetKeyLook(kind, false);
    AddCap(dl, pos, size, look.cap, kUiSmallKeyRadii);
    if (kind == UiKeyKind_Block) AddHatchRounded(dl, pos, pos + size, UiCol.Block, 6.f, 1.2f, kUiSmallKeyRadii);
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
    const bool changed = Stepper(id, ImVec2(pos.x + width - 112.f, pos.y), ImVec2(112.f, 28.f), value, index, count);
    EndPanelRow(pos, width);
    return changed;
}

bool ImGui::UiListRow(const char* id, const char* label, const char* hint, bool enabled)
{
    const float hintW = UiTextWidth(UiFonts::Medium, 10.5f, hint);
    const ListRow row = PlaceListRow(id, UiTextWidth(UiFonts::Medium, 13.f, label) + (16.f + hintW), enabled);

    ImDrawList* dl = GetWindowDrawList();
    dl->AddText(UiFonts::Medium, 13.f, row.pos + ImVec2(0.f, kListRowLabelY), enabled ? UiCol.Ink : UiCol.Ink3, label);
    const float hintAlpha = enabled ? row.hoverT : 1.f;
    if (hintAlpha > 0.01f)
        dl->AddText(UiFonts::Medium, 10.5f, ImVec2(row.pos.x + row.width - hintW, row.pos.y + 10.f),
                    UiWithAlpha(UiCol.Ink3, hintAlpha), hint);
    return row.clicked;
}

bool ImGui::UiMenuRow(const char* id, const char* label, ImU32 labelCol)
{
    const ListRow row = PlaceListRow(id, UiTextWidth(UiFonts::Medium, 13.f, label), true);
    GetWindowDrawList()->AddText(UiFonts::Medium, 13.f, row.pos + ImVec2(0.f, kListRowLabelY), labelCol, label);
    return row.clicked;
}

void ImGui::UiListNote(const char* text)
{
    const ImVec2 pos = GetCursorScreenPos();
    GetWindowDrawList()->AddText(UiFonts::Medium, 13.f, pos + ImVec2(0.f, kListRowLabelY), UiCol.Ink3, text);
    Dummy(ImVec2(UiTextWidth(UiFonts::Medium, 13.f, text), kListRowHeight));
}

void ImGui::UiDotListRow(const char* id, const char* label, ImU32 dotCol)
{
    constexpr float kLabelX = 18.f;
    const ListRow row = PlaceListRow(id, kLabelX + UiTextWidth(UiFonts::Bold, 13.f, label), true);

    ImDrawList* dl = GetWindowDrawList();
    dl->AddCircleFilled(row.pos + ImVec2(6.f, 15.f), 3.f, dotCol);
    dl->AddText(UiFonts::Bold, 13.f, row.pos + ImVec2(kLabelX, kListRowLabelY), UiCol.Ink, label);
}

void ImGui::UiSectionLabel(const char* text)
{
    constexpr float kSilkLineHeight = 11.f;
    constexpr float kGapBelow = 6.f;
    const ImVec2 pos = GetCursorScreenPos();
    AddSilk(GetWindowDrawList(), pos, text);
    Dummy(ImVec2(SilkWidth(text), kSilkLineHeight + kGapBelow));
}

void ImGui::UiDivider(float width)
{
    constexpr float kMargin = 8.f;
    AddDivider(GetWindowDrawList(), GetCursorScreenPos() + ImVec2(0.f, kMargin), width);
    Dummy(ImVec2(width, kMargin + kDividerThickness + kMargin));
}

bool ImGui::UiSearchField(const char* id, char* buf, size_t bufSize, float width, const char* hint, bool focus)
{
    constexpr float kHeight = 34.f;
    constexpr float kTextX = 34.f;
    constexpr float kFontSize = 13.f;
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
    if (focus) UiFocusNextItem();
    PushID(id);
    const bool edited = InputTextWithHint("##field", hint, buf, bufSize);
    PopID();
    PopStyleColor(hiddenFrame);
    PopStyleVar();
    PopFont();
    return edited;
}
