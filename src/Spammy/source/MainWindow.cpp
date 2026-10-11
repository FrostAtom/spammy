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
constexpr DWORD kRateWindowMs = 1000;

// title bar
constexpr float kTabsX = 168.f;
constexpr float kTabsMaxX = 1000.f; // the BUILD silk starts past here
constexpr float kTabY = 11.f;
constexpr float kTabGap = 8.f;
constexpr float kAddTabSize = 34.f;
// a key face is one grid unit minus these; 4 of the vertical gap is the lip under the face
constexpr ImVec2 kKeyGap(6.f, 10.f);
constexpr float kKeyLip = 4.f;

constexpr const char* kEllipsis = "\xE2\x80\xA6";

// the MODIFIERS switch, segment by segment
constexpr const char* kLayerLabels[] = {"NoMod", "Shift", "Ctrl", "Alt"};
constexpr unsigned kLayerMods[] = {KeyMod_None, KeyMod_Shift, KeyMod_Ctrl, KeyMod_Alt};

constexpr float kMouseLeftRadii[4] = {28.f, 4.f, 4.f, 4.f};
constexpr float kMouseRightRadii[4] = {4.f, 28.f, 4.f, 4.f};
constexpr float kMouseBodyRadii[4] = {6.f, 6.f, 40.f, 40.f};

struct PressBadge {
    ImVec2 anchor;
    unsigned rate;
    DWORD age;
};
using PressBadges = boost::container::small_vector<PressBadge, 32>;

UiKeyKind KindOfAction(Action action)
{
    switch (action) {
    case Action_Spammy: return UiKeyKind_Auto;
    case Action_Speedy: return UiKeyKind_Tap;
    case Action_Disabled: return UiKeyKind_Block;
    default: return UiKeyKind_Normal;
    }
}

// how a key looks on a MODIFIERS layer: its own slot there, else a pale echo of what it does with no modifier
UiKeyKind KeyKindOnLayer(const Profile& profile, unsigned vkCode, unsigned mods)
{
    if (vkCode == VK_LWIN || vkCode == VK_RWIN) return profile.disableWin ? UiKeyKind_Locked : UiKeyKind_Fixed;
    if (Keyboard::IsModifier((unsigned short)vkCode)) return UiKeyKind_Fixed;
    const Profile::KeyModList_t& slots = profile.keys[vkCode];
    if (mods == KeyMod_None || slots[mods].action != Action_None) return KindOfAction(slots[mods].action);
    switch (slots[KeyMod_None].action) {
    case Action_Spammy: return UiKeyKind_SameAuto;
    case Action_Speedy: return UiKeyKind_SameTap;
    default: return UiKeyKind_Normal;
    }
}

// NoMod toggles autofire; a modifier layer cycles through everything its slot can hold
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

// layout names are stored in caps ("ESC", "PGUP"); the board prints them the way keycaps do ("Esc", "PgUp")
const char* KeyCapLabel(const char* name, char* buf, size_t bufSize)
{
    if (!strcmp(name, "PGUP")) return "PgUp";
    if (!strcmp(name, "PGDN")) return "PgDn";
    size_t n = 0;
    for (; name[n] && n + 1 < bufSize; n++) {
        const bool afterLetter = n && isalpha((unsigned char)name[n - 1]);
        buf[n] = afterLetter ? (char)tolower((unsigned char)name[n]) : name[n];
    }
    buf[n] = '\0';
    return buf;
}

// longest start of label that fits a tab of maxW with an ellipsis after it
std::string ElideTabLabel(std::string_view label, float maxW)
{
    std::string text(label);
    while (!text.empty()) {
        size_t cut = text.size() - 1;
        while (cut && ((unsigned char)text[cut] & 0xC0) == 0x80) // back to the lead byte of a multi-byte char
            cut--;
        text.erase(cut);
        while (!text.empty() && text.back() == ' ')
            text.pop_back();
        if (TabWidth((text + kEllipsis).c_str()) <= maxW) break;
    }
    return text + kEllipsis;
}

// widest a tab may get so that all of them, the gaps and the "+" tab end before kTabsMaxX; FLT_MAX when all fit
float TabWidthCap(std::span<const float> widths)
{
    float avail = kTabsMaxX - kTabsX - kAddTabSize - kTabGap * (float)widths.size();
    float total = 0.f;
    for (const float w : widths)
        total += w;
    if (total <= avail) return FLT_MAX;

    // the widest tabs give way first: fill from the narrowest until the even share of the rest is too small
    boost::container::small_vector<float, 8> sorted(widths.begin(), widths.end());
    std::sort(sorted.begin(), sorted.end());
    for (size_t i = 0; i < sorted.size(); i++) {
        const float share = avail / (float)(sorted.size() - i);
        if (sorted[i] > share) return ImMax(share, 0.f);
        avail -= sorted[i];
    }
    return FLT_MAX;
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

// the mouse wheel while unbound: a dark rubber bar rather than a tiny white cap
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

// __DATE__ pads a one-digit day with a second space ("Oct  1 2026")
void SqueezeSpaces(char* text)
{
    char* out = text;
    for (const char* c = text; *c; c++) {
        if (*c == ' ' && out > text && out[-1] == ' ') continue;
        *out++ = *c;
    }
    *out = '\0';
}
} // namespace

// everything a key needs while the keyboard is drawn, resolved once per frame
struct MainWindow::KeyFrame {
    Profile* profile;
    unsigned mods;     // the layer being edited
    unsigned heldMods; // modifiers physically held: the live tester shows what the game would get
    DWORD nowTicks;
    bool wndFocused;
    bool paused;
    bool clickBlocked; // the left press went down while a popup was open: it only closes that popup
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
        snprintf(msg, std::size(msg), "%s (0x%08lX)", FormatError(ec), (unsigned long)LastError());
        MessageBoxA(nullptr, msg, APP_NAME, MB_ICONERROR | MB_OK);
        return false;
    }

    HICON icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_ICON1));
    SetIcon(icon);
    _pausedIcon = CreateGrayscaleIcon(icon);
    EnableMoving();
    EnableTitleBar(false);
    SetSize({(int)kWindowSize.x, (int)kWindowSize.y});
    SetScaleFactor(UiSizeFactor(sConfig.uiSize));
    ResetPosition();

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
        // the popup is drawn inside the ImGui frame, so the window has to be up for the user to see it
        if (!IsWndNormalized()) Show();
        Focus();
        _askClose = true;
    }
}

bool MainWindow::HandleKeyPress(unsigned short vkCode, bool repeat, bool focused)
{
    if (!repeat && focused) _physicalPresses.Push({vkCode, GetTickCount()});
    return !Keyboard::IsMouseButton(vkCode) && _capturingPauseKey;
}

bool MainWindow::HandleKeyRelease(unsigned short vkCode)
{
    if (Keyboard::IsMouseButton(vkCode) || !_capturingPauseKey) return false;
    _capturedPauseKey = MakeKeyBundle(vkCode, sKeyboard.TestModifiers());
    _capturingPauseKey = false;
    return true;
}

void MainWindow::ApplyHookEvents()
{
    _physicalPresses.Drain([this](const PhysicalPress& press) { LogKeyPress(press.vkCode, press.ticks); });
    if (const unsigned pauseKey = _capturedPauseKey.exchange(0)) {
        if (auto profile = sConfig.editingProfile)
            SetSetting(profile->vkPause, pauseKey);
        else
            _setupPauseKey = pauseKey;
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
    return (unsigned)std::ranges::count_if(_pressLog[vkCode],
                                           [&](DWORD t) { return t && nowTicks - t <= kRateWindowMs; });
}

// autofire happens on the input worker, so the UI re-derives the simulated presses from the profile speed
void MainWindow::TickSimulatedPresses(unsigned short vkCode, DWORD nowTicks, unsigned speed, bool firing)
{
    DWORD& tick = _simulatedTick[vkCode];
    if (!firing) {
        tick = nowTicks;
        return;
    }
    if (nowTicks - tick > kRateWindowMs) tick = nowTicks - kRateWindowMs;
    while (nowTicks - tick >= speed) {
        tick += speed;
        LogKeyPress(vkCode, tick);
    }
}

void MainWindow::TogglePauseKeyCapture()
{
    // the old key stays until a new one is actually captured, so cancelling loses nothing
    if (_capturingPauseKey) {
        _capturingPauseKey = false;
        return;
    }
    sKeyboard.Attach(true);
    _capturingPauseKey = true;
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

void MainWindow::OnTrayClick()
{
    if (IsShown()) {
        Hide();
    } else {
        Show();
        Focus();
    }
}

void MainWindow::OnTrayMenu(TrayIconMenu& menu)
{
    menu.Disabled(L"" APP_NAME);
    if (IsShown())
        menu.Button(L"Hide", std::bind_front(&MainWindow::Hide, this));
    else
        menu.Button(L"Show", std::bind_front(&MainWindow::Show, this));
    menu.Toggle(L"Enable", sApp.IsEnabled(), [] { sApp.Enable(!sApp.IsEnabled()); });
    menu.Button(L"Exit", std::bind_front(&MainWindow::Close, this));
}

void MainWindow::Draw()
{
    ApplyHookEvents();
    const bool noGames = sConfig.profiles.empty();
    if (!noGames && !sConfig.editingProfile) sConfig.SetEditingProfile(sConfig.profiles.front()->name.c_str());
    auto profile = sConfig.editingProfile; // own a reference: popups can delete it mid-frame
    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 o = GetWindowPos();

    DrawTitleBar(dl, o);
    if (noGames || _screen == Screen_AddGame)
        DrawAddGame(dl, o, noGames);
    else
        DrawEditor(dl, o, profile);

    // popups open and draw here, at the root of the id stack, whichever widget asked for them
    if (std::exchange(_askClose, false)) RequestPopup(kPopupClose);
    if (_pendingPopup) OpenPopup(std::exchange(_pendingPopup, nullptr));
    if (profile) {
        DrawKeyMenu(o, profile);
        DrawAppsPopup(o, profile);
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

    // game tabs; when they would run into the build label the longest names get cut short
    const auto lit = sApp.LastActiveProfile();
    boost::container::small_vector<float, 8> widths;
    for (const std::shared_ptr<Profile>& item : sConfig.profiles)
        widths.push_back(TabWidth(item->name.c_str()));
    const float widthCap = TabWidthCap(std::span<const float>(widths.data(), widths.size()));

    float x = kTabsX;
    for (size_t i = 0; i < sConfig.profiles.size(); i++) {
        const std::shared_ptr<Profile>& item = sConfig.profiles[i];
        const bool elided = widths[i] > widthCap;
        const std::string label = elided ? ElideTabLabel(item->name, widthCap) : std::string();
        const char* shown = elided ? label.c_str() : item->name.c_str();

        PushID(item.get());
        const bool selected = onEditor && item == sConfig.editingProfile;
        if (UiTab("##tab", o + ImVec2(x, kTabY), shown, selected, item == lit, !enabled)) {
            sConfig.SetEditingProfile(item->name.c_str());
            CloseAddGame();
        }
        // on release like ImGui's own context menus, so a right click on another tab moves an open menu there
        if (IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && IsMouseReleased(ImGuiMouseButton_Right)) {
            _tabMenuProfile = item;
            _tabMenuPos = ImVec2(GetItemRectMin().x, GetItemRectMax().y + 6.f);
            RequestPopup(kPopupTabMenu);
        }
        if (elided) Tip("%s", item->name.c_str());
        PopID();
        x += (elided ? TabWidth(shown) : widths[i]) + kTabGap;
    }

    // the "+" tab stands raised while the add screen it opens is up (not in first run, where there is no other)
    const ImVec2 addPos = o + ImVec2(x, kTabY);
    const ImVec2 addSize(kAddTabSize, kAddTabSize);
    bool addClicked;
    if (!noGames && _screen == Screen_AddGame) {
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

    // build date, or the newer build waiting on GitHub
    const float buildRight = 1148.f;
    if (sUpdater.IsUpdateAvailable()) {
        char update[48];
        snprintf(update, sizeof(update), "new build %s", sUpdater.LatestDate());
        SqueezeSpaces(update);
        const float textW = SilkWidth(update);
        const float glyphSize = 9.f;
        const float totalW = textW + 4.f + glyphSize;
        const ImVec2 pos = o + ImVec2(buildRight - totalW, 24.f);
        SetCursorScreenPos(pos - ImVec2(4.f, 6.f));
        if (InvisibleButton("##update", ImVec2(totalW + 8.f, 22.f))) sUpdater.OpenReleasePage();
        Tip("Open latest release on GitHub");
        AddSilk(dl, pos, update, UiCol::Signal);
        AddGlyph(dl, pos + ImVec2(textW + 4.f + glyphSize * .5f, 5.f), glyphSize, UiGlyph_Arrow, UiCol::Signal);
    } else {
        char build[32];
        snprintf(build, sizeof(build), "build %s", __DATE__);
        SqueezeSpaces(build);
        AddSilk(dl, o + ImVec2(buildRight - SilkWidth(build), 24.f), build);
    }

    if (UiIconButton("##gear", o + ImVec2(1171.f, 13.f), 30.f, UiGlyph_Gear)) RequestPopup(kPopupSettings);
    if (UiIconButton("##min", o + ImVec2(1201.f, 13.f), 30.f, UiGlyph_Minimize)) ShowWindow(Native(), SW_MINIMIZE);
    if (UiIconButton("##close", o + ImVec2(1231.f, 13.f), 30.f, UiGlyph_Close)) RequestClose();

    dl->AddRectFilled(o + ImVec2(0.f, 56.f), o + ImVec2(kWindowSize.x, 57.f), UiCol::Line);
}

void MainWindow::DrawEditor(ImDrawList* dl, const ImVec2& o, const std::shared_ptr<Profile>& profile)
{
    if (!profile) return;
    DrawStrip(dl, o, *profile);
    DrawKeyboard(dl, o, *profile);
    DrawRail(dl, o, *profile);
}

void MainWindow::DrawStrip(ImDrawList* dl, const ImVec2& o, Profile& profile)
{
    AddSilk(dl, o + ImVec2(24.f, 81.f), "runs in");

    // app tapes up to x=560; whatever doesn't fit collapses into a "+N" tape
    const float tapesX = 82.f, tapesMaxX = 560.f, tapeGap = 6.f, tapeY = 74.f;
    const size_t appCount = profile.apps.size();
    auto tapesEnd = [&](size_t count) {
        float end = tapesX;
        for (size_t i = 0; i < count; i++)
            end += TapeWidth(profile.apps.nth(i)->c_str()) + tapeGap;
        return end;
    };
    size_t shown = 0;
    while (shown < appCount && tapesEnd(shown) + TapeWidth(profile.apps.nth(shown)->c_str()) <= tapesMaxX)
        shown++;
    char more[16] = "";
    if (shown < appCount) {
        // the "+N" tape needs room too
        for (;;) {
            snprintf(more, sizeof(more), "+%d", (int)(appCount - shown));
            if (!shown || tapesEnd(shown) + TapeWidth(more) <= tapesMaxX) break;
            shown--;
        }
    }

    float x = tapesX;
    for (size_t i = 0; i < shown; i++) {
        const char* app = profile.apps.nth(i)->c_str();
        AddTape(dl, o + ImVec2(x, tapeY), app);
        x += TapeWidth(app) + tapeGap;
    }
    bool openApps = false;
    if (more[0]) {
        const ImVec2 pos = o + ImVec2(x, tapeY);
        const float w = TapeWidth(more);
        AddTape(dl, pos, more);
        SetCursorScreenPos(pos);
        openApps |= InvisibleButton("##moreapps", ImVec2(w, 24.f));
        std::string hidden;
        for (size_t i = shown; i < appCount; i++) {
            if (!hidden.empty()) hidden += '\n';
            hidden += *profile.apps.nth(i);
        }
        Tip("%s", hidden.c_str());
        x += w + tapeGap;
    }
    openApps |= UiOutlineButton("##addapp", o + ImVec2(x, tapeY), "+ App");
    if (openApps) {
        _appsPopupPos = ImVec2(GetItemRectMin().x, GetItemRectMax().y + 6.f);
        RequestPopup(kPopupApps);
    }

    // what the board shows right now, or why it isn't doing anything
    char note[128] = "";
    ImU32 noteCol = UiCol::Ink2;
    if (profile.apps.empty()) {
        ImStrncpy(note, "Not tied to a game yet \xE2\x80\x93 add its .exe", sizeof(note));
        noteCol = UiCol::Signal;
    } else if (!sApp.IsEnabled()) {
        const char* locks = profile.disableWin && profile.disableAltF4 ? " Win key and Alt+F4 stay blocked."
                            : profile.disableWin                      ? " Win key stays blocked."
                            : profile.disableAltF4                    ? " Alt+F4 stays blocked."
                                                                      : "";
        snprintf(note, sizeof(note), "Paused: every key acts normally.%s", locks);
    } else if (_editMods != KeyMod_None) {
        snprintf(note, sizeof(note), "What keys do while %s is held", ModsName(_editMods));
    }
    if (note[0]) {
        const float w = UiTextWidth(UiFonts::SemiBold, 11.f, note);
        dl->AddText(UiFonts::SemiBold, 11.f, o + ImVec2(906.f - w, 80.f), noteCol, note);
    }

    AddSilk(dl, o + ImVec2(918.f, 81.f), "modifiers");
    int layer = (int)(std::ranges::find(kLayerMods, _editMods) - std::begin(kLayerMods));
    if (layer >= (int)std::size(kLayerMods)) layer = 0;
    if (UiSegmented("##layer", o + ImVec2(996.f, 70.f), ImVec2(260.f, 32.f), kLayerLabels, layer, true))
        _editMods = kLayerMods[layer];
}

void MainWindow::DrawKeyboard(ImDrawList* dl, const ImVec2& o, Profile& profile)
{
    const ImVec2 wellMin = o + ImVec2(24.f, 116.f);
    const ImVec2 wellMax = o + ImVec2(1256.f, 532.f);
    dl->AddRectFilled(wellMin, wellMax, UiCol::Well, 16.f);
    dl->AddLine(o + ImVec2(40.f, 117.f), o + ImVec2(1240.f, 117.f), UiCol::WellEdge, 2.f);

    const MouseForm mouseForm = sConfig.mouse;
    const ImVec2 mouseOrigin = o + ImVec2(1072.f, 214.f);
    const ImVec2 areaMin = o + ImVec2(48.f, 140.f);
    const ImVec2 areaMax(mouseForm != MouseForm_Off ? mouseOrigin.x - 70.f : wellMax.x - 24.f, o.y + 504.f);
    const ImVec2 area = areaMax - areaMin;

    const std::span<const KeyboardKey> layout = GetKeyboardLayout(sConfig.form, sConfig.variant);
    ImVec2 grid(1.f, 1.f);
    for (const KeyboardKey& key : layout)
        grid = ImMax(grid, ImVec2(key.x + key.w, key.y + key.h));

    // the last column's face ends a gap short of the grid, the last row's lip a gap minus the lip short of it;
    // that trailing gap may hang past the area
    const ImVec2 trailing(kKeyGap.x, kKeyGap.y - kKeyLip);
    const float unit = ImFloor(ImMin((area.x + trailing.x) / grid.x, (area.y + trailing.y) / grid.y));
    const ImVec2 board = grid * unit - trailing;
    const ImVec2 origin = ImFloor(areaMin + ImMax(area - board, ImVec2(0.f, 0.f)) * .5f);

    // the click that closes a popup must not also edit the key under it
    static bool s_pressClosedPopup = false;
    if (IsMouseClicked(ImGuiMouseButton_Left))
        s_pressClosedPopup = IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

    KeyFrame frame = {
        .profile = &profile,
        .mods = _editMods,
        .heldMods = sKeyboard.TestModifiers() & (unsigned)(kKeyboardKeyModCount - 1),
        .nowTicks = GetTickCount(),
        .wndFocused = GetForegroundWindow() == Native(),
        .paused = !sApp.IsEnabled(),
        .clickBlocked = s_pressClosedPopup,
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

    if (mouseForm != MouseForm_Off) DrawMouse(dl, frame, mouseOrigin, mouseForm == MouseForm_5);
    DrawPressBadges(dl, frame.badges);

    char hint[160];
    if (frame.paused) {
        char keyName[48];
        if (const char* key = PauseKeyName(profile.vkPause, keyName, sizeof(keyName)))
            snprintf(hint, sizeof(hint),
                     "Changes still save while paused.  Press %s or the power button to resume.", key);
        else
            ImStrncpy(hint, "Changes still save while paused.  Press the power button to resume.", sizeof(hint));
    } else if (_editMods != KeyMod_None) {
        snprintf(hint, sizeof(hint),
                 "Pale keys act as with no modifier.  Click one to give it its own %s setting.", ModsName(_editMods));
    } else {
        ImStrncpy(hint,
                  "Click a key to toggle autofire.  Right-click for one tap, block and modifier combos.  "
                  "Hold a key to test it.",
                  sizeof(hint));
    }
    dl->AddText(UiFonts::Medium, 11.f, o + ImVec2(48.f, 508.f), UiCol::Ink3, hint);
}

void MainWindow::DrawKey(KeyFrame& frame, const char* id, const char* label, UINT vkCode, const ImVec2& pos,
                         const ImVec2& size, const float* radii)
{
    IM_ASSERT(vkCode < kKeyboardKeysCount);
    Profile& profile = *frame.profile;
    const unsigned short vk = (unsigned short)vkCode;
    const Profile::KeyModList_t& slots = profile.keys[vk];

    UiKeyDesc desc = {};
    desc.label = label;
    desc.kind = KeyKindOnLayer(profile, vk, frame.mods);
    desc.pressed = sKeyboard.IsPressed(vk);
    desc.paused = frame.paused;
    desc.centered = Keyboard::IsMouseButton(vk);
    if (radii) std::copy_n(radii, 4, desc.radii);
    const bool editable = desc.kind != UiKeyKind_Fixed && desc.kind != UiKeyKind_Locked;

    // on NoMod a key tells which modifiers have their own binding for it
    if (frame.mods == KeyMod_None && editable) {
        auto boundWith = [&](unsigned mod) {
            for (unsigned m = 1; m < kKeyboardKeyModCount; m++)
                if ((m & mod) && slots[m].action != Action_None) return true;
            return false;
        };
        if (boundWith(KeyMod_Shift)) desc.marks.push_back('S');
        if (boundWith(KeyMod_Ctrl)) desc.marks.push_back('C');
        if (boundWith(KeyMod_Alt)) desc.marks.push_back('A');
    }

    const bool firing = frame.wndFocused && desc.pressed && !frame.paused &&
                        ResolveKeyAction(profile, vk, frame.heldMods) == Action_Spammy;
    TickSimulatedPresses(vk, frame.nowTicks, profile.speed, firing);

    const bool clicked = vk == VK_MBUTTON && desc.kind == UiKeyKind_Normal
                             ? WheelBar(GetWindowDrawList(), id, pos, size, desc.pressed)
                             : UiKey(id, pos, size, desc);
    const ImVec2 keyMin = GetItemRectMin();
    const ImVec2 keyMax = GetItemRectMax();

    if (const unsigned rate = PressRate(vk, frame.nowTicks); rate > 2) {
        const ImVec2 anchor((keyMin.x + keyMax.x) * .5f, keyMin.y);
        frame.badges.push_back({anchor, rate, frame.nowTicks - _lastPressTick[vk]});
    }

    if (!editable) return;
    if (clicked && !frame.clickBlocked) {
        Action& action = profile.keys[vk][frame.mods].action;
        SetSetting(action, NextKeyAction(action, frame.mods));
    }
    // on release like ImGui's own context menus, so a right click on another key moves an open menu there
    if (IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && IsMouseReleased(ImGuiMouseButton_Right)) {
        _menuVk = vk;
        _menuKeyMin = keyMin;
        _menuKeyMax = keyMax;
        _menuShowCombos = false;
        RequestPopup(kPopupKeyMenu);
    }
}

void MainWindow::DrawMouse(ImDrawList* dl, KeyFrame& frame, const ImVec2& origin, bool sideButtons)
{
    AddCap(dl, origin + ImVec2(0.f, 100.f), ImVec2(136.f, 118.f), UiCapKind_Plain, kMouseBodyRadii);

    DrawKey(frame, "##mbL", "LMB", VK_LBUTTON, origin, ImVec2(62.f, 96.f), kMouseLeftRadii);
    DrawKey(frame, "##mbR", "RMB", VK_RBUTTON, origin + ImVec2(74.f, 0.f), ImVec2(62.f, 96.f), kMouseRightRadii);
    DrawKey(frame, "##mbM", "", VK_MBUTTON, origin + ImVec2(63.f, 26.f), ImVec2(10.f, 30.f));
    const float wheelLabelW = UiTextWidth(UiFonts::Medium, 9.f, "M3");
    dl->AddText(UiFonts::Medium, 9.f, origin + ImVec2(68.f - wheelLabelW * .5f, 62.f), UiCol::Ink3, "M3");

    if (sideButtons) {
        DrawKey(frame, "##mb5", "M5", VK_XBUTTON2, origin + ImVec2(10.f, 116.f), ImVec2(26.f, 30.f));
        DrawKey(frame, "##mb4", "M4", VK_XBUTTON1, origin + ImVec2(10.f, 154.f), ImVec2(26.f, 30.f));
    }
}

void MainWindow::DrawRail(ImDrawList* dl, const ImVec2& o, Profile& profile)
{
    const bool enabled = sApp.IsEnabled();
    for (const float x : {264.f, 776.f, 1024.f})
        dl->AddRectFilled(o + ImVec2(x, 554.f), o + ImVec2(x + 1.f, 664.f), UiCol::Line);

    // power and the pause key
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
    } else if (const char* key = PauseKeyName(profile.vkPause, keyName, sizeof(keyName))) {
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
        _capturingPauseKey = false;
        SetSetting(profile.vkPause, 0u);
    }
    if (IsItemHovered()) dl->AddLine(pausePos + ImVec2(0.f, 15.f), pausePos + ImVec2(pauseW, 15.f), pauseCol);
    Tip("Click, then press the key that pauses and resumes Spammy. Right-click to clear it.");
    dl->AddText(UiFonts::Medium, 12.f, pausePos, pauseCol, pauseText);

    // autofire rate
    char silk[96];
    snprintf(silk, sizeof(silk), "autofire rate \xC2\xB7 %s", profile.name.c_str());
    AddSilk(dl, o + ImVec2(288.f, 552.f), silk);

    const unsigned rate = SpeedToRate(profile.speed);
    // rates are whole numbers, so a slow drag would round back to where it started every frame; while the knob is
    // held it keeps its own exact angle
    static float s_knobT = 0.f;
    static bool s_knobHeld = false;
    float knobT = s_knobHeld ? s_knobT
                             : (float)(rate - kProfileRateMin) / (float)(kProfileRateMax - kProfileRateMin);
    if (UiKnob("##rate", o + ImVec2(344.f, 614.f), knobT, "5", "100", !enabled)) {
        const float picked = (float)kProfileRateMin + ImSaturate(knobT) * (float)(kProfileRateMax - kProfileRateMin);
        SetSetting(profile.speed, RateToSpeed((unsigned)lroundf(picked)));
    }
    s_knobHeld = IsItemActive();
    s_knobT = knobT;

    char number[8];
    snprintf(number, sizeof(number), "%u", SpeedToRate(profile.speed));
    const ImVec2 numberPos = o + ImVec2(420.f, 566.f);
    dl->AddText(UiFonts::Light, 72.f, numberPos, enabled ? UiCol::Ink : UiCol::Ink3, number);
    const float slashX = numberPos.x + UiTextWidth(UiFonts::Light, 72.f, number) + 5.f;
    dl->AddText(UiFonts::Medium, 22.f, ImVec2(slashX, o.y + 612.f), UiCol::Ink2, "/s");

    char caption[48];
    snprintf(caption, sizeof(caption), "one press every %u ms", profile.speed);
    // a three-digit rate pushes the caption right; it sits below the -/+ caps, so it may run under them
    const float captionX = ImMax(o.x + 548.f, slashX + UiTextWidth(UiFonts::Medium, 22.f, "/s") + 14.f);
    dl->AddText(UiFonts::Medium, 12.f, ImVec2(captionX, o.y + 618.f), UiCol::Ink3, caption);

    // step until the shown number changes: neighbouring intervals often round to the same rate
    auto stepRate = [&](int faster) {
        const unsigned current = SpeedToRate(profile.speed);
        for (int speed = (int)profile.speed - faster; speed >= kProfileSpeedMin && speed <= kProfileSpeedMax;
             speed -= faster) {
            if (SpeedToRate((unsigned)speed) != current) {
                SetSetting(profile.speed, (unsigned)speed);
                break;
            }
        }
    };
    if (UiCapIconButton("##slower", o + ImVec2(688.f, 580.f), 30.f, UiGlyph_Minus)) stepRate(-1);
    if (UiCapIconButton("##faster", o + ImVec2(726.f, 580.f), 30.f, UiGlyph_Plus)) stepRate(1);

    // locks
    snprintf(silk, sizeof(silk), "locks \xC2\xB7 %s", profile.name.c_str());
    AddSilk(dl, o + ImVec2(800.f, 552.f), silk);
    auto lockRow = [&](const char* id, float y, bool& flag, const char* label, const char* onNote,
                       const char* offNote) {
        if (UiToggle(id, o + ImVec2(800.f, y), flag)) SetSetting(flag, !flag);
        dl->AddText(UiFonts::SemiBold, 13.f, o + ImVec2(860.f, y + 1.f), flag ? UiCol::Ink : UiCol::Ink2, label);
        const char* note = !flag ? offNote : enabled ? onNote : "still blocked while paused";
        dl->AddText(UiFonts::Medium, 10.f, o + ImVec2(860.f, y + 17.f), UiCol::Ink3, note);
    };
    lockRow("##lockwin", 578.f, profile.disableWin, "Block Win key", "Start menu stays shut",
            "when on, Start menu stays shut");
    lockRow("##lockaltf4", 624.f, profile.disableAltF4, "Block Alt+F4", "sent as plain F4 instead",
            "when on, sent as plain F4");

    // legend for the layer on the board
    AddSilk(dl, o + ImVec2(1048.f, 552.f), "keys");
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
        snprintf(ownText, sizeof(ownText), "Own setting for %s", ModsName(_editMods));
        snprintf(blockedText, sizeof(blockedText), "Blocked with %s", ModsName(_editMods));
        rows[0] = {UiKeyKind_SameAuto, "Same as NoMod"};
        rows[1] = {UiKeyKind_Auto, ownText};
        rows[2] = {UiKeyKind_Block, blockedText};
    }
    for (int i = 0; i < 3; i++) {
        const float y = 574.f + 28.f * i;
        AddKeySwatch(dl, o + ImVec2(1048.f, y), rows[i].kind);
        dl->AddText(UiFonts::Medium, 12.f, o + ImVec2(1084.f, y + 4.f), UiCol::Ink2, rows[i].text);
    }
}

bool MainWindow::HandleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
    if (Window::HandleWndProc(msg, wParam, lParam, result)) return true;
    switch (msg) {
    case WM_USER_FOCUS:
        if (!IsShown()) Show();
        Focus();
        return true;
    case WM_SHOWWINDOW:
        if (wParam) _editMods = KeyMod_None;
        break;
    }
    return false;
}

const char* MainWindowUtil::PauseKeyName(unsigned keyBundle, char* buf, size_t bufSize)
{
    if (!keyBundle || !bufSize) return nullptr;
    const unsigned mods = KeyBundleMods(keyBundle);
    const char* name = Keyboard::GetKeyName((unsigned short)KeyBundleVk(keyBundle));
    if (!name || !name[0]) name = "Unknown";

    const int prefix = snprintf(buf, bufSize, "%s%s%s", (mods & KeyMod_Ctrl) ? "Ctrl+" : "",
                                (mods & KeyMod_Alt) ? "Alt+" : "", (mods & KeyMod_Shift) ? "Shift+" : "");
    size_t n = (size_t)ImClamp(prefix, 0, (int)bufSize - 1);
    // "Num Del" reads as one key as "NumDel"; "Mouse 4" keeps its space before the number
    for (const char* c = name; *c && n + 1 < bufSize; c++) {
        if (*c == ' ' && !isdigit((unsigned char)c[1])) continue;
        buf[n++] = *c;
    }
    buf[n] = '\0';
    return buf;
}

const char* MainWindowUtil::ModsName(unsigned mods)
{
    // indexed by the KeyMod bits: Shift 1, Alt 2, Ctrl 4
    static constexpr const char* kNames[] = {
        "no modifier", "Shift", "Alt", "Shift+Alt", "Ctrl", "Shift+Ctrl", "Ctrl+Alt", "Shift+Ctrl+Alt",
    };
    return kNames[mods & 7];
}

bool MainWindowUtil::MatchesFilter(std::string_view name, std::string_view filter)
{
    auto equalNoCase = [](char a, char b) { return tolower((unsigned char)a) == tolower((unsigned char)b); };
    return filter.empty() || !std::ranges::search(name, filter, equalNoCase).empty();
}
