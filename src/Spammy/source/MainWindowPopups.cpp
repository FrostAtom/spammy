#include "MainWindow.h"
#include "App.h"
#include "Config.h"
#include "KeyboardLayout.h"
#include "Modes.h"
#include "Updater.h"
#include "Utils.h"

using namespace ImGui;
using MainWindowUtil::MatchesFilter;
using MainWindowUtil::ModsName;
using MainWindowUtil::SetSetting;

namespace {
// UiBeginPopup / UiBeginModal padding; popups placed before they are drawn need their height up front
constexpr float kPopupPad = 18.f;
constexpr float kWindowMargin = 8.f;

constexpr float kKeyMenuW = 340.f;
constexpr float kKeyMenuRowsY = 160.f;
constexpr float kKeyMenuRowStep = 42.f;

constexpr const char* kAloneLabels[] = {"Normal", "Autofire", "One tap"};
constexpr Action kAloneActions[] = {Action_None, Action_Spammy, Action_Speedy};
constexpr UiCapKind kAloneKinds[] = {UiCapKind_Plain, UiCapKind_Signal, UiCapKind_Tap};
constexpr const char* kModLabels[] = {"Same", "Auto", "Tap", "Block"};
constexpr Action kModActions[] = {Action_None, Action_Spammy, Action_Speedy, Action_Disabled};
constexpr UiCapKind kModKinds[] = {UiCapKind_Plain, UiCapKind_Signal, UiCapKind_Tap, UiCapKind_Plain};

// the single modifiers first, then the combos hidden behind "+ 4 more combos"
constexpr unsigned kMenuMods[] = {
    KeyMod_Shift,
    KeyMod_Ctrl,
    KeyMod_Alt,
    KeyMod_Shift | KeyMod_Ctrl,
    KeyMod_Shift | KeyMod_Alt,
    KeyMod_Ctrl | KeyMod_Alt,
    KeyMod_Shift | KeyMod_Ctrl | KeyMod_Alt,
};
constexpr size_t kSingleMods = 3;

// an action the control has no segment for (Disabled pressed alone, from old configs) shows as the first one
int ActionIndex(std::span<const Action> actions, Action action)
{
    const auto it = std::ranges::find(actions, action);
    return it != actions.end() ? (int)(it - actions.begin()) : 0;
}

UiKeyKind KeyKindOf(Action action)
{
    switch (action) {
    case Action_Spammy: return UiKeyKind_Auto;
    case Action_Speedy: return UiKeyKind_Tap;
    case Action_Disabled: return UiKeyKind_Block;
    default: return UiKeyKind_Normal;
    }
}

// the key as the editor labels it
const char* KeyLabel(unsigned short vkCode)
{
    switch (vkCode) {
    case VK_LBUTTON: return "LMB";
    case VK_RBUTTON: return "RMB";
    case VK_MBUTTON: return "M3";
    case VK_XBUTTON1: return "M4";
    case VK_XBUTTON2: return "M5";
    }
    for (const KeyboardKey& key : GetKeyboardLayout(sConfig.form, sConfig.variant))
        if (key.vkCode == vkCode && key.name && key.name[0]) return key.name;
    const char* name = Keyboard::GetKeyName(vkCode);
    return name && name[0] ? name : "?";
}

// top of the "+ 4 more combos" link; everything below it has a fixed height
float KeyMenuLinkY(size_t rows)
{
    return kKeyMenuRowsY + (float)rows * kKeyMenuRowStep - 10.f + 16.f;
}

float KeyMenuContentHeight(size_t rows)
{
    return KeyMenuLinkY(rows) + 56.f;
}

void AddDivider(ImDrawList* dl, const ImVec2& pos, float width)
{
    dl->AddLine(pos + ImVec2(0.f, .5f), pos + ImVec2(width, .5f), UiCol::Divider);
}

// a list row like UiListRow's, but the label color is the caller's (the tab menu's red "Remove game")
bool MenuRow(const char* id, const char* label, ImU32 col, float width)
{
    const ImVec2 pos = GetCursorScreenPos();
    const bool clicked = InvisibleButton(id, ImVec2(width, 30.f));
    ImDrawList* dl = GetWindowDrawList();
    if (IsItemHovered())
        dl->AddRectFilled(GetItemRectMin() - ImVec2(10.f, 0.f), GetItemRectMax() + ImVec2(10.f, 0.f), UiCol::RowHover,
                          6.f);
    dl->AddText(UiFonts::Medium, 13.f, pos + ImVec2(0.f, 7.f), col, label);
    return clicked;
}

// RenameProfile trims too, so keeping the current name counts as done rather than as taken
bool RenameGame(const std::shared_ptr<Profile>& profile, std::string_view name)
{
    constexpr std::string_view kSpace = " \t";
    const size_t first = name.find_first_not_of(kSpace);
    if (first == std::string_view::npos) return false;
    name = name.substr(first, name.find_last_not_of(kSpace) - first + 1);
    return name == profile->name || sConfig.RenameProfile(profile, name);
}
} // namespace

void MainWindow::DrawKeyMenu(const ImVec2& o, const std::shared_ptr<Profile>& profile)
{
    const size_t rows = _menuShowCombos ? std::size(kMenuMods) : kSingleMods;
    const ImVec2 size(kKeyMenuW, KeyMenuContentHeight(rows) + kPopupPad * 2.f);
    const ImVec2 lo = o + ImVec2(kWindowMargin, kWindowMargin);
    const ImVec2 hi = o + kWindowSize - ImVec2(kWindowMargin, kWindowMargin) - size;
    // under the key, or above it when the window ends first
    ImVec2 pos(_menuKeyMin.x, _menuKeyMax.y + 8.f);
    if (pos.y > hi.y) pos.y = _menuKeyMin.y - 8.f - size.y;
    SetNextWindowPos(ImClamp(pos, lo, hi));
    SetNextWindowSize(ImVec2(size.x, 0.f));
    if (!UiBeginPopup(kPopupKeyMenu)) return;
    if (!profile || _menuVk >= kKeyboardKeysCount) {
        CloseCurrentPopup();
        UiEndPopup();
        return;
    }

    Profile::KeyModList_t& slots = profile->keys[_menuVk];
    // combos stay folded only while the key has none set, so no binding is ever out of sight
    auto comboBound = [&](unsigned mods) { return slots[mods].action != Action_None; };
    if (IsWindowAppearing() && std::ranges::any_of(kMenuMods | std::views::drop(kSingleMods), comboBound))
        _menuShowCombos = true;

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 p = GetCursorScreenPos();
    const float w = GetContentRegionAvail().x;
    const char* keyLabel = KeyLabel(_menuVk);

    UiKeyDesc swatch = {};
    swatch.label = keyLabel;
    swatch.kind = KeyKindOf(slots[KeyMod_None].action);
    swatch.paused = !sApp.IsEnabled();
    UiKey("##swatch", p, ImVec2(38.f, 36.f), swatch);

    char text[128];
    snprintf(text, sizeof(text), "Key %s", keyLabel);
    dl->AddText(UiFonts::Heavy, 18.f, p + ImVec2(52.f, 0.f), UiCol::Ink, text);
    snprintf(text, sizeof(text), "%s \xC2\xB7 %s", profile->name.c_str(), ModsName(KeyMod_None));
    AddSilk(dl, p + ImVec2(52.f, 26.f), text);
    AddDivider(dl, p + ImVec2(0.f, 58.f), w);

    AddSilk(dl, p + ImVec2(0.f, 72.f), "pressed alone");
    int alone = ActionIndex(kAloneActions, slots[KeyMod_None].action);
    if (UiSegmented("##alone", p + ImVec2(0.f, 90.f), ImVec2(w, 32.f), kAloneLabels, alone, false, kAloneKinds))
        SetSetting(slots[KeyMod_None].action, kAloneActions[alone]);

    AddSilk(dl, p + ImVec2(0.f, 142.f), "with a modifier held");
    // all segmented controls start at one x, past the widest label shown
    float labelW = 0.f;
    for (const unsigned mods : kMenuMods | std::views::take(rows))
        labelW = ImMax(labelW, UiTextWidth(UiFonts::Bold, 13.f, ModsName(mods)));
    const float segX = ImMax(_menuShowCombos ? 96.f : 56.f, ImCeil(labelW) + 14.f);
    for (size_t i = 0; i < rows; i++) {
        const unsigned mods = kMenuMods[i];
        const float y = kKeyMenuRowsY + (float)i * kKeyMenuRowStep;
        dl->AddText(UiFonts::Bold, 13.f, p + ImVec2(0.f, y + 8.f), UiCol::Ink, ModsName(mods));
        PushID((int)mods);
        int index = ActionIndex(kModActions, slots[mods].action);
        if (UiSegmented("##mods", p + ImVec2(segX, y), ImVec2(w - segX, 32.f), kModLabels, index, false, kModKinds))
            SetSetting(slots[mods].action, kModActions[index]);
        PopID();
    }

    const float linkY = KeyMenuLinkY(rows);
    const char* link =
        _menuShowCombos ? "\xE2\x88\x92 fewer combos" : "+ 4 more combos (Shift+Ctrl, Ctrl+Alt \xE2\x80\xA6)";
    SetCursorScreenPos(p + ImVec2(0.f, linkY - 2.f));
    if (InvisibleButton("##combos", ImVec2(UiTextWidth(UiFonts::SemiBold, 12.f, link), 19.f)))
        _menuShowCombos = !_menuShowCombos;
    const bool linkHovered = IsItemHovered();
    if (linkHovered) SetMouseCursor(ImGuiMouseCursor_Hand);
    dl->AddText(UiFonts::SemiBold, 12.f, p + ImVec2(0.f, linkY), linkHovered ? UiCol::Ink : UiCol::Ink2, link);

    AddDivider(dl, p + ImVec2(0.f, linkY + 28.f), w);
    snprintf(text, sizeof(text), "Combos match exactly: Shift+Ctrl+%s skips the Shift row.", keyLabel);
    dl->AddText(UiFonts::Medium, 10.5f, p + ImVec2(0.f, linkY + 42.f), UiCol::Ink3, text);

    SetCursorScreenPos(p);
    Dummy(ImVec2(w, KeyMenuContentHeight(rows)));

    if (IsKeyPressed(ImGuiKey_Escape)) CloseCurrentPopup();
    UiEndPopup();
}

void MainWindow::DrawAppsPopup(const ImVec2& o, const std::shared_ptr<Profile>& profile)
{
    const float panelW = 300.f;
    SetNextWindowPos(
        ImVec2(ImMin(_appsPopupPos.x, o.x + kWindowSize.x - kWindowMargin - panelW), _appsPopupPos.y));
    SetNextWindowSize(ImVec2(panelW, 0.f));
    if (!UiBeginPopup(kPopupApps)) return;
    if (!profile) {
        CloseCurrentPopup();
        UiEndPopup();
        return;
    }

    static char s_search[64] = {};
    static std::vector<RunningApp> s_running;
    const bool appearing = IsWindowAppearing();
    if (appearing) {
        s_search[0] = '\0';
        s_running = ListRunningApps();
    }

    const float w = GetContentRegionAvail().x;
    UiSearchField("##appsearch", s_search, sizeof(s_search), w, "search", appearing);

    SetNextWindowSizeConstraints(ImVec2(0.f, 0.f), ImVec2(FLT_MAX, 420.f));
    BeginChild("##applist", ImVec2(0.f, 0.f), ImGuiChildFlags_AutoResizeY);

    if (!profile->apps.empty()) {
        char caption[64];
        snprintf(caption, sizeof(caption), "in %s", profile->name.c_str());
        UiSectionLabel(caption);
        std::string unbindApp;
        for (const std::string& app : profile->apps) {
            if (!MatchesFilter(app, s_search)) continue;
            PushID(app.c_str());
            // lets the unbind button submitted after the row take the hover and the click
            SetNextItemAllowOverlap();
            UiListRow("##row", app.c_str(), nullptr, false, false, true, UiCol::Signal);
            const ImVec2 rowMin = GetItemRectMin();
            const ImVec2 rowMax = GetItemRectMax();
            const ImVec2 cursor = GetCursorScreenPos();
            if (UiIconButton("##unbind", ImVec2(rowMax.x - 28.f, (rowMin.y + rowMax.y) * .5f - 12.f), 24.f,
                             UiGlyph_Close))
                unbindApp = app;
            SetCursorScreenPos(cursor);
            PopID();
        }
        // after the loop: unbinding erases from the set being walked
        if (!unbindApp.empty()) sConfig.UnbindProfile(profile->name.c_str(), unbindApp.c_str());
        UiDivider(GetContentRegionAvail().x);
    }

    UiSectionLabel("running");
    bool anyShown = false;
    for (const RunningApp& app : s_running) {
        if (profile->apps.contains(app.exe)) continue;
        if (!MatchesFilter(app.exe, s_search) && !MatchesFilter(app.name, s_search)) continue;
        const auto owner = sConfig.FindProfileByApp(app.exe.c_str());
        const bool elsewhere = owner && owner != profile;
        char hint[64] = "click to add";
        if (elsewhere) snprintf(hint, sizeof(hint), "in %s", owner->name.c_str());
        PushID(app.exe.c_str());
        if (UiListRow("##row", app.exe.c_str(), hint, elsewhere, elsewhere))
            sConfig.BindProfile(profile->name.c_str(), app.exe.c_str());
        PopID();
        anyShown = true;
    }
    if (!anyShown) {
        const ImVec2 pos = GetCursorScreenPos();
        GetWindowDrawList()->AddText(UiFonts::Medium, 13.f, pos + ImVec2(0.f, 7.f), UiCol::Ink3,
                                     s_search[0] ? "no matches" : "nothing running");
        Dummy(ImVec2(1.f, 30.f));
    }
    EndChild();

    if (IsKeyPressed(ImGuiKey_Escape)) CloseCurrentPopup();
    UiEndPopup();
}

void MainWindow::DrawSettingsPopup(const ImVec2& o)
{
    SetNextWindowPos(o + ImVec2(1252.f, 52.f), ImGuiCond_Always, ImVec2(1.f, 0.f));
    SetNextWindowSize(ImVec2(300.f, 0.f));
    if (!UiBeginPopup(kPopupSettings)) return;

    // asking the registry every frame is wasteful, and nothing else changes it while the popup is up
    static bool s_autoStart = false;
    if (IsWindowAppearing()) s_autoStart = sApp.IsAutoStartEnabled();

    const float w = GetContentRegionAvail().x;
    auto stepEnum = [w](const char* id, const char* label, auto name, int count, auto& value) {
        int index = (int)value;
        return UiStepperRow(id, label, name(value), index, count, w) &&
               SetSetting(value, (std::remove_reference_t<decltype(value)>)index);
    };

    UiSectionLabel("app");
    if (UiToggleRow("##autostart", "Start with Windows", s_autoStart, w) && sApp.EnableAutoStart(!s_autoStart))
        s_autoStart = !s_autoStart;
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
    UiDivider(w);

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 p = GetCursorScreenPos();
    const float githubW = UiTextWidth(UiFonts::SemiBold, 13.f, "GitHub");
    if (InvisibleButton("##github", ImVec2(githubW + 22.f, 20.f))) LaunchUrl(L"https://github.com/FrostAtom/spammy");
    const bool githubHovered = IsItemHovered();
    if (githubHovered) SetMouseCursor(ImGuiMouseCursor_Hand);
    dl->AddText(UiFonts::SemiBold, 13.f, p + ImVec2(0.f, 2.f), UiCol::Ink, "GitHub");
    AddGlyph(dl, p + ImVec2(githubW + 12.f, 10.f), 10.f, UiGlyph_Arrow, githubHovered ? UiCol::Ink : UiCol::Ink2);
    const char* build = "build " __DATE__;
    AddSilk(dl, ImVec2(p.x + w - SilkWidth(build), p.y + 5.f), build);

    if (IsKeyPressed(ImGuiKey_Escape)) CloseCurrentPopup();
    UiEndPopup();
}

void MainWindow::DrawTabMenu(const ImVec2& o)
{
    static char s_name[kProfileNameMax + 1] = {};
    static std::weak_ptr<Profile> s_renameTarget; // the game being renamed, even once the menu moved to another tab
    static bool s_renaming = false;
    static bool s_focusName = false;
    static bool s_renameFailed = false;
    static bool s_armedRemove = false;

    auto finishRename = [] {
        s_renaming = false;
        if (const auto target = s_renameTarget.lock()) RenameGame(target, s_name);
    };
    // a click outside closes the menu before the field gets to report losing focus: save the way Enter would
    if (s_renaming && !IsPopupOpen(kPopupTabMenu)) finishRename();

    const float panelW = 220.f;
    SetNextWindowPos(ImVec2(ImMin(_tabMenuPos.x, o.x + kWindowSize.x - kWindowMargin - panelW), _tabMenuPos.y));
    SetNextWindowSize(ImVec2(panelW, 0.f));
    if (!UiBeginPopup(kPopupTabMenu)) return;
    if (IsWindowAppearing()) {
        if (s_renaming) finishRename();
        s_renameFailed = false;
        s_armedRemove = false;
    }
    if (!_tabMenuProfile || std::ranges::find(sConfig.profiles, _tabMenuProfile) == sConfig.profiles.end()) {
        CloseCurrentPopup();
        UiEndPopup();
        return;
    }

    const float w = GetContentRegionAvail().x;
    UiSectionLabel(_tabMenuProfile->name.c_str());

    bool escaped = false;
    if (s_renaming) {
        SetNextItemWidth(w);
        if (std::exchange(s_focusName, false)) SetKeyboardFocusHere();
        const bool enter = InputText("##name", s_name, sizeof(s_name),
                                     ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        const bool deactivated = IsItemDeactivated();
        if (IsKeyPressed(ImGuiKey_Escape)) {
            s_renaming = false;
            escaped = true;
        } else if (enter || deactivated) {
            if (RenameGame(_tabMenuProfile, s_name)) {
                s_renaming = false;
                if (enter) CloseCurrentPopup();
            } else {
                s_renameFailed = true;
                s_focusName = enter; // Enter keeps typing; a click elsewhere in the menu goes where it was aimed
            }
        }
        if (s_renaming && s_renameFailed) {
            const ImVec2 pos = GetCursorScreenPos();
            GetWindowDrawList()->AddText(UiFonts::Medium, 10.5f, pos, UiCol::Block, "name taken or empty");
            Dummy(ImVec2(w, 16.f));
        }
    } else if (MenuRow("##rename", "Rename", UiCol::Ink, w)) {
        ImStrncpy(s_name, _tabMenuProfile->name.c_str(), sizeof(s_name));
        s_renameTarget = _tabMenuProfile;
        s_renaming = true;
        s_focusName = true;
        s_renameFailed = false;
    }

    if (MenuRow("##addapp", "Add an app\xE2\x80\xA6", UiCol::Ink, w)) {
        if (_screen != Screen_Editor) CloseAddGame();
        sConfig.SetEditingProfile(_tabMenuProfile->name.c_str());
        _appsPopupPos = _tabMenuPos;
        CloseCurrentPopup();
        // OpenPopup from in here would nest under this popup's id; the request is opened at the root next frame
        RequestPopup(kPopupApps);
    }

    UiDivider(w);
    const char* removeLabel = s_armedRemove ? "Click again to remove" : "Remove game";
    const ImU32 removeCol = s_armedRemove ? UiFlash(UiCol::Block, 1.f, .4f) : UiCol::Block;
    if (MenuRow("##remove", removeLabel, removeCol, w)) {
        if (s_armedRemove) {
            const std::string name = _tabMenuProfile->name;
            CloseCurrentPopup();
            _tabMenuProfile.reset();
            sApp.DeleteProfile(name.c_str());
        } else {
            s_armedRemove = true;
        }
    }

    if (!escaped && !s_renaming && IsKeyPressed(ImGuiKey_Escape)) CloseCurrentPopup();
    UiEndPopup();
}

void MainWindow::DrawClosePopup(const ImVec2& o)
{
    const float contentW = 312.f;
    SetNextWindowPos(o + kWindowSize * .5f, ImGuiCond_Always, ImVec2(.5f, .5f));
    SetNextWindowSize(ImVec2(contentW + kPopupPad * 2.f, 0.f));
    if (!UiBeginModal(kPopupClose)) return;

    static bool s_remember = false;
    if (IsWindowAppearing()) s_remember = false;

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 p = GetCursorScreenPos();
    const float w = GetContentRegionAvail().x;
    AddSilk(dl, p, "close window");
    dl->AddText(UiFonts::Heavy, 22.f, p + ImVec2(0.f, 18.f), UiCol::Ink, "Hide to tray or exit?");
    dl->AddText(UiFonts::Medium, 13.f, p + ImVec2(0.f, 52.f), UiCol::Ink2,
                "Hidden, Spammy keeps working in the background.");

    const ImVec2 btnSize((w - 12.f) * .5f, 42.f);
    CloseAction chosen = CloseAction_Ask;
    if (UiButton("##hide", p + ImVec2(0.f, 86.f), btnSize, "Hide to tray", UiButtonKind_Primary))
        chosen = CloseAction_Hide;
    if (UiButton("##exit", p + ImVec2(btnSize.x + 12.f, 86.f), btnSize, "Exit", UiButtonKind_Danger))
        chosen = CloseAction_Exit;

    AddDivider(dl, p + ImVec2(0.f, 148.f), w);
    dl->AddText(UiFonts::SemiBold, 13.f, p + ImVec2(0.f, 164.f), UiCol::Ink2, "Remember my choice");
    if (UiToggle("##remember", p + ImVec2(w - 46.f, 160.f), s_remember)) s_remember = !s_remember;
    SetCursorScreenPos(p);
    Dummy(ImVec2(w, 186.f));

    // a modal ignores clicks outside and Escape by itself, so dismiss it the same way the other popups go away;
    // AllowWhenBlockedByActiveItem, or pressing any control inside would count as "outside" on the press frame
    const bool clickedOutside = IsMouseClicked(ImGuiMouseButton_Left) && !IsWindowAppearing() &&
                                !IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (clickedOutside || IsKeyPressed(ImGuiKey_Escape)) CloseCurrentPopup();

    if (chosen != CloseAction_Ask) {
        if (s_remember) SetSetting(sConfig.closeAction, chosen);
        CloseCurrentPopup();
        if (chosen == CloseAction_Hide)
            Hide();
        else
            Close();
    }
    UiEndPopup();
}
