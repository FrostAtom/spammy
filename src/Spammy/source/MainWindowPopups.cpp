#include "App.h"
#include "Config.h"
#include "KeyboardLayout.h"
#include "MainWindow.h"
#include <ranges>

using namespace ImGui;
using namespace MainWindowUtil;

namespace {
constexpr float kWindowMargin = 8.f;

constexpr float kKeyMenuW = 340.f;
constexpr float kKeyMenuSegmentH = 32.f;
constexpr float kKeyMenuRowsY = 160.f;
constexpr float kKeyMenuRowGap = 10.f;
constexpr float kKeyMenuRowStep = kKeyMenuSegmentH + kKeyMenuRowGap;
constexpr float kKeyMenuLinkGap = 16.f;
constexpr float kKeyMenuFooterH = 56.f;

constexpr UiSegment kAloneSegments[] = {
    {"Normal", UiCapKind_Plain},
    {"Autofire", UiCapKind_Signal},
    {"One tap", UiCapKind_Tap},
};
constexpr Action kAloneActions[] = {Action_None, Action_Spammy, Action_Speedy};
static_assert(std::size(kAloneActions) == std::size(kAloneSegments));
constexpr UiSegment kModSegments[] = {
    {"Same", UiCapKind_Plain},
    {"Auto", UiCapKind_Signal},
    {"Tap", UiCapKind_Tap},
    {"Block", UiCapKind_Plain},
};
constexpr Action kModActions[] = {Action_None, Action_Spammy, Action_Speedy, Action_Disabled};
static_assert(std::size(kModActions) == std::size(kModSegments));

constexpr unsigned kMenuMods[] = {
    KeyMod_Shift,
    KeyMod_Ctrl,
    KeyMod_Alt,
    KeyMod_Shift | KeyMod_Ctrl,
    KeyMod_Shift | KeyMod_Alt,
    KeyMod_Ctrl | KeyMod_Alt,
    KeyMod_Shift | KeyMod_Ctrl | KeyMod_Alt,
};
constexpr size_t kSingleModCount = 3;
constexpr auto kComboMods = std::span(kMenuMods).subspan<kSingleModCount>();
static_assert(kComboMods.size() == 4, "the fold link reads \"+ 4 more combos\"");

int ActionIndex(std::span<const Action> actions, Action action)
{
    const auto found = std::ranges::find(actions, action);
    IM_ASSERT(found != actions.end());
    return (int)(found - actions.begin());
}

bool MatchesFilter(std::string_view name, std::string_view filter)
{
    return filter.empty() || una::caseless::find_utf8(name, filter);
}

float KeyMenuLinkY(size_t rows)
{
    const float rowsBottom = kKeyMenuRowsY + (float)rows * kKeyMenuRowStep - kKeyMenuRowGap;
    return rowsBottom + kKeyMenuLinkGap;
}

float KeyMenuContentHeight(size_t rows)
{
    return KeyMenuLinkY(rows) + kKeyMenuFooterH;
}
} // namespace

void MainWindow::DrawKeyMenu(const ImVec2& o, Profile& profile)
{
    const size_t rows = _keyMenuShowsCombos ? std::size(kMenuMods) : kSingleModCount;
    const ImVec2 size(kKeyMenuW, KeyMenuContentHeight(rows) + kUiPopupPad * 2.f);
    const ImVec2 lo = o + ImVec2(kWindowMargin, kWindowMargin);
    const ImVec2 hi = o + WindowSize() - ImVec2(kWindowMargin, kWindowMargin) - size;
    constexpr float kGapToKey = 8.f;
    const ImVec2 belowKey(_keyMenuKeyMin.x, _keyMenuKeyMax.y + kGapToKey);
    const ImVec2 aboveKey(_keyMenuKeyMin.x, _keyMenuKeyMin.y - kGapToKey - size.y);
    const bool fitsBelowKey = belowKey.y <= hi.y;
    SetNextWindowPos(ImClamp(fitsBelowKey ? belowKey : aboveKey, lo, hi));
    SetNextWindowSize(ImVec2(size.x, 0.f));
    if (!UiBeginPopup(kPopupKeyMenu)) return;

    Profile::KeyModList_t& slots = profile.keys[_keyMenuVk];
    auto isBound = [&](unsigned mods) { return slots[mods].action != Action_None; };
    if (IsWindowAppearing() && std::ranges::any_of(kComboMods, isBound)) _keyMenuShowsCombos = true;

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 p = GetCursorScreenPos();
    const float w = GetContentRegionAvail().x;
    Keyboard::KeyNameBuf keyBuf;
    const char* keyLabel = KeyLabel(_keyMenuVk, keyBuf);

    UiKeyDesc swatch = {};
    swatch.label = keyLabel;
    swatch.kind = KeyKindOf(slots[KeyMod_None].action);
    swatch.paused = !sApp.IsEnabled();
    std::ranges::copy(kUiKeyRadii, swatch.radii);
    UiKey("##swatch", p, ImVec2(38.f, 36.f), swatch);

    char text[128];
    dl->AddText(UiFonts::Heavy, 18.f, p + ImVec2(52.f, 0.f), UiCol.Ink, FormatInto(text, "Key {}", keyLabel));
    AddSilk(dl, p + ImVec2(52.f, 26.f),
            FormatInto(text, "{} \xC2\xB7 {}", profile.name.c_str(), ModsName(KeyMod_None)));
    AddDivider(dl, p + ImVec2(0.f, 58.f), w);

    AddSilk(dl, p + ImVec2(0.f, 72.f), "pressed alone");
    int alone = ActionIndex(kAloneActions, slots[KeyMod_None].action);
    if (UiSegmented("##alone", p + ImVec2(0.f, 90.f), ImVec2(w, kKeyMenuSegmentH), kAloneSegments, alone))
        SetSetting(slots[KeyMod_None].action, kAloneActions[alone]);

    AddSilk(dl, p + ImVec2(0.f, 142.f), "with a modifier held");
    float widestModsLabelW = 0.f;
    for (const unsigned mods : kMenuMods | std::views::take(rows))
        widestModsLabelW = ImMax(widestModsLabelW, UiTextWidth(UiFonts::Bold, 13.f, ModsName(mods)));
    const float segmentsX = ImMax(_keyMenuShowsCombos ? 96.f : 56.f, ImCeil(widestModsLabelW) + 14.f);
    const ImVec2 segmentSize(w - segmentsX, kKeyMenuSegmentH);
    for (size_t i = 0; i < rows; i++) {
        const unsigned mods = kMenuMods[i];
        const float y = kKeyMenuRowsY + (float)i * kKeyMenuRowStep;
        dl->AddText(UiFonts::Bold, 13.f, p + ImVec2(0.f, y + 8.f), UiCol.Ink, ModsName(mods));
        PushID((int)mods);
        int index = ActionIndex(kModActions, slots[mods].action);
        if (UiSegmented("##mods", p + ImVec2(segmentsX, y), segmentSize, kModSegments, index))
            SetSetting(slots[mods].action, kModActions[index]);
        PopID();
    }

    const float linkY = KeyMenuLinkY(rows);
    const char* link =
        _keyMenuShowsCombos ? "\xE2\x88\x92 fewer combos" : "+ 4 more combos (Shift+Ctrl, Ctrl+Alt \xE2\x80\xA6)";
    SetCursorScreenPos(p + ImVec2(0.f, linkY - 2.f));
    if (InvisibleButton("##combos", ImVec2(UiTextWidth(UiFonts::SemiBold, 12.f, link), 19.f)))
        _keyMenuShowsCombos = !_keyMenuShowsCombos;
    const bool linkHovered = IsItemHovered();
    if (linkHovered) SetMouseCursor(ImGuiMouseCursor_Hand);
    dl->AddText(UiFonts::SemiBold, 12.f, p + ImVec2(0.f, linkY), linkHovered ? UiCol.Ink : UiCol.Ink2, link);

    AddDivider(dl, p + ImVec2(0.f, linkY + 28.f), w);
    dl->AddText(UiFonts::Medium, 10.5f, p + ImVec2(0.f, linkY + 42.f), UiCol.Ink3,
                FormatInto(text, "Combos match exactly: Shift+Ctrl+{} skips the Shift row.", keyLabel));

    SetCursorScreenPos(p);
    Dummy(ImVec2(w, KeyMenuContentHeight(rows)));

    if (IsKeyPressed(ImGuiKey_Escape)) CloseCurrentPopup();
    UiEndPopup();
}

void MainWindow::DrawAppsPopup(const ImVec2& o, Profile& profile)
{
    const float panelW = 300.f;
    SetNextWindowPos(ImVec2(ImMin(_appsPopupPos.x, o.x + WindowSize().x - kWindowMargin - panelW), _appsPopupPos.y));
    SetNextWindowSize(ImVec2(panelW, 0.f));
    if (!UiBeginPopup(kPopupApps)) return;

    const bool appearing = IsWindowAppearing();
    if (appearing) {
        _appsSearch[0] = '\0';
        _appsRunning = ListRunningApps();
    }

    const float w = GetContentRegionAvail().x;
    UiSearchField("##appsearch", _appsSearch, sizeof(_appsSearch), w, "search", appearing);

    SetNextWindowSizeConstraints(ImVec2(0.f, 0.f), ImVec2(FLT_MAX, 420.f));
    BeginChild("##applist", ImVec2(0.f, 0.f), ImGuiChildFlags_AutoResizeY);

    if (!profile.apps.empty()) {
        char caption[64];
        UiSectionLabel(FormatInto(caption, "in {}", profile.name.c_str()));
        std::string deferredUnbind;
        for (const std::string& app : profile.apps) {
            if (!MatchesFilter(app, _appsSearch)) continue;
            PushID(app.c_str());
            SetNextItemAllowOverlap();
            UiDotListRow("##row", app.c_str(), UiCol.Signal);
            const ImVec2 rowMin = GetItemRectMin();
            const ImVec2 rowMax = GetItemRectMax();
            const ImVec2 cursor = GetCursorScreenPos();
            if (UiIconButton("##unbind", ImVec2(rowMax.x - 28.f, (rowMin.y + rowMax.y) * .5f - 12.f), 24.f,
                             UiGlyph_Close))
                deferredUnbind = app;
            SetCursorScreenPos(cursor);
            PopID();
        }
        if (!deferredUnbind.empty()) sConfig.UnbindProfile(profile, deferredUnbind);
        UiDivider(GetContentRegionAvail().x);
    }

    UiSectionLabel("running");
    bool anyShown = false;
    for (const RunningApp& app : _appsRunning) {
        if (profile.apps.contains(app.exeName)) continue;
        if (!MatchesFilter(app.exeName, _appsSearch) && !MatchesFilter(app.friendlyName, _appsSearch)) continue;
        const auto owner = sConfig.FindProfileByApp(app.exeName);
        const bool elsewhere = owner && owner.get() != &profile;
        char ownerHint[64];
        const char* hint = elsewhere ? FormatInto(ownerHint, "in {}", owner->name.c_str()) : "click to add";
        PushID(app.exeName.c_str());
        if (UiListRow("##row", app.exeName.c_str(), hint, !elsewhere)) sConfig.BindProfile(profile, app.exeName);
        PopID();
        anyShown = true;
    }
    if (!anyShown) UiListNote(_appsSearch[0] ? "no matches" : "nothing running");
    EndChild();

    UiDivider(w);
    if (UiBrowseButton("##browse", GetCursorScreenPos(), ImVec2(w, 36.f), "Browse for an .exe file").clicked)
        PostMessageW(Native(), WM_USER_BROWSE_APP, 0, 0);

    if (IsKeyPressed(ImGuiKey_Escape)) CloseCurrentPopup();
    UiEndPopup();
}

void MainWindow::DrawSettingsPopup(const ImVec2& o)
{
    SetNextWindowPos(o + ImVec2(1252.f + _keyboardGrowth.x, 52.f), ImGuiCond_Always, ImVec2(1.f, 0.f));
    SetNextWindowSize(ImVec2(300.f, 0.f));
    if (!UiBeginPopup(kPopupSettings)) return;

    if (IsWindowAppearing()) _autoStart = sApp.IsAutoStartEnabled();

    const float w = GetContentRegionAvail().x;
    auto stepEnum = [w](const char* id, const char* label, auto name, int count, auto& value) {
        int index = (int)value;
        return UiStepperRow(id, label, name(value), index, count, w) &&
               SetSetting(value, (std::remove_reference_t<decltype(value)>)index);
    };

    UiSectionLabel("app");
    if (UiToggleRow("##autostart", kAutoStartLabel, _autoStart, w)) ToggleAutoStart();
    if (UiToggleRow("##sounds", "Sounds on pause", sConfig.soundsEnabled, w)) {
        sConfig.soundsEnabled = !sConfig.soundsEnabled;
        sConfig.MarkDirty();
    }
    stepEnum("##close", "Closing the window", CloseActionName, CloseAction_Count, sConfig.closeAction);
    UiDivider(w);

    UiSectionLabel("drawing");
    stepEnum("##form", "Keyboard", KeyboardFormName, KeyboardForm_Count, sConfig.form);
    stepEnum("##variant", "Key variant", KeyboardVariantName, KeyboardVariant_Count, sConfig.variant);
    stepEnum("##mouse", "Mouse", MouseFormName, MouseForm_Count, sConfig.mouse);
    if (stepEnum("##uisize", "Window size", UiSizeName, UiSize_Count, sConfig.uiSize))
        SetScaleFactor(UiSizeFactor(sConfig.uiSize));
    if (stepEnum("##theme", "Theme", UiThemeName, UiTheme_Count, sConfig.theme)) _themeStale = true;
    UiDivider(w);

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 p = GetCursorScreenPos();
    const float githubW = UiTextWidth(UiFonts::SemiBold, 13.f, "GitHub");
    if (InvisibleButton("##github", ImVec2(githubW + 22.f, 20.f))) LaunchUrl(L"https://github.com/FrostAtom/spammy");
    const bool githubHovered = IsItemHovered();
    if (githubHovered) SetMouseCursor(ImGuiMouseCursor_Hand);
    dl->AddText(UiFonts::SemiBold, 13.f, p + ImVec2(0.f, 2.f), UiCol.Ink, "GitHub");
    AddGlyph(dl, p + ImVec2(githubW + 12.f, 10.f), 10.f, UiGlyph_ExternalLink,
             githubHovered ? UiCol.Ink : UiCol.Ink2);
    AddSilk(dl, ImVec2(p.x + w - SilkWidth(BuildLabel()), p.y + 5.f), BuildLabel());

    if (IsKeyPressed(ImGuiKey_Escape)) CloseCurrentPopup();
    UiEndPopup();
}

void MainWindow::DrawTabMenu(const ImVec2& o)
{
    auto renameOrNotify = [this](const std::shared_ptr<Profile>& profile) {
        const bool renamed = sConfig.RenameProfile(profile, _tabMenuName);
        if (!renamed) Notify("name taken or empty");
        return renamed;
    };
    auto commitRename = [&] {
        _tabMenuRenaming = false;
        if (const auto target = _tabMenuRenameTarget.lock()) renameOrNotify(target);
    };
    const bool menuClosedWhileRenaming = _tabMenuRenaming && !IsPopupOpen(kPopupTabMenu);
    if (menuClosedWhileRenaming) commitRename();

    const float panelW = 220.f;
    SetNextWindowPos(ImVec2(ImMin(_tabMenuPos.x, o.x + WindowSize().x - kWindowMargin - panelW), _tabMenuPos.y));
    SetNextWindowSize(ImVec2(panelW, 0.f));
    if (!UiBeginPopup(kPopupTabMenu)) return;
    if (IsWindowAppearing()) {
        if (_tabMenuRenaming) commitRename();
        _tabMenuRemoveArmed = false;
    }
    if (!_tabMenuProfile || std::ranges::find(sConfig.profiles, _tabMenuProfile) == sConfig.profiles.end()) {
        CloseCurrentPopup();
        UiEndPopup();
        return;
    }

    const float w = GetContentRegionAvail().x;
    UiSectionLabel(_tabMenuProfile->name.c_str());

    bool escaped = false;
    if (_tabMenuRenaming) {
        SetNextItemWidth(w);
        if (std::exchange(_tabMenuFocusesName, false)) UiFocusNextItem();
        const bool enterPressed = InputText("##name", _tabMenuName, sizeof(_tabMenuName),
                                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        const bool leftFieldWithoutEnter = IsItemDeactivated() && !enterPressed;
        if (IsKeyPressed(ImGuiKey_Escape)) {
            _tabMenuRenaming = false;
            escaped = true;
        } else if (enterPressed || leftFieldWithoutEnter) {
            if (renameOrNotify(_tabMenuProfile)) {
                _tabMenuRenaming = false;
                if (enterPressed) CloseCurrentPopup();
            } else {
                _tabMenuFocusesName = !leftFieldWithoutEnter;
            }
        }
    } else if (UiMenuRow("##rename", "Rename", UiCol.Ink)) {
        ImStrncpy(_tabMenuName, _tabMenuProfile->name.c_str(), sizeof(_tabMenuName));
        _tabMenuRenameTarget = _tabMenuProfile;
        _tabMenuRenaming = true;
        _tabMenuFocusesName = true;
    }

    if (UiMenuRow("##addapp", "Add an app\xE2\x80\xA6", UiCol.Ink)) {
        if (_screen != Screen_Editor) CloseAddGame();
        sConfig.SetEditingProfile(_tabMenuProfile);
        _appsPopupPos = _tabMenuPos;
        CloseCurrentPopup();
        RequestPopup(kPopupApps);
    }

    UiDivider(w);
    const char* removeLabel = _tabMenuRemoveArmed ? "Click again to remove" : "Remove game";
    const ImU32 removeCol = _tabMenuRemoveArmed ? UiFlash(UiCol.Block, 1.f, .4f) : UiCol.Block;
    if (UiMenuRow("##remove", removeLabel, removeCol)) {
        if (_tabMenuRemoveArmed) {
            const std::shared_ptr<Profile> profile = std::move(_tabMenuProfile);
            CloseCurrentPopup();
            sApp.DeleteProfile(profile);
            const bool noGamesLeft = sConfig.profiles.empty();
            if (noGamesLeft) {
                OpenAddGame();
                _firstGamePauseKey = 0;
            }
        } else {
            _tabMenuRemoveArmed = true;
        }
    }

    if (!escaped && !_tabMenuRenaming && IsKeyPressed(ImGuiKey_Escape)) CloseCurrentPopup();
    UiEndPopup();
}

void MainWindow::DrawClosePopup(const ImVec2& o)
{
    const float contentW = 312.f;
    SetNextWindowPos(o + WindowSize() * .5f, ImGuiCond_Always, ImVec2(.5f, .5f));
    SetNextWindowSize(ImVec2(contentW + kUiPopupPad * 2.f, 0.f));
    if (!UiBeginModal(kPopupClose)) return;

    if (IsWindowAppearing()) _closeChoiceRemembered = false;

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 p = GetCursorScreenPos();
    const float w = GetContentRegionAvail().x;
    AddSilk(dl, p, "close window");
    dl->AddText(UiFonts::Heavy, 22.f, p + ImVec2(0.f, 18.f), UiCol.Ink, "Hide to tray or exit?");
    dl->AddText(UiFonts::Medium, 13.f, p + ImVec2(0.f, 52.f), UiCol.Ink2,
                "Hidden, Spammy keeps working in the background.");

    const ImVec2 btnSize((w - 12.f) * .5f, 42.f);
    CloseAction chosen = CloseAction_Ask;
    if (UiButton("##hide", p + ImVec2(0.f, 86.f), btnSize, "Hide to tray", UiButtonKind_Primary))
        chosen = CloseAction_Hide;
    if (UiButton("##exit", p + ImVec2(btnSize.x + 12.f, 86.f), btnSize, "Exit", UiButtonKind_Danger))
        chosen = CloseAction_Exit;

    AddDivider(dl, p + ImVec2(0.f, 148.f), w);
    dl->AddText(UiFonts::SemiBold, 13.f, p + ImVec2(0.f, 164.f), UiCol.Ink2, "Remember my choice");
    if (UiToggle("##remember", p + ImVec2(w - kUiToggleSize.x, 160.f), _closeChoiceRemembered))
        _closeChoiceRemembered = !_closeChoiceRemembered;
    SetCursorScreenPos(p);
    Dummy(ImVec2(w, 186.f));

    // modals ignore outside clicks and Escape; AllowWhenBlockedByActiveItem: a press inside isn't counted as outside
    const bool clickedOutside = IsMouseClicked(ImGuiMouseButton_Left) && !IsWindowAppearing() &&
                                !IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (clickedOutside || IsKeyPressed(ImGuiKey_Escape)) CloseCurrentPopup();

    if (chosen != CloseAction_Ask) {
        if (_closeChoiceRemembered) SetSetting(sConfig.closeAction, chosen);
        CloseCurrentPopup();
        if (chosen == CloseAction_Hide)
            Hide();
        else
            Close();
    }
    UiEndPopup();
}
