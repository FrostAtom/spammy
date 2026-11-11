#include "MainWindow.h"
#include "App.h"
#include "Config.h"
#include "KeyboardLayout.h"
#include "Modes.h"
#include "Resources.h"
#include "Text.h"
#include "Updater.h"
#include <cctype>
#include <fmt/ranges.h>

using namespace ImGui;
using namespace MainWindowUtil;

namespace {
template <class WidthOf>
FittedText ElideToFit(const char* text, float maxW, WidthOf&& widthOf)
{
    static constexpr char kEllipsis[] = "\xE2\x80\xA6";
    FittedText fitted;
    if (widthOf(text) <= maxW) {
        fitted.fitting = text;
        return fitted;
    }
    auto& elided = fitted.elided;
    size_t kept = strlen(text);
    elided.reserve(kept + std::size(kEllipsis));
    elided.assign(text, text + kept);
    while (kept) {
        kept--;
        while (kept && IsUtf8ContinuationByte(elided[kept]))
            kept--;
        while (kept && elided[kept - 1] == ' ')
            kept--;
        elided.resize(kept);
        elided.insert(elided.end(), std::begin(kEllipsis), std::end(kEllipsis));
        if (widthOf(elided.data()) <= maxW) return fitted;
    }
    elided.assign(std::begin(kEllipsis), std::end(kEllipsis));
    return fitted;
}

bool SystemAppsUseDarkTheme()
{
    DWORD appsUseLightTheme = 1;
    DWORD size = sizeof(appsUseLightTheme);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &appsUseLightTheme, &size);
    return appsUseLightTheme == 0;
}

constexpr float kTabsX = 168.f;
constexpr float kTabsMaxX = 1000.f;
constexpr float kTabY = 11.f;
constexpr float kTabGap = 8.f;
constexpr float kAddTabSize = 34.f;
constexpr float kBuildLabelRightX = 1148.f;

constexpr ImVec2 kKeyGap(6.f, 6.f + kUiCapLipDepth);
constexpr ImVec2 kEmptyAfterLastKey(kKeyGap.x, kKeyGap.y - kUiCapLipDepth);
constexpr float kMinKeyUnit = 60.f;

struct KeyboardBox {
    ImVec2 wellMin, wellMax;
    ImVec2 areaMin, areaMax;
    ImVec2 mouseOrigin;
};

KeyboardBox KeyboardBoxAt(const ImVec2& o, const ImVec2& growth, bool withMouse)
{
    KeyboardBox box;
    box.wellMin = o + ImVec2(24.f, 116.f);
    box.wellMax = o + ImVec2(1256.f, 532.f) + growth;
    box.mouseOrigin = o + ImVec2(1072.f + growth.x, 214.f + growth.y * .5f);
    box.areaMin = o + ImVec2(48.f, 140.f);
    box.areaMax = ImVec2(withMouse ? box.mouseOrigin.x - 70.f : box.wellMax.x - 24.f, box.wellMax.y - 28.f);
    return box;
}

ImVec2 KeyboardGrid(std::span<const KeyboardKey> layout)
{
    ImVec2 grid(1.f, 1.f);
    for (const KeyboardKey& key : layout)
        grid = ImMax(grid, ImVec2(key.x + key.w, key.y + key.h));
    return grid;
}

ImVec2 WindowGrowthForMinKeyUnit()
{
    const std::span<const KeyboardKey> layout = GetKeyboardLayout(sConfig.form, sConfig.variant);
    const ImVec2 boardAtMinKeyUnit = KeyboardGrid(layout) * kMinKeyUnit - kEmptyAfterLastKey;
    const KeyboardBox base = KeyboardBoxAt(ImVec2(0.f, 0.f), ImVec2(0.f, 0.f), sConfig.mouse != MouseForm_Off);
    return ImMax(boardAtMinKeyUnit - (base.areaMax - base.areaMin), ImVec2(0.f, 0.f));
}

constexpr UiFlagSegment kModifierSegments[] = {{"Shift", KeyMod_Shift}, {"Ctrl", KeyMod_Ctrl}, {"Alt", KeyMod_Alt}};

constexpr float kMouseLeftRadii[UiCorner_COUNT] = {28.f, 4.f, 4.f, 4.f};
constexpr float kMouseRightRadii[UiCorner_COUNT] = {4.f, 28.f, 4.f, 4.f};
constexpr float kMouseBodyRadii[UiCorner_COUNT] = {6.f, 6.f, 40.f, 40.f};

struct PressBadge {
    ImVec2 anchor;
    unsigned rate = 0;
    DWORD age = 0;
};
using PressBadges = boost::container::small_vector<PressBadge, 32>;

struct ActionKeyKinds {
    Action action;
    UiKeyKind own, inherited;
};
constexpr ActionKeyKinds kActionKeyKinds[] = {
    {Action_Spammy, UiKeyKind_Auto, UiKeyKind_SameAuto},
    {Action_Speedy, UiKeyKind_Tap, UiKeyKind_SameTap},
    {Action_Disabled, UiKeyKind_Block, UiKeyKind_Normal},
};
constexpr ActionKeyKinds kUnboundKeyKinds = {Action_None, UiKeyKind_Normal, UiKeyKind_Normal};

const ActionKeyKinds& KeyKindsOf(Action action)
{
    const auto* kinds = std::ranges::find(kActionKeyKinds, action, &ActionKeyKinds::action);
    return kinds != std::end(kActionKeyKinds) ? *kinds : kUnboundKeyKinds;
}

UiKeyKind KeyKindOnLayer(const Profile& profile, unsigned vkCode, unsigned mods)
{
    const bool winKey = vkCode == VK_LWIN || vkCode == VK_RWIN;
    if (winKey && profile.disableWin) return UiKeyKind_Locked;
    if (!IsBindableKey(vkCode)) return UiKeyKind_Fixed;
    const Profile::KeyModList_t& slots = profile.keys[vkCode];
    const Action ownAction = slots[mods].action;
    if (mods == KeyMod_None || ownAction != Action_None) return KeyKindsOf(ownAction).own;
    return KeyKindsOf(slots[KeyMod_None].action).inherited;
}

Action NextKeyAction(Action action, unsigned mods)
{
    if (mods == KeyMod_None) return action == Action_Spammy ? Action_None : Action_Spammy;
    switch (action) {
    case Action_None: return Action_Spammy;
    case Action_Spammy: return Action_Speedy;
    case Action_Speedy: return Action_Disabled;
    default: return Action_None;
    }
}

struct MouseButtonLabelOf {
    unsigned short vkCode;
    const char* label;
};
constexpr MouseButtonLabelOf kMouseButtonLabels[] = {
    {VK_LBUTTON, "LMB"},
    {VK_RBUTTON, "RMB"},
    {VK_MBUTTON, "M3"},
    {VK_XBUTTON1, "M4"},
    {VK_XBUTTON2, "M5"},
};

const char* MouseButtonLabel(unsigned short vkCode)
{
    const auto* button = std::ranges::find(kMouseButtonLabels, vkCode, &MouseButtonLabelOf::vkCode);
    return button != std::end(kMouseButtonLabels) ? button->label : nullptr;
}

using TabWidths = boost::container::small_vector<float, 16>;

float TabWidthCap(std::span<const float> widths, float tabsMaxX)
{
    constexpr float kNoCap = FLT_MAX;
    float avail = tabsMaxX - kTabsX - kAddTabSize - kTabGap * (float)widths.size();
    if (std::ranges::fold_left(widths, 0.f, std::plus()) <= avail) return kNoCap;

    TabWidths narrowestFirst(widths.begin(), widths.end());
    std::ranges::sort(narrowestFirst);
    for (size_t i = 0; i < narrowestFirst.size(); i++) {
        const float evenShareOfRest = avail / (float)(narrowestFirst.size() - i);
        if (narrowestFirst[i] > evenShareOfRest) return ImMax(evenShareOfRest, 0.f);
        avail -= narrowestFirst[i];
    }
    return kNoCap;
}

void DrawPressBadges(ImDrawList* dl, const PressBadges& badges)
{
    const bool badgesPopAndFadeWithAge = !badges.empty();
    if (badgesPopAndFadeWithAge) UiRequestFrameIn(0.f);
    for (const PressBadge& badge : badges) {
        char buf[16];
        const float pop = badge.age < 120 ? 1.3f - .3f * (badge.age / 120.f) : 1.f;
        const float alpha = badge.age > 700 ? ImMax(0.f, 1.f - (badge.age - 700) / 300.f) : 1.f;
        AddRateBadge(dl, badge.anchor, FormatInto(buf, "{}/s", badge.rate), alpha, pop);
    }
}

bool WheelBar(ImDrawList* dl, const char* id, const ImVec2& pos, const ImVec2& size, bool pressed)
{
    SetCursorScreenPos(pos);
    const bool clicked = InvisibleButton(id, size);
    const ImVec2 sink(0.f, pressed ? 3.f : 0.f);
    if (IsItemHovered())
        dl->AddRect(pos + sink - ImVec2(2.f, 2.f), pos + sink + size + ImVec2(2.f, 2.f), UiCol.Ink2, 6.f, 0, 1.5f);
    dl->AddRectFilled(pos + sink, pos + sink + size, UiCol.Tap, 5.f);
    return clicked;
}

// __DATE__ space-pads one-digit days: "Oct  1 2026"
void SqueezeSpaces(char* text)
{
    const auto bothSpaces = [](char a, char b) { return a == ' ' && b == ' '; };
    *std::unique(text, text + strlen(text), bothSpaces) = '\0';
}

// the test ImGui's BeginPopupContextItem uses
bool IsItemContextClicked()
{
    return IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && IsMouseReleased(ImGuiMouseButton_Right);
}
} // namespace

struct MainWindow::KeyFrame {
    Profile* profile;
    unsigned layerMods;
    unsigned heldMods;
    DWORD nowTicks;
    bool wndFocused;
    bool paused;
    bool clickClosesPopup;
    PressBadges badges;
};

MainWindow::~MainWindow()
{
    if (_pausedIcon) DestroyIcon(_pausedIcon);
}

bool MainWindow::Setup()
{
    if (HWND running = FindWindowW(L"" APP_NAME, L"" APP_NAME)) {
        PostMessageW(running, WM_USER_FOCUS, NULL, NULL);
        return false;
    }

    if (const ErrorCode ec = Initialize(); ec != ErrorCode_OK) {
        char msg[256];
        FormatInto(msg, "{} (0x{:08X})", FormatError(ec), (unsigned long)LastInitError());
        MessageBoxA(nullptr, msg, APP_NAME, MB_ICONERROR | MB_OK);
        return false;
    }
    LoadUiFonts();

    HICON icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_ICON1));
    SetIcon(icon);
    _pausedIcon = CreateGrayscaleIcon(icon);
    _keyboardGrowth = WindowGrowthForMinKeyUnit();
    SetSize({(int)WindowSize().x, (int)WindowSize().y});
    SetScaleFactor(UiSizeFactor(sConfig.uiSize));
    CenterOnWorkArea();

    auto trayIcon = std::make_unique<TrayIcon>();
    trayIcon->SetTip(L"" APP_NAME);
    trayIcon->SetOnClick(std::bind_front(&MainWindow::OnTrayClick, this));
    trayIcon->SetMenu(std::bind_front(&MainWindow::OnTrayMenu, this));
    SetTrayIcon(std::move(trayIcon));
    SyncTrayIcon();

    ApplyTheme();
    return true;
}

void MainWindow::ApplyTheme()
{
    _themeStale = false;
    LoadUiStyle(sConfig.theme == UiTheme_Dark || (sConfig.theme == UiTheme_System && SystemAppsUseDarkTheme()));
}

void MainWindow::SyncTrayIcon()
{
    SetTrayIconOverride(sApp.IsEnabled() ? nullptr : _pausedIcon);
}

void MainWindow::RequestClose()
{
    switch (sConfig.closeAction) {
    case CloseAction_Hide: Hide(); break;
    case CloseAction_Exit: Close(); break;
    default:
        Show();
        Focus();
        _closePromptRequested = true;
    }
}

bool MainWindow::HandleKeyPress(unsigned short vkCode, bool repeat, bool ownWindowFocused)
{
    if (!repeat && ownWindowFocused) _physicalPresses.Push({vkCode, GetTickCount()});
    return !Keyboard::IsMouseButton(vkCode) && IsCapturingPauseKey();
}

bool MainWindow::HandleKeyRelease(unsigned short vkCode)
{
    if (Keyboard::IsMouseButton(vkCode)) return false;
    KeyBundle_t expected = kPauseCaptureArmed;
    return _pauseKeyCapture.compare_exchange_strong(expected, MakeKeyBundle(vkCode, sKeyboard.TestModifiers()));
}

void MainWindow::ApplyHookEvents()
{
    _physicalPresses.Drain([this](const PhysicalPress& press) { LogKeyPress(press.vkCode, press.ticks); });
    const KeyBundle_t pauseKey = _pauseKeyCapture;
    const bool keyCaptured = pauseKey != kPauseCaptureIdle && pauseKey != kPauseCaptureArmed;
    if (keyCaptured) {
        _pauseKeyCapture = kPauseCaptureIdle;
        if (auto profile = sConfig.editingProfile)
            SetSetting(profile->pauseKey, pauseKey);
        else
            _firstGamePauseKey = pauseKey;
    }
}

void MainWindow::LogKeyPress(unsigned short vkCode, DWORD ticks)
{
    PressLog& log = _pressLog[vkCode];
    log[_pressHead[vkCode]++ % log.size()] = ticks;
    _lastPressTick[vkCode] = ticks;
}

unsigned MainWindow::PressRate(unsigned short vkCode, DWORD nowTicks) const
{
    constexpr unsigned kMinPressesForRate = 3;
    const bool lastPressLeftRateWindow = nowTicks - _lastPressTick[vkCode] > kRateWindowMs;
    if (lastPressLeftRateWindow) return 0;
    unsigned presses = 0;
    DWORD newestAgeMs = kRateWindowMs;
    DWORD oldestAgeMs = 0;
    for (const DWORD tick : _pressLog[vkCode]) {
        const DWORD ageMs = nowTicks - tick;
        if (!tick || ageMs > kRateWindowMs) continue;
        presses++;
        newestAgeMs = ImMin(newestAgeMs, ageMs);
        oldestAgeMs = ImMax(oldestAgeMs, ageMs);
    }
    const DWORD burstSpanMs = oldestAgeMs - newestAgeMs;
    if (presses < kMinPressesForRate || burstSpanMs == 0) return 0;
    const unsigned intervals = presses - 1;
    return (unsigned)lroundf((float)intervals * 1000.f / (float)burstSpanMs);
}

// autofire runs on the input worker; the UI replays its presses from the profile's interval
void MainWindow::ReplayAutofirePresses(unsigned short vkCode, DWORD nowTicks, unsigned intervalMs, bool autofiring)
{
    std::optional<DWORD>& lastReplayedTick = _autofireReplayTick[vkCode];
    if (!autofiring) {
        lastReplayedTick.reset();
        return;
    }
    DWORD tick = lastReplayedTick.value_or(nowTicks);
    if (nowTicks - tick > kRateWindowMs) tick = nowTicks - kRateWindowMs;
    while (nowTicks - tick >= intervalMs) {
        tick += intervalMs;
        LogKeyPress(vkCode, tick);
    }
    lastReplayedTick = tick;
    const DWORD untilNextPressMs = intervalMs - (nowTicks - tick);
    UiRequestFrameIn((float)untilNextPressMs / 1000.f);
}

void MainWindow::TogglePauseKeyCapture()
{
    if (IsCapturingPauseKey()) {
        CancelPauseKeyCapture();
        return;
    }
    if (!sKeyboard.Attach(true)) {
        NotifyKeyboardHookFailed();
        return;
    }
    KeyBundle_t idle = kPauseCaptureIdle;
    _pauseKeyCapture.compare_exchange_strong(idle, kPauseCaptureArmed);
}

void MainWindow::OpenAddGame()
{
    _screen = Screen_AddGame;
    _addGame = {};
}

void MainWindow::CloseAddGame()
{
    _screen = Screen_Editor;
}

std::optional<std::filesystem::path> MainWindow::BrowseForUnclaimedExe(const Profile* allowedOwner)
{
    std::optional<std::filesystem::path> path = BrowseForExe(Native());
    if (!path) return std::nullopt;
    const std::string exe = Utf8FileName(*path);
    const std::shared_ptr<Profile> owner = sConfig.FindProfileByApp(exe);
    if (owner && owner.get() != allowedOwner) {
        Notify(fmt::format("{} already belongs to {}", exe, owner->name));
        return std::nullopt;
    }
    return path;
}

void MainWindow::Notify(std::string notice)
{
    _notice = std::move(notice);
    _noticeShownTicks = GetTickCount();
}

void MainWindow::NotifyKeyboardHookFailed()
{
    Notify("can't hook the keyboard");
}

void MainWindow::ToggleAutoStart()
{
    if (sApp.EnableAutoStart(!_autoStart))
        _autoStart = !_autoStart;
    else
        Notify(fmt::format("can't change \"{}\"", kAutoStartLabel));
}

void MainWindow::DrawNotice(const ImVec2& o)
{
    constexpr DWORD kShowMs = 4000;
    constexpr ImVec2 kGapToWindowCorner(16.f, 16.f);
    const DWORD shownMs = GetTickCount() - _noticeShownTicks;
    const bool shown = !_notice.empty() && shownMs < kShowMs;
    if (shown) UiRequestFrameIn((float)(kShowMs - shownMs) / 1000.f);
    UiToast(o + WindowSize() - kGapToWindowCorner, _notice.c_str(), shown);
}

void MainWindow::OnTrayClick()
{
    if (IsHiddenOrHiding()) {
        Show();
        Focus();
    } else {
        Hide();
    }
}

void MainWindow::OnTrayMenu(TrayIconMenu& menu)
{
    menu.Disabled(L"" APP_NAME);
    if (IsHiddenOrHiding())
        menu.Button(L"Show", std::bind_front(&MainWindow::Show, this));
    else
        menu.Button(L"Hide", std::bind_front(&MainWindow::Hide, this));
    menu.Toggle(L"Enable", sApp.IsEnabled(), [] { sApp.Enable(!sApp.IsEnabled()); });
    menu.Button(L"Exit", std::bind_front(&MainWindow::Close, this));
}

void MainWindow::FitWindowToKeyboard()
{
    const ImVec2 growth = WindowGrowthForMinKeyUnit();
    if (growth.x == _keyboardGrowth.x && growth.y == _keyboardGrowth.y) return;
    _keyboardGrowth = growth;
    SetSize({(int)WindowSize().x, (int)WindowSize().y});
}

std::optional<float> MainWindow::Draw()
{
    // a palette swapped inside a popup would be undone by its PopStyleColor
    if (_themeStale) ApplyTheme();
    ApplyHookEvents();
    FitWindowToKeyboard();
    const bool noGames = sConfig.profiles.empty();
    if (!noGames && !sConfig.editingProfile) sConfig.SetEditingProfile(sConfig.profiles.front());
    const std::shared_ptr<Profile> profileKeptThroughFrame = sConfig.editingProfile;
    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 o = GetWindowPos();

    DrawTitleBar(dl, o);
    if (noGames || _screen == Screen_AddGame)
        DrawAddGame(dl, o + ImFloor(_keyboardGrowth * .5f), noGames);
    else if (profileKeptThroughFrame)
        DrawEditor(dl, o, *profileKeptThroughFrame);

    if (std::exchange(_closePromptRequested, false)) RequestPopup(kPopupClose);
    if (_pendingPopup) OpenPopup(std::exchange(_pendingPopup, nullptr));
    if (profileKeptThroughFrame) {
        DrawKeyMenu(o, *profileKeptThroughFrame);
        DrawAppsPopup(o, *profileKeptThroughFrame);
    }
    DrawSettingsPopup(o);
    DrawTabMenu(o);
    DrawClosePopup(o);
    DrawNotice(o);

    const bool leftWorkForNextFrame = _pendingPopup || _closePromptRequested || _themeStale;
    if (leftWorkForNextFrame) UiRequestFrameIn(0.f);
    return UiTakeFrameRequest();
}

void MainWindow::DrawTitleBar(ImDrawList* dl, const ImVec2& o)
{
    const bool enabled = sApp.IsEnabled();
    const bool noGames = sConfig.profiles.empty();
    const bool onEditor = !noGames && _screen == Screen_Editor;

    dl->AddRectFilled(o + ImVec2(24.f, 21.f), o + ImVec2(38.f, 35.f), enabled ? UiCol.Signal : UiCol.Ink3, 3.f);
    dl->AddText(UiFonts::Heavy, 20.f, o + ImVec2(46.f, 14.f), UiCol.Ink, "spammy");

    const auto lit = sApp.LastActiveProfile();
    TabWidths widths;
    for (const std::shared_ptr<Profile>& item : sConfig.profiles)
        widths.push_back(TabWidth(item->name.c_str()));
    const float growX = _keyboardGrowth.x;
    const float widthCap = TabWidthCap(std::span<const float>(widths.data(), widths.size()), kTabsMaxX + growX);

    const ImU32 litDot = enabled ? UiCol.Signal : UiCol.Ink3;
    float x = kTabsX;
    for (size_t i = 0; i < sConfig.profiles.size(); i++) {
        const std::shared_ptr<Profile>& item = sConfig.profiles[i];
        const char* name = item->name.c_str();
        const bool elided = widths[i] > widthCap;
        const FittedText label = elided ? ElideToFit(name, widthCap, TabWidth) : FittedText{name};
        const char* shown = label.c_str();

        PushID(item.get());
        const bool selected = onEditor && item == sConfig.editingProfile;
        if (UiTab("##tab", o + ImVec2(x, kTabY), shown, selected, item == lit ? litDot : UiCol.DotOff)) {
            sConfig.SetEditingProfile(item);
            CloseAddGame();
        }
        if (IsItemContextClicked()) {
            _tabMenuProfile = item;
            _tabMenuPos = ImVec2(GetItemRectMin().x, GetItemRectMax().y + 6.f);
            RequestPopup(kPopupTabMenu);
        }
        if (elided) Tip(name);
        PopID();
        x += (elided ? TabWidth(shown) : widths[i]) + kTabGap;
    }

    const bool addTabSelected = !noGames && _screen == Screen_AddGame;
    const ImVec2 addPos = o + ImVec2(x, kTabY);
    const ImVec2 addSize(kAddTabSize, kAddTabSize);
    bool addClicked = false;
    if (addTabSelected) {
        const UiCapHit hit = UiCapFrame("##addgame", addPos, addSize, UiCapKind_Plain, 8.f);
        AddGlyph(dl, hit.face + addSize * .5f, 14.f, UiGlyph_Plus, UiCol.Ink2);
    } else {
        AddRecess(dl, addPos, addPos + addSize, 8.f);
        SetCursorScreenPos(addPos);
        addClicked = InvisibleButton("##addgame", addSize);
        AddGlyph(dl, addPos + addSize * .5f, 14.f, UiGlyph_Plus, IsItemHovered() ? UiCol.Ink : UiCol.Ink2);
    }
    Tip("Add a game");
    if (addClicked && !noGames) OpenAddGame();

    if (sUpdater.IsUpdateAvailable()) {
        char update[48];
        FormatInto(update, "new build {}", sUpdater.LatestDate());
        const float textW = SilkWidth(update);
        const float glyphSize = 9.f;
        const float totalW = textW + 4.f + glyphSize;
        const ImVec2 pos = o + ImVec2(kBuildLabelRightX + growX - totalW, 24.f);
        SetCursorScreenPos(pos - ImVec2(4.f, 6.f));
        if (InvisibleButton("##update", ImVec2(totalW + 8.f, 22.f))) sUpdater.OpenReleasePage();
        Tip("Open latest release on GitHub");
        AddSilk(dl, pos, update, UiCol.Signal);
        AddGlyph(dl, pos + ImVec2(textW + 4.f + glyphSize * .5f, 5.f), glyphSize, UiGlyph_ExternalLink, UiCol.Signal);
    } else {
        AddSilk(dl, o + ImVec2(kBuildLabelRightX + growX - SilkWidth(BuildLabel()), 24.f), BuildLabel());
    }

    const ImVec2 right = o + ImVec2(growX, 0.f);
    if (UiIconButton("##gear", right + ImVec2(1171.f, 13.f), 30.f, UiGlyph_Gear)) RequestPopup(kPopupSettings);
    if (UiIconButton("##min", right + ImVec2(1201.f, 13.f), 30.f, UiGlyph_Minimize)) ShowWindow(Native(), SW_MINIMIZE);
    if (UiIconButton("##close", right + ImVec2(1231.f, 13.f), 30.f, UiGlyph_Close)) RequestClose();

    dl->AddRectFilled(o + ImVec2(0.f, 56.f), o + ImVec2(WindowSize().x, 57.f), UiCol.Line);
}

void MainWindow::DrawEditor(ImDrawList* dl, const ImVec2& o, Profile& profile)
{
    DrawStrip(dl, o, profile);
    DrawKeyboard(dl, o, profile);
    DrawRail(dl, o, profile);
}

void MainWindow::DrawStrip(ImDrawList* dl, const ImVec2& o, Profile& profile)
{
    AddSilk(dl, o + ImVec2(24.f, 81.f), "runs in");

    const ImVec2 right = o + ImVec2(_keyboardGrowth.x, 0.f);
    const float tapesX = 82.f;
    const float tapesMaxX = 560.f + _keyboardGrowth.x;
    const float tapeGap = 6.f;
    const float tapeY = 74.f;
    const size_t appCount = profile.apps.size();
    boost::container::small_vector<float, 8> xAfterTapes = {tapesX};
    for (const std::string& app : profile.apps) {
        const float end = xAfterTapes.back() + TapeWidth(app.c_str());
        if (end > tapesMaxX) break;
        xAfterTapes.push_back(end + tapeGap);
    }
    size_t shown = xAfterTapes.size() - 1;
    char overflowTape[16] = "";
    while (shown < appCount) {
        FormatInto(overflowTape, "+{}", appCount - shown);
        const bool overflowTapeFits = xAfterTapes[shown] + TapeWidth(overflowTape) <= tapesMaxX;
        if (overflowTapeFits || !shown) break;
        shown--;
    }

    for (size_t i = 0; i < shown; i++)
        AddTape(dl, o + ImVec2(xAfterTapes[i], tapeY), profile.apps.nth(i)->c_str());
    float x = xAfterTapes[shown];
    bool openApps = false;
    if (shown < appCount) {
        const ImVec2 pos = o + ImVec2(x, tapeY);
        const float w = TapeWidth(overflowTape);
        AddTape(dl, pos, overflowTape);
        SetCursorScreenPos(pos);
        openApps |= InvisibleButton("##moreapps", ImVec2(w, 24.f));
        if (IsItemHovered())
            Tip(fmt::format("{}", fmt::join(profile.apps.nth(shown), profile.apps.end(), "\n")).c_str());
        x += w + tapeGap;
    }
    openApps |= UiOutlineButton("##addapp", o + ImVec2(x, tapeY), "+ App");
    if (openApps) {
        _appsPopupPos = ImVec2(GetItemRectMin().x, GetItemRectMax().y + 6.f);
        RequestPopup(kPopupApps);
    }

    static constexpr const char* kLocksKeptByWinThenAltF4[2][2] = {
        {"", " Alt+F4 stays blocked."},
        {" Win key stays blocked.", " Win key and Alt+F4 stay blocked."},
    };
    char formattedNote[128];
    const char* statusNote = nullptr;
    ImU32 statusCol = UiCol.Ink2;
    if (profile.apps.empty()) {
        statusNote = "Not tied to a game yet \xE2\x80\x93 add its .exe";
        statusCol = UiCol.Signal;
    } else if (!sApp.IsEnabled()) {
        const bool winLocked = profile.disableWin;
        const bool altF4Locked = profile.disableAltF4;
        statusNote = FormatInto(formattedNote, "Paused: every key acts normally.{}",
                                kLocksKeptByWinThenAltF4[winLocked][altF4Locked]);
    } else if (_editMods != KeyMod_None) {
        statusNote = FormatInto(formattedNote, "What keys do while {} is held", ModsName(_editMods));
    }
    if (statusNote) {
        const float w = UiTextWidth(UiFonts::SemiBold, 11.f, statusNote);
        dl->AddText(UiFonts::SemiBold, 11.f, right + ImVec2(906.f - w, 80.f), statusCol, statusNote);
    }

    AddSilk(dl, right + ImVec2(918.f, 81.f), "modifiers");
    UiSegmentedFlags("##layer", right + ImVec2(996.f, 70.f), ImVec2(260.f, 32.f), "NoMod", kModifierSegments,
                     _editMods);
}

void MainWindow::DrawKeyboard(ImDrawList* dl, const ImVec2& o, Profile& profile)
{
    const MouseForm mouseForm = sConfig.mouse;
    const KeyboardBox box = KeyboardBoxAt(o, _keyboardGrowth, mouseForm != MouseForm_Off);
    AddWell(dl, box.wellMin, box.wellMax);

    const ImVec2 area = box.areaMax - box.areaMin;
    const std::span<const KeyboardKey> layout = GetKeyboardLayout(sConfig.form, sConfig.variant);
    const ImVec2 grid = KeyboardGrid(layout);
    const ImVec2 unitFit = (area + kEmptyAfterLastKey) / grid;
    const float unit = ImFloor(ImMin(unitFit.x, unitFit.y));
    const ImVec2 board = grid * unit - kEmptyAfterLastKey;
    const ImVec2 origin = ImFloor(box.areaMin + ImMax(area - board, ImVec2(0.f, 0.f)) * .5f);

    if (IsMouseClicked(ImGuiMouseButton_Left))
        _leftPressClosedPopup = IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

    KeyFrame frame = {
        .profile = &profile,
        .layerMods = _editMods,
        .heldMods = sKeyboard.TestModifiers(),
        .nowTicks = GetTickCount(),
        .wndFocused = GetForegroundWindow() == Native(),
        .paused = !sApp.IsEnabled(),
        .clickClosesPopup = _leftPressClosedPopup,
    };

    for (size_t i = 0; i < layout.size(); i++) {
        const KeyboardKey& key = layout[i];
        const ImVec2 min = ImFloor(origin + ImVec2(key.x, key.y) * unit);
        const ImVec2 max = ImFloor(origin + ImVec2(key.x + key.w, key.y + key.h) * unit);
        PushID((int)i);
        DrawKey(frame, "##key", key.name, key.vkCode, min, max - min - kKeyGap, kUiKeyRadii);
        PopID();
    }

    if (mouseForm != MouseForm_Off) DrawMouse(dl, frame, box.mouseOrigin, mouseForm == MouseForm_5);
    DrawPressBadges(dl, frame.badges);

    char formattedHint[160];
    const char* hint = "Click a key to toggle autofire.  Right-click for one tap, block and modifier combos.  "
                       "Hold a key to test it.";
    if (frame.paused) {
        char keyName[48];
        if (const char* key = PauseKeyName(profile.pauseKey, keyName, sizeof(keyName)))
            hint = FormatInto(formattedHint,
                              "Changes still save while paused.  Press {} or the power button to resume.", key);
        else
            hint = "Changes still save while paused.  Press the power button to resume.";
    } else if (_editMods != KeyMod_None) {
        hint = FormatInto(formattedHint, "Pale keys act as with no modifier.  Click one to give it its own {} setting.",
                          ModsName(_editMods));
    }
    dl->AddText(UiFonts::Medium, 11.f, ImVec2(box.areaMin.x, box.areaMax.y + 4.f), UiCol.Ink3, hint);
}

void MainWindow::DrawKey(KeyFrame& frame, const char* id, const char* label, UINT vkCode, const ImVec2& pos,
                         const ImVec2& size, const float radii[UiCorner_COUNT])
{
    IM_ASSERT(vkCode < kKeyboardKeysCount);
    Profile& profile = *frame.profile;
    const auto vk = (unsigned short)vkCode;
    Profile::KeyModList_t& slots = profile.keys[vk];

    UiKeyDesc desc = {};
    desc.label = label;
    desc.kind = KeyKindOnLayer(profile, vk, frame.layerMods);
    desc.pressed = sKeyboard.IsPressed(vk);
    desc.paused = frame.paused;
    desc.centered = Keyboard::IsMouseButton(vk);
    std::copy_n(radii, UiCorner_COUNT, desc.radii);
    const bool editable = desc.kind != UiKeyKind_Fixed && desc.kind != UiKeyKind_Locked;

    if (frame.layerMods == KeyMod_None && editable) {
        auto hasOwnBindingWith = [&](unsigned mod) {
            for (unsigned m = 1; m < kKeyboardKeyModCount; m++)
                if ((m & mod) && slots[m].action != Action_None) return true;
            return false;
        };
        if (hasOwnBindingWith(KeyMod_Shift)) desc.marks.push_back('S');
        if (hasOwnBindingWith(KeyMod_Ctrl)) desc.marks.push_back('C');
        if (hasOwnBindingWith(KeyMod_Alt)) desc.marks.push_back('A');
    }

    const bool autofiring = frame.wndFocused && desc.pressed && !frame.paused &&
                            ResolveKeyAction(profile, vk, frame.heldMods) == Action_Spammy;
    ReplayAutofirePresses(vk, frame.nowTicks, profile.intervalMs, autofiring);

    const bool unboundWheel = vk == VK_MBUTTON && desc.kind == UiKeyKind_Normal;
    const bool clicked =
        unboundWheel ? WheelBar(GetWindowDrawList(), id, pos, size, desc.pressed) : UiKey(id, pos, size, desc);
    const ImVec2 keyMin = GetItemRectMin();
    const ImVec2 keyMax = GetItemRectMax();

    if (const unsigned rate = PressRate(vk, frame.nowTicks); rate > 2) {
        const ImVec2 anchor((keyMin.x + keyMax.x) * .5f, keyMin.y);
        frame.badges.push_back({anchor, rate, frame.nowTicks - _lastPressTick[vk]});
    }

    if (!editable) return;
    if (clicked && !frame.clickClosesPopup) {
        std::atomic<Action>& action = slots[frame.layerMods].action;
        SetSetting(action, NextKeyAction(action, frame.layerMods));
    }
    if (IsItemContextClicked()) {
        _keyMenuVk = vk;
        _keyMenuKeyMin = keyMin;
        _keyMenuKeyMax = keyMax;
        _keyMenuShowsCombos = false;
        RequestPopup(kPopupKeyMenu);
    }
}

void MainWindow::DrawMouse(ImDrawList* dl, KeyFrame& frame, const ImVec2& origin, bool sideButtons)
{
    AddCap(dl, origin + ImVec2(0.f, 100.f), ImVec2(136.f, 118.f), UiCapKind_Plain, kMouseBodyRadii);

    DrawKey(frame, "##mbL", MouseButtonLabel(VK_LBUTTON), VK_LBUTTON, origin, ImVec2(62.f, 96.f), kMouseLeftRadii);
    DrawKey(frame, "##mbR", MouseButtonLabel(VK_RBUTTON), VK_RBUTTON, origin + ImVec2(74.f, 0.f), ImVec2(62.f, 96.f),
            kMouseRightRadii);
    DrawKey(frame, "##mbM", "", VK_MBUTTON, origin + ImVec2(63.f, 26.f), ImVec2(10.f, 30.f), kUiSmallKeyRadii);
    const char* labelUnderWheel = MouseButtonLabel(VK_MBUTTON);
    const float labelW = UiTextWidth(UiFonts::Medium, 9.f, labelUnderWheel);
    dl->AddText(UiFonts::Medium, 9.f, origin + ImVec2(68.f - labelW * .5f, 62.f), UiCol.Ink3, labelUnderWheel);

    if (sideButtons) {
        DrawKey(frame, "##mb5", MouseButtonLabel(VK_XBUTTON2), VK_XBUTTON2, origin + ImVec2(10.f, 116.f),
                ImVec2(26.f, 30.f), kUiSmallKeyRadii);
        DrawKey(frame, "##mb4", MouseButtonLabel(VK_XBUTTON1), VK_XBUTTON1, origin + ImVec2(10.f, 154.f),
                ImVec2(26.f, 30.f), kUiSmallKeyRadii);
    }
}

void MainWindow::DrawRail(ImDrawList* dl, const ImVec2& windowPos, Profile& profile)
{
    const ImVec2 o = windowPos + ImVec2(0.f, _keyboardGrowth.y);
    const ImVec2 locksO = o + ImVec2(ImFloor(_keyboardGrowth.x * .5f), 0.f);
    const ImVec2 legendO = o + ImVec2(_keyboardGrowth.x, 0.f);
    const bool enabled = sApp.IsEnabled();
    for (const ImVec2& separator :
         {o + ImVec2(264.f, 554.f), locksO + ImVec2(776.f, 554.f), legendO + ImVec2(1024.f, 554.f)})
        dl->AddRectFilled(separator, separator + ImVec2(1.f, 110.f), UiCol.Line);

    AddSilk(dl, o + ImVec2(24.f, 552.f), "power");
    if (UiPowerButton("##power", o + ImVec2(66.f, 614.f), enabled)) sApp.Enable(!enabled);
    dl->AddText(UiFonts::Heavy, 30.f, o + ImVec2(118.f, 584.f), enabled ? UiCol.Ink : UiCol.Ink2,
                enabled ? "ON" : "PAUSED");

    char keyName[48];
    char pauseLine[64];
    const char* pauseText = pauseLine;
    ImU32 pauseCol = UiCol.Ink2;
    if (IsCapturingPauseKey()) {
        pauseText = "Press a key...";
        pauseCol = UiFlash(UiCol.Signal, 1.2f, .35f);
    } else if (const char* key = PauseKeyName(profile.pauseKey, keyName, sizeof(keyName))) {
        FormatInto(pauseLine, "{} {}", key, enabled ? "pauses" : "resumes");
    } else {
        pauseText = "Set a pause key";
        pauseCol = UiCol.Signal;
    }
    const ImVec2 pausePos = o + ImVec2(118.f, 624.f);
    const float pauseW = UiTextWidth(UiFonts::Medium, 12.f, pauseText);
    SetCursorScreenPos(pausePos - ImVec2(3.f, 3.f));
    if (InvisibleButton("##pausekey", ImVec2(pauseW + 6.f, 20.f))) TogglePauseKeyCapture();
    if (IsItemHovered() && IsMouseClicked(ImGuiMouseButton_Right)) {
        CancelPauseKeyCapture();
        SetSetting(profile.pauseKey, 0u);
    }
    if (IsItemHovered()) dl->AddLine(pausePos + ImVec2(0.f, 15.f), pausePos + ImVec2(pauseW, 15.f), pauseCol);
    Tip("Click, then press the key that pauses and resumes Spammy. Right-click to clear it.");
    dl->AddText(UiFonts::Medium, 12.f, pausePos, pauseCol, pauseText);

    char silk[96];
    AddSilk(dl, o + ImVec2(288.f, 552.f), FormatInto(silk, "autofire rate \xC2\xB7 {}", profile.name.c_str()));

    const unsigned rate = IntervalToRate(profile.intervalMs);
    const float rateAsKnobT = (float)(rate - kProfileRateMin) / (float)(kProfileRateMax - kProfileRateMin);
    float knobT = _rateKnobHeld ? _rateKnobT : rateAsKnobT;
    if (UiKnob("##rate", o + ImVec2(344.f, 614.f), knobT, "5", "100", !enabled)) {
        const float picked = (float)kProfileRateMin + ImSaturate(knobT) * (float)(kProfileRateMax - kProfileRateMin);
        SetSetting(profile.intervalMs, RateToInterval((unsigned)lroundf(picked)));
    }
    _rateKnobHeld = IsItemActive();
    _rateKnobT = knobT;

    char number[8];
    FormatInto(number, "{}", IntervalToRate(profile.intervalMs));
    const ImVec2 numberPos = o + ImVec2(420.f, 566.f);
    dl->AddText(UiFonts::Light, 72.f, numberPos, enabled ? UiCol.Ink : UiCol.Ink3, number);
    const float slashX = numberPos.x + UiTextWidth(UiFonts::Light, 72.f, number) + 5.f;
    dl->AddText(UiFonts::Medium, 22.f, ImVec2(slashX, o.y + 612.f), UiCol.Ink2, "/s");

    char caption[48];
    FormatInto(caption, "one press every {} ms", profile.intervalMs.load());
    const float captionDefaultX = o.x + 548.f;
    const float afterRateUnitX = slashX + UiTextWidth(UiFonts::Medium, 22.f, "/s") + 14.f;
    const float captionX = ImMax(captionDefaultX, afterRateUnitX);
    dl->AddText(UiFonts::Medium, 12.f, ImVec2(captionX, o.y + 618.f), UiCol.Ink3, caption);

    auto stepToNextShownRate = [&](int faster) {
        const unsigned shownRate = IntervalToRate(profile.intervalMs);
        for (int candidateMs = (int)profile.intervalMs - faster;
             candidateMs >= kProfileIntervalMinMs && candidateMs <= kProfileIntervalMaxMs; candidateMs -= faster) {
            if (IntervalToRate((unsigned)candidateMs) != shownRate) {
                SetSetting(profile.intervalMs, (unsigned)candidateMs);
                break;
            }
        }
    };
    if (UiCapIconButton("##slower", o + ImVec2(688.f, 580.f), 30.f, UiGlyph_Minus)) stepToNextShownRate(-1);
    if (UiCapIconButton("##faster", o + ImVec2(726.f, 580.f), 30.f, UiGlyph_Plus)) stepToNextShownRate(1);

    AddSilk(dl, locksO + ImVec2(800.f, 552.f), FormatInto(silk, "locks \xC2\xB7 {}", profile.name.c_str()));
    auto lockRow = [&](const char* id, float y, std::atomic<bool>& lock, const char* label, const char* onNote,
                       const char* offNote) {
        if (UiToggle(id, locksO + ImVec2(800.f, y), lock)) SetSetting(lock, !lock);
        const bool flag = lock;
        const char* noteWhenOn = enabled ? onNote : "still blocked while paused";
        AddLabelWithNote(dl, locksO + ImVec2(860.f, y + 1.f), flag ? UiCol.Ink : UiCol.Ink2, label,
                         flag ? noteWhenOn : offNote);
    };
    lockRow("##lockwin", 578.f, profile.disableWin, "Block Win key", "Start menu stays shut",
            "when on, Start menu stays shut");
    lockRow("##lockaltf4", 624.f, profile.disableAltF4, "Block Alt+F4", "sent as plain F4 instead",
            "when on, sent as plain F4");

    AddSilk(dl, legendO + ImVec2(1048.f, 552.f), "keys");
    struct LegendRow {
        UiKeyKind kind;
        const char* text;
    };
    char ownText[48];
    char blockedText[48];
    LegendRow rows[3] = {
        {UiKeyKind_Auto, "Autofire while held"},
        {UiKeyKind_Tap, "One tap, release ignored"},
        {UiKeyKind_Block, "Blocked (combo layers)"},
    };
    if (_editMods != KeyMod_None) {
        rows[0] = {UiKeyKind_SameAuto, "Same as NoMod"};
        rows[1] = {UiKeyKind_Auto, FormatInto(ownText, "Own {} setting", ModsName(_editMods))};
        rows[2] = {UiKeyKind_Block, FormatInto(blockedText, "Blocked with {}", ModsName(_editMods))};
    }
    for (int i = 0; i < 3; i++) {
        const float y = 574.f + 28.f * i;
        AddKeySwatch(dl, legendO + ImVec2(1048.f, y), rows[i].kind);
        dl->AddText(UiFonts::Medium, 12.f, legendO + ImVec2(1084.f, y + 4.f), UiCol.Ink2, rows[i].text);
    }
}

bool MainWindow::HandleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
    if (Window::HandleWndProc(msg, wParam, lParam, result)) return true;
    switch (msg) {
    case WM_USER_FOCUS:
        if (IsHiddenOrHiding()) Show();
        Focus();
        return true;
    case WM_USER_BROWSE_APP:
        if (const std::shared_ptr<Profile> profile = sConfig.editingProfile)
            if (const auto path = BrowseForUnclaimedExe(profile.get()))
                sConfig.BindProfile(*profile, Utf8FileName(*path));
        return true;
    case WM_USER_BROWSE_GAME: {
        const Profile* noGameMayOwnIt = nullptr;
        if (const auto path = BrowseForUnclaimedExe(noGameMayOwnIt)) {
            RunningApp app{Utf8FileName(*path), AppFriendlyName(*path), *path};
            _addGame.selectedExe = app.exeName;
            _addGame.browsed = std::move(app);
        }
        return true;
    }
    case WM_SHOWWINDOW:
        if (wParam) _editMods = KeyMod_None;
        break;
    case WM_SETTINGCHANGE:
        // Windows announces a light/dark switch under this name
        if (lParam && wcscmp((const wchar_t*)lParam, L"ImmersiveColorSet") == 0) _themeStale = true;
        break;
    }
    return false;
}

const char* MainWindowUtil::PauseKeyName(KeyBundle_t pauseKey, char* buf, size_t bufSize)
{
    if (!pauseKey || !bufSize) return nullptr;
    const unsigned mods = KeyBundleMods(pauseKey);
    Keyboard::KeyNameBuf nameBuf;
    const char* name = Keyboard::GetKeyName((unsigned short)KeyBundleVk(pauseKey), nameBuf);
    if (!name[0]) name = "Unknown";

    const auto prefix = fmt::format_to_n(buf, bufSize - 1, "{}{}{}", (mods & KeyMod_Ctrl) ? "Ctrl+" : "",
                                         (mods & KeyMod_Alt) ? "Alt+" : "", (mods & KeyMod_Shift) ? "Shift+" : "");
    auto n = (size_t)(prefix.out - buf);
    for (const char* c = name; *c && n + 1 < bufSize; c++) {
        const bool spaceBeforeWord = *c == ' ' && !isdigit((unsigned char)c[1]);
        if (!spaceBeforeWord) buf[n++] = *c;
    }
    buf[n] = '\0';
    return buf;
}

const char* MainWindowUtil::KeyLabel(unsigned short vkCode, Keyboard::KeyNameBuf& buf)
{
    if (const char* mouse = MouseButtonLabel(vkCode)) return mouse;
    for (const KeyboardKey& key : GetKeyboardLayout(sConfig.form, sConfig.variant))
        if (key.vkCode == vkCode) return key.name;
    const char* name = Keyboard::GetKeyName(vkCode, buf);
    return name[0] ? name : "?";
}

const char* MainWindowUtil::BuildLabel()
{
    static const std::string s_label = [] {
        char label[] = "build " __DATE__;
        SqueezeSpaces(label);
        return std::string(label);
    }();
    return s_label.c_str();
}

const char* MainWindowUtil::ModsName(unsigned mods)
{
    static_assert(KeyMod_Shift == 1 && KeyMod_Alt == 2 && KeyMod_Ctrl == 4);
    static constexpr const char* kNames[] = {
        "no modifier", "Shift", "Alt", "Shift+Alt", "Ctrl", "Shift+Ctrl", "Ctrl+Alt", "Shift+Ctrl+Alt",
    };
    static_assert(std::size(kNames) == kKeyboardKeyModCount);
    return kNames[mods & (kKeyboardKeyModCount - 1)];
}

UiKeyKind MainWindowUtil::KeyKindOf(Action action)
{
    return KeyKindsOf(action).own;
}

FittedText MainWindowUtil::FitText(ImFont* font, float size, const char* text, float maxW, float tracking)
{
    return ElideToFit(text, maxW, [&](const char* candidate) { return UiTextWidth(font, size, candidate, tracking); });
}
