#include "MainWindow.h"
#include "App.h"
#include "Config.h"
#include "KeyboardLayout.h"
#include "Modes.h"
#include "Resources.h"
#include "Updater.h"
#include "Utils.h"

using namespace ImGui;
using namespace MainWindowUtil;

namespace {
constexpr UINT WM_USER_FOCUS = WM_APP + 0x20;

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
    unsigned rate;
    DWORD age;
};
using PressBadges = boost::container::small_vector<PressBadge, 32>;

UiKeyKind KeyKindOnLayer(const Profile& profile, unsigned vkCode, unsigned mods)
{
    if (vkCode == VK_LWIN || vkCode == VK_RWIN) return profile.disableWin ? UiKeyKind_Locked : UiKeyKind_Fixed;
    if (Keyboard::IsModifier((unsigned short)vkCode)) return UiKeyKind_Fixed;
    const Profile::KeyModList_t& slots = profile.keys[vkCode];
    const Action ownAction = slots[mods].action;
    if (mods == KeyMod_None || ownAction != Action_None) return KeyKindOf(ownAction);
    const Action inheritedAction = slots[KeyMod_None].action;
    switch (inheritedAction) {
    case Action_Spammy: return UiKeyKind_SameAuto;
    case Action_Speedy: return UiKeyKind_SameTap;
    default: return UiKeyKind_Normal;
    }
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

const char* KeyCapLabel(const char* upperCaseName, char* buf, size_t bufSize)
{
    if (!strcmp(upperCaseName, "PGUP")) return "PgUp";
    if (!strcmp(upperCaseName, "PGDN")) return "PgDn";
    size_t n = 0;
    for (; upperCaseName[n] && n + 1 < bufSize; n++) {
        const char c = upperCaseName[n];
        const bool afterLetter = n && isalpha((unsigned char)upperCaseName[n - 1]);
        buf[n] = afterLetter ? (char)tolower((unsigned char)c) : c;
    }
    buf[n] = '\0';
    return buf;
}

const char* MouseButtonLabel(unsigned short vkCode)
{
    switch (vkCode) {
    case VK_LBUTTON: return "LMB";
    case VK_RBUTTON: return "RMB";
    case VK_MBUTTON: return "M3";
    case VK_XBUTTON1: return "M4";
    case VK_XBUTTON2: return "M5";
    }
    return nullptr;
}

float TabWidthCap(std::span<const float> widths, float tabsMaxX)
{
    constexpr float kNoCap = FLT_MAX;
    float avail = tabsMaxX - kTabsX - kAddTabSize - kTabGap * (float)widths.size();
    float total = 0.f;
    for (const float w : widths)
        total += w;
    if (total <= avail) return kNoCap;

    boost::container::small_vector<float, 8> narrowestFirst(widths.begin(), widths.end());
    std::sort(narrowestFirst.begin(), narrowestFirst.end());
    for (size_t i = 0; i < narrowestFirst.size(); i++) {
        const float evenShareOfRest = avail / (float)(narrowestFirst.size() - i);
        if (narrowestFirst[i] > evenShareOfRest) return ImMax(evenShareOfRest, 0.f);
        avail -= narrowestFirst[i];
    }
    return kNoCap;
}

void DrawPressBadges(ImDrawList* dl, const PressBadges& badges)
{
    for (const PressBadge& badge : badges) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u/s", badge.rate);
        const float pop = badge.age < 120 ? 1.3f - .3f * (badge.age / 120.f) : 1.f;
        const float alpha = badge.age > 700 ? ImMax(0.f, 1.f - (badge.age - 700) / 300.f) : 1.f;
        AddRateBadge(dl, badge.anchor, buf, alpha, pop);
    }
}

bool WheelBar(ImDrawList* dl, const char* id, const ImVec2& pos, const ImVec2& size, bool pressed)
{
    SetCursorScreenPos(pos);
    const bool clicked = InvisibleButton(id, size);
    const ImVec2 sink(0.f, pressed ? 3.f : 0.f);
    if (IsItemHovered())
        dl->AddRect(pos + sink - ImVec2(2.f, 2.f), pos + sink + size + ImVec2(2.f, 2.f), UiCol::Ink2, 6.f, 0, 1.5f);
    dl->AddRectFilled(pos + sink, pos + sink + size, UiCol::Tap, 5.f);
    return clicked;
}

// __DATE__ space-pads one-digit days: "Oct  1 2026"
void SqueezeSpaces(char* text)
{
    char* out = text;
    for (const char* c = text; *c; c++) {
        if (*c == ' ' && out > text && out[-1] == ' ') continue;
        *out++ = *c;
    }
    *out = '\0';
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

bool MainWindow::Initialize()
{
    if (HWND running = FindWindowW(L"" APP_NAME, L"" APP_NAME)) {
        PostMessageW(running, WM_USER_FOCUS, NULL, NULL);
        return false;
    }

    if (const ErrorCode ec = Window::Initialize(); ec != ErrorCode_OK) {
        char msg[256];
        snprintf(msg, std::size(msg), "%s (0x%08lX)", FormatError(ec), (unsigned long)LastInitError());
        MessageBoxA(nullptr, msg, APP_NAME, MB_ICONERROR | MB_OK);
        return false;
    }

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

    LoadUiStyle();
    return true;
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
    unsigned presses = 0;
    DWORD newestAgeMs = kRateWindowMs, oldestAgeMs = 0;
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
    DWORD& tick = _autofireReplayTick[vkCode];
    if (!autofiring) {
        tick = nowTicks;
        return;
    }
    if (nowTicks - tick > kRateWindowMs) tick = nowTicks - kRateWindowMs;
    while (nowTicks - tick >= intervalMs) {
        tick += intervalMs;
        LogKeyPress(vkCode, tick);
    }
}

void MainWindow::TogglePauseKeyCapture()
{
    if (IsCapturingPauseKey()) {
        CancelPauseKeyCapture();
        return;
    }
    sKeyboard.Attach(true);
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

std::optional<std::filesystem::path> MainWindow::BrowseForUnclaimedExe(const Profile* allowedOwner,
                                                                       TimedNote& rejection)
{
    std::filesystem::path path;
    if (!BrowseForExe(Native(), path)) return std::nullopt;
    const std::string exe = Utf8FileName(path);
    const std::shared_ptr<Profile> owner = sConfig.FindProfileByApp(exe.c_str());
    if (owner && owner.get() != allowedOwner) {
        rejection.Show(exe + " already belongs to " + owner->name);
        return std::nullopt;
    }
    rejection.text.clear();
    return path;
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

void MainWindow::Draw()
{
    ApplyHookEvents();
    FitWindowToKeyboard();
    const bool noGames = sConfig.profiles.empty();
    if (!noGames && !sConfig.editingProfile) sConfig.SetEditingProfile(sConfig.profiles.front());
    const std::shared_ptr<Profile> profile = sConfig.editingProfile; // popups can delete the game mid-frame
    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 o = GetWindowPos();

    DrawTitleBar(dl, o);
    if (noGames || _screen == Screen_AddGame)
        DrawAddGame(dl, o + ImFloor(_keyboardGrowth * .5f), noGames);
    else if (profile)
        DrawEditor(dl, o, *profile);

    if (std::exchange(_closePromptRequested, false)) RequestPopup(kPopupClose);
    if (_pendingPopup) OpenPopup(std::exchange(_pendingPopup, nullptr));
    if (profile) {
        DrawKeyMenu(o, *profile);
        DrawAppsPopup(o, *profile);
    }
    DrawSettingsPopup(o);
    DrawTabMenu(o);
    DrawClosePopup(o);
}

void MainWindow::DrawTitleBar(ImDrawList* dl, const ImVec2& o)
{
    const bool enabled = sApp.IsEnabled();
    const bool noGames = sConfig.profiles.empty();
    const bool onEditor = !noGames && _screen == Screen_Editor;

    dl->AddRectFilled(o + ImVec2(24.f, 21.f), o + ImVec2(38.f, 35.f), enabled ? UiCol::Signal : UiCol::Ink3, 3.f);
    dl->AddText(UiFonts::Heavy, 20.f, o + ImVec2(46.f, 14.f), UiCol::Ink, "spammy");

    const auto lit = sApp.LastActiveProfile();
    boost::container::small_vector<float, 8> widths;
    for (const std::shared_ptr<Profile>& item : sConfig.profiles)
        widths.push_back(TabWidth(item->name.c_str()));
    const float growX = _keyboardGrowth.x;
    const float widthCap = TabWidthCap(std::span<const float>(widths.data(), widths.size()), kTabsMaxX + growX);

    float x = kTabsX;
    for (size_t i = 0; i < sConfig.profiles.size(); i++) {
        const std::shared_ptr<Profile>& item = sConfig.profiles[i];
        const bool elided = widths[i] > widthCap;
        const std::string label = elided ? FitText(item->name, widthCap, TabWidth) : std::string();
        const char* shown = elided ? label.c_str() : item->name.c_str();

        PushID(item.get());
        const bool selected = onEditor && item == sConfig.editingProfile;
        if (UiTab("##tab", o + ImVec2(x, kTabY), shown, selected, item == lit, !enabled)) {
            sConfig.SetEditingProfile(item);
            CloseAddGame();
        }
        if (IsItemContextClicked()) {
            _tabMenuProfile = item;
            _tabMenuPos = ImVec2(GetItemRectMin().x, GetItemRectMax().y + 6.f);
            RequestPopup(kPopupTabMenu);
        }
        if (elided) Tip("%s", item->name.c_str());
        PopID();
        x += (elided ? TabWidth(shown) : widths[i]) + kTabGap;
    }

    const bool addTabSelected = !noGames && _screen == Screen_AddGame;
    const ImVec2 addPos = o + ImVec2(x, kTabY);
    const ImVec2 addSize(kAddTabSize, kAddTabSize);
    bool addClicked;
    if (addTabSelected) {
        const UiCapHit hit = UiCapFrame("##addgame", addPos, addSize, UiCapKind_Plain, 8.f);
        AddGlyph(dl, hit.face + addSize * .5f, 14.f, UiGlyph_Plus, UiCol::Ink2);
        addClicked = false;
    } else {
        AddRecess(dl, addPos, addPos + addSize, 8.f);
        SetCursorScreenPos(addPos);
        addClicked = InvisibleButton("##addgame", addSize);
        AddGlyph(dl, addPos + addSize * .5f, 14.f, UiGlyph_Plus, IsItemHovered() ? UiCol::Ink : UiCol::Ink2);
    }
    Tip("Add a game");
    if (addClicked && !noGames) OpenAddGame();

    if (sUpdater.IsUpdateAvailable()) {
        char update[48];
        snprintf(update, sizeof(update), "new build %s", sUpdater.LatestDate());
        SqueezeSpaces(update);
        const float textW = SilkWidth(update);
        const float glyphSize = 9.f;
        const float totalW = textW + 4.f + glyphSize;
        const ImVec2 pos = o + ImVec2(kBuildLabelRightX + growX - totalW, 24.f);
        SetCursorScreenPos(pos - ImVec2(4.f, 6.f));
        if (InvisibleButton("##update", ImVec2(totalW + 8.f, 22.f))) sUpdater.OpenReleasePage();
        Tip("Open latest release on GitHub");
        AddSilk(dl, pos, update, UiCol::Signal);
        AddGlyph(dl, pos + ImVec2(textW + 4.f + glyphSize * .5f, 5.f), glyphSize, UiGlyph_ExternalLink, UiCol::Signal);
    } else {
        AddSilk(dl, o + ImVec2(kBuildLabelRightX + growX - SilkWidth(BuildLabel()), 24.f), BuildLabel());
    }

    const ImVec2 right = o + ImVec2(growX, 0.f);
    if (UiIconButton("##gear", right + ImVec2(1171.f, 13.f), 30.f, UiGlyph_Gear)) RequestPopup(kPopupSettings);
    if (UiIconButton("##min", right + ImVec2(1201.f, 13.f), 30.f, UiGlyph_Minimize)) ShowWindow(Native(), SW_MINIMIZE);
    if (UiIconButton("##close", right + ImVec2(1231.f, 13.f), 30.f, UiGlyph_Close)) RequestClose();

    dl->AddRectFilled(o + ImVec2(0.f, 56.f), o + ImVec2(WindowSize().x, 57.f), UiCol::Line);
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
    const float tapesX = 82.f, tapesMaxX = 560.f + _keyboardGrowth.x, tapeGap = 6.f, tapeY = 74.f;
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
        snprintf(overflowTape, sizeof(overflowTape), "+%d", (int)(appCount - shown));
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
        if (IsItemHovered()) {
            std::string hidden;
            for (size_t i = shown; i < appCount; i++) {
                if (!hidden.empty()) hidden += '\n';
                hidden += *profile.apps.nth(i);
            }
            Tip("%s", hidden.c_str());
        }
        x += w + tapeGap;
    }
    openApps |= UiOutlineButton("##addapp", o + ImVec2(x, tapeY), "+ App");
    if (openApps) {
        _appsPopupPos = ImVec2(GetItemRectMin().x, GetItemRectMax().y + 6.f);
        RequestPopup(kPopupApps);
    }

    char statusNote[128] = "";
    ImU32 statusCol = UiCol::Ink2;
    if (profile.apps.empty()) {
        ImStrncpy(statusNote, "Not tied to a game yet \xE2\x80\x93 add its .exe", sizeof(statusNote));
        statusCol = UiCol::Signal;
    } else if (!sApp.IsEnabled()) {
        const char* locks = profile.disableWin && profile.disableAltF4 ? " Win key and Alt+F4 stay blocked."
                            : profile.disableWin                       ? " Win key stays blocked."
                            : profile.disableAltF4                     ? " Alt+F4 stays blocked."
                                                                       : "";
        snprintf(statusNote, sizeof(statusNote), "Paused: every key acts normally.%s", locks);
    } else if (_editMods != KeyMod_None) {
        snprintf(statusNote, sizeof(statusNote), "What keys do while %s is held", ModsName(_editMods));
    }
    if (statusNote[0]) {
        const float w = UiTextWidth(UiFonts::SemiBold, 11.f, statusNote);
        dl->AddText(UiFonts::SemiBold, 11.f, right + ImVec2(906.f - w, 80.f), statusCol, statusNote);
    }

    AddSilk(dl, right + ImVec2(918.f, 81.f), "modifiers");
    UiSegmentedFlags("##layer", right + ImVec2(996.f, 70.f), ImVec2(260.f, 32.f), "NoMod", kModifierSegments,
                     _editMods, UiLabelCase_Upper);
}

void MainWindow::DrawKeyboard(ImDrawList* dl, const ImVec2& o, Profile& profile)
{
    const MouseForm mouseForm = sConfig.mouse;
    const KeyboardBox box = KeyboardBoxAt(o, _keyboardGrowth, mouseForm != MouseForm_Off);
    dl->AddRectFilled(box.wellMin, box.wellMax, UiCol::Well, 16.f);
    dl->AddLine(ImVec2(box.wellMin.x + 16.f, box.wellMin.y + 1.f), ImVec2(box.wellMax.x - 16.f, box.wellMin.y + 1.f),
                UiCol::WellEdge, 2.f);

    const ImVec2 area = box.areaMax - box.areaMin;
    const std::span<const KeyboardKey> layout = GetKeyboardLayout(sConfig.form, sConfig.variant);
    const ImVec2 grid = KeyboardGrid(layout);
    const ImVec2 unitFit = (area + kEmptyAfterLastKey) / grid;
    const float unit = ImFloor(ImMin(unitFit.x, unitFit.y));
    const ImVec2 board = grid * unit - kEmptyAfterLastKey;
    const ImVec2 origin = ImFloor(box.areaMin + ImMax(area - board, ImVec2(0.f, 0.f)) * .5f);

    static bool s_leftPressClosedPopup = false;
    if (IsMouseClicked(ImGuiMouseButton_Left))
        s_leftPressClosedPopup = IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

    KeyFrame frame = {
        .profile = &profile,
        .layerMods = _editMods,
        .heldMods = sKeyboard.TestModifiers(),
        .nowTicks = GetTickCount(),
        .wndFocused = GetForegroundWindow() == Native(),
        .paused = !sApp.IsEnabled(),
        .clickClosesPopup = s_leftPressClosedPopup,
    };

    for (size_t i = 0; i < layout.size(); i++) {
        const KeyboardKey& key = layout[i];
        const ImVec2 min = ImFloor(origin + ImVec2(key.x, key.y) * unit);
        const ImVec2 max = ImFloor(origin + ImVec2(key.x + key.w, key.y + key.h) * unit);
        char label[16];
        PushID((int)i);
        DrawKey(frame, "##key", KeyCapLabel(key.name, label, sizeof(label)), key.vkCode, min, max - min - kKeyGap);
        PopID();
    }

    if (mouseForm != MouseForm_Off) DrawMouse(dl, frame, box.mouseOrigin, mouseForm == MouseForm_5);
    DrawPressBadges(dl, frame.badges);

    char hint[160];
    if (frame.paused) {
        char keyName[48];
        if (const char* key = PauseKeyName(profile.pauseKey, keyName, sizeof(keyName)))
            snprintf(hint, sizeof(hint), "Changes still save while paused.  Press %s or the power button to resume.",
                     key);
        else
            ImStrncpy(hint, "Changes still save while paused.  Press the power button to resume.", sizeof(hint));
    } else if (_editMods != KeyMod_None) {
        snprintf(hint, sizeof(hint), "Pale keys act as with no modifier.  Click one to give it its own %s setting.",
                 ModsName(_editMods));
    } else {
        ImStrncpy(hint,
                  "Click a key to toggle autofire.  Right-click for one tap, block and modifier combos.  "
                  "Hold a key to test it.",
                  sizeof(hint));
    }
    dl->AddText(UiFonts::Medium, 11.f, ImVec2(box.areaMin.x, box.areaMax.y + 4.f), UiCol::Ink3, hint);
}

void MainWindow::DrawKey(KeyFrame& frame, const char* id, const char* label, UINT vkCode, const ImVec2& pos,
                         const ImVec2& size, const float radii[UiCorner_COUNT])
{
    IM_ASSERT(vkCode < kKeyboardKeysCount);
    Profile& profile = *frame.profile;
    const unsigned short vk = (unsigned short)vkCode;
    Profile::KeyModList_t& slots = profile.keys[vk];

    UiKeyDesc desc = {};
    desc.label = label;
    desc.kind = KeyKindOnLayer(profile, vk, frame.layerMods);
    desc.pressed = sKeyboard.IsPressed(vk);
    desc.paused = frame.paused;
    desc.centered = Keyboard::IsMouseButton(vk);
    if (radii) std::copy_n(radii, UiCorner_COUNT, desc.radii);
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
        Action& action = slots[frame.layerMods].action;
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
    DrawKey(frame, "##mbM", "", VK_MBUTTON, origin + ImVec2(63.f, 26.f), ImVec2(10.f, 30.f));
    const char* labelUnderWheel = MouseButtonLabel(VK_MBUTTON);
    const float labelW = UiTextWidth(UiFonts::Medium, 9.f, labelUnderWheel);
    dl->AddText(UiFonts::Medium, 9.f, origin + ImVec2(68.f - labelW * .5f, 62.f), UiCol::Ink3, labelUnderWheel);

    if (sideButtons) {
        DrawKey(frame, "##mb5", MouseButtonLabel(VK_XBUTTON2), VK_XBUTTON2, origin + ImVec2(10.f, 116.f),
                ImVec2(26.f, 30.f));
        DrawKey(frame, "##mb4", MouseButtonLabel(VK_XBUTTON1), VK_XBUTTON1, origin + ImVec2(10.f, 154.f),
                ImVec2(26.f, 30.f));
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
        dl->AddRectFilled(separator, separator + ImVec2(1.f, 110.f), UiCol::Line);

    AddSilk(dl, o + ImVec2(24.f, 552.f), "power");
    if (UiPowerButton("##power", o + ImVec2(66.f, 614.f), enabled)) sApp.Enable(!enabled);
    dl->AddText(UiFonts::Heavy, 30.f, o + ImVec2(118.f, 584.f), enabled ? UiCol::Ink : UiCol::Ink2,
                enabled ? "ON" : "PAUSED");

    char keyName[48], pauseLine[64];
    const char* pauseText = pauseLine;
    ImU32 pauseCol = UiCol::Ink2;
    if (IsCapturingPauseKey()) {
        pauseText = "Press a key...";
        pauseCol = UiFlash(UiCol::Signal, 1.2f, .35f);
    } else if (const char* key = PauseKeyName(profile.pauseKey, keyName, sizeof(keyName))) {
        snprintf(pauseLine, sizeof(pauseLine), "%s %s", key, enabled ? "pauses" : "resumes");
    } else {
        pauseText = "Set a pause key";
        pauseCol = UiCol::Signal;
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
    snprintf(silk, sizeof(silk), "autofire rate \xC2\xB7 %s", profile.name.c_str());
    AddSilk(dl, o + ImVec2(288.f, 552.f), silk);

    const unsigned rate = IntervalToRate(profile.intervalMs);
    const float rateAsKnobT = (float)(rate - kProfileRateMin) / (float)(kProfileRateMax - kProfileRateMin);
    static float s_unroundedKnobT = 0.f;
    static bool s_knobHeld = false;
    float knobT = s_knobHeld ? s_unroundedKnobT : rateAsKnobT;
    if (UiKnob("##rate", o + ImVec2(344.f, 614.f), knobT, "5", "100", !enabled)) {
        const float picked = (float)kProfileRateMin + ImSaturate(knobT) * (float)(kProfileRateMax - kProfileRateMin);
        SetSetting(profile.intervalMs, RateToInterval((unsigned)lroundf(picked)));
    }
    s_knobHeld = IsItemActive();
    s_unroundedKnobT = knobT;

    char number[8];
    snprintf(number, sizeof(number), "%u", IntervalToRate(profile.intervalMs));
    const ImVec2 numberPos = o + ImVec2(420.f, 566.f);
    dl->AddText(UiFonts::Light, 72.f, numberPos, enabled ? UiCol::Ink : UiCol::Ink3, number);
    const float slashX = numberPos.x + UiTextWidth(UiFonts::Light, 72.f, number) + 5.f;
    dl->AddText(UiFonts::Medium, 22.f, ImVec2(slashX, o.y + 612.f), UiCol::Ink2, "/s");

    char caption[48];
    snprintf(caption, sizeof(caption), "one press every %u ms", profile.intervalMs);
    const float captionDefaultX = o.x + 548.f;
    const float afterRateUnitX = slashX + UiTextWidth(UiFonts::Medium, 22.f, "/s") + 14.f;
    const float captionX = ImMax(captionDefaultX, afterRateUnitX);
    dl->AddText(UiFonts::Medium, 12.f, ImVec2(captionX, o.y + 618.f), UiCol::Ink3, caption);

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

    snprintf(silk, sizeof(silk), "locks \xC2\xB7 %s", profile.name.c_str());
    AddSilk(dl, locksO + ImVec2(800.f, 552.f), silk);
    auto lockRow = [&](const char* id, float y, bool& flag, const char* label, const char* onNote,
                       const char* offNote) {
        if (UiToggle(id, locksO + ImVec2(800.f, y), flag)) SetSetting(flag, !flag);
        dl->AddText(UiFonts::SemiBold, 13.f, locksO + ImVec2(860.f, y + 1.f), flag ? UiCol::Ink : UiCol::Ink2, label);
        const char* note = !flag ? offNote : enabled ? onNote : "still blocked while paused";
        dl->AddText(UiFonts::Medium, 10.f, locksO + ImVec2(860.f, y + 17.f), UiCol::Ink3, note);
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
    char ownText[48], blockedText[48];
    LegendRow rows[3] = {
        {UiKeyKind_Auto, "Autofire while held"},
        {UiKeyKind_Tap, "One tap, release ignored"},
        {UiKeyKind_Block, "Blocked (combo layers)"},
    };
    if (_editMods != KeyMod_None) {
        snprintf(ownText, sizeof(ownText), "Own %s setting", ModsName(_editMods));
        snprintf(blockedText, sizeof(blockedText), "Blocked with %s", ModsName(_editMods));
        rows[0] = {UiKeyKind_SameAuto, "Same as NoMod"};
        rows[1] = {UiKeyKind_Auto, ownText};
        rows[2] = {UiKeyKind_Block, blockedText};
    }
    for (int i = 0; i < 3; i++) {
        const float y = 574.f + 28.f * i;
        AddKeySwatch(dl, legendO + ImVec2(1048.f, y), rows[i].kind);
        dl->AddText(UiFonts::Medium, 12.f, legendO + ImVec2(1084.f, y + 4.f), UiCol::Ink2, rows[i].text);
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
    case WM_SHOWWINDOW:
        if (wParam) _editMods = KeyMod_None;
        break;
    }
    return false;
}

const char* MainWindowUtil::PauseKeyName(KeyBundle_t pauseKey, char* buf, size_t bufSize)
{
    if (!pauseKey || !bufSize) return nullptr;
    const unsigned mods = KeyBundleMods(pauseKey);
    const char* name = Keyboard::GetKeyName((unsigned short)KeyBundleVk(pauseKey));
    if (!name[0]) name = "Unknown";

    const int prefix = snprintf(buf, bufSize, "%s%s%s", (mods & KeyMod_Ctrl) ? "Ctrl+" : "",
                                (mods & KeyMod_Alt) ? "Alt+" : "", (mods & KeyMod_Shift) ? "Shift+" : "");
    size_t n = (size_t)ImClamp(prefix, 0, (int)bufSize - 1);
    for (const char* c = name; *c && n + 1 < bufSize; c++) {
        const bool spaceBeforeWord = *c == ' ' && !isdigit((unsigned char)c[1]);
        if (!spaceBeforeWord) buf[n++] = *c;
    }
    buf[n] = '\0';
    return buf;
}

const char* MainWindowUtil::KeyLabel(unsigned short vkCode, char* buf, size_t bufSize)
{
    if (const char* mouse = MouseButtonLabel(vkCode)) return mouse;
    for (const KeyboardKey& key : GetKeyboardLayout(sConfig.form, sConfig.variant))
        if (key.vkCode == vkCode && key.name[0]) return KeyCapLabel(key.name, buf, bufSize);
    const char* name = Keyboard::GetKeyName(vkCode);
    return name[0] ? name : "?";
}

const char* MainWindowUtil::BuildLabel()
{
    static const std::string s_label = [] {
        char label[32];
        snprintf(label, sizeof(label), "build %s", __DATE__);
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
    switch (action) {
    case Action_Spammy: return UiKeyKind_Auto;
    case Action_Speedy: return UiKeyKind_Tap;
    case Action_Disabled: return UiKeyKind_Block;
    default: return UiKeyKind_Normal;
    }
}

std::string MainWindowUtil::FitText(ImFont* font, float size, std::string text, float maxW, float tracking)
{
    return FitText(std::move(text), maxW,
                   [&](const char* candidate) { return UiTextWidth(font, size, candidate, tracking); });
}
