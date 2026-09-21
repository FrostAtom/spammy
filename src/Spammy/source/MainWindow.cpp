#include "MainWindow.h"
#include "App.h"
#include "Config.h"
#include "KeyboardLayout.h"
#include "Modes.h"
#include "Resources.h"
#include "Updater.h"
#include "Utils.h"

using namespace ImGui;

namespace {
constexpr UINT WM_USER_FOCUS = WM_APP + 0x20;
constexpr ImVec2 kWindowSize(1280.f, 680.f);
constexpr ImVec2 kPanelMin(20.f, 134.f);
constexpr ImVec2 kPanelMax(1260.f, 660.f);
constexpr float kKeyGap = 7.f;
constexpr DWORD kRateWindowMs = 1000;

bool MatchesFilter(const std::string& name, const char* filter)
{
    if (!filter[0]) return true;
    auto it = std::search(name.begin(), name.end(), filter, filter + strlen(filter),
                          [](char a, char b) { return tolower((unsigned char)a) == tolower((unsigned char)b); });
    return it != name.end();
}

std::string EllipsizedAppList(const Profile::AppList_t& apps, float maxW)
{
    std::string text;
    for (const std::string& app : apps) {
        if (!text.empty()) text += ", ";
        text += app;
    }
    auto width = [](const std::string& s) { return UiFonts::Semi->CalcTextSizeA(18.f, FLT_MAX, 0.f, s.c_str()).x; };
    if (width(text) <= maxW) return text;
    while (!text.empty() && width(text + "...") > maxW) {
        text.pop_back();
        while (!text.empty() && ((unsigned char)text.back() & 0xC0) == 0x80) // finish removing a multi-byte char
            text.pop_back();
    }
    return text + "...";
}
} // namespace

MainWindow::MainWindow(const wchar_t* className, const wchar_t* wndName)
    : Window(className, wndName), _appFilePath(GetModulePath())
{
}

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

    if (ErrorCode ec = Window::Initialize(); ec != ErrorCode_OK) {
        char msg[256];
        snprintf(msg, std::size(msg), "%s (0x%08lX)", FormatError(ec), (unsigned long)LastError());
        MessageBoxA(NULL, msg, APP_NAME, MB_ICONERROR | MB_OK);
        return false;
    }

    HICON icon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_ICON1));
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
    SetTrayIconOverride(sApp.IsEnabled() ? NULL : _pausedIcon);
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
    if (!repeat && focused && vkCode < kKeyboardKeysCount) LogKeyPress(vkCode, GetTickCount());
    return !Keyboard::IsMouseButton(vkCode) && _editPause;
}

bool MainWindow::HandleKeyRelease(unsigned short vkCode)
{
    if (Keyboard::IsMouseButton(vkCode) || !_editPause) return false;
    if (auto profile = sConfig.editingProfile) {
        profile->vkPause = MAKE_KEY_BUNDLE(vkCode, sKeyboard.TestModifiers());
        sConfig.MarkDirty();
    }
    _editPause = false;
    return true;
}

void MainWindow::LogKeyPress(unsigned short vkCode, DWORD ticks)
{
    PressLog& log = _pressLog[vkCode];
    log[_pressHead[vkCode]++ % log.size()] = ticks;
    _pressTick[vkCode] = ticks;
}

unsigned MainWindow::PressRate(unsigned short vkCode, DWORD nowTicks) const
{
    return (unsigned)std::ranges::count_if(_pressLog[vkCode],
                                           [&](DWORD t) { return t && nowTicks - t <= kRateWindowMs; });
}

// autofire happens on the input worker, so the UI re-derives the simulated presses from the profile speed
void MainWindow::TickSimulatedPresses(unsigned short vkCode, DWORD nowTicks, unsigned speed, bool firing)
{
    DWORD& tick = _spamTick[vkCode];
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
    auto profile = sConfig.editingProfile;
    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 o = GetWindowPos();

    DrawTitleBar(dl, o);
    DrawHeader(dl, o, profile);
    DrawKeyboard(dl, o, profile);

    if (std::exchange(_askClose, false)) OpenPopup("##close");
    DrawClosePopup(o);
}

void MainWindow::DrawTitleBar(ImDrawList* dl, const ImVec2& o)
{
    AddAccentHairline(dl, o, kWindowSize.x, 2.f);
    AddLogoMark(dl, o + ImVec2(28.f, 20.f), 22.f);
    AddTrackedText(dl, UiFonts::Bold, 22.f, o + ImVec2(60.f, 22.f), UiCol::Text, "SPAMMY", 4.f);
    dl->AddText(UiFonts::Mono, 12.f, o + ImVec2(180.f, 29.f), UiCol::Mute, __DATE__);

    if (sUpdater.IsUpdateAvailable()) {
        char update[48];
        snprintf(update, sizeof(update), "NEW BUILD AVAIL %s", sUpdater.LatestDate());
        if (UiBadge("##update", o + ImVec2(272.f, 19.f), update, UiCol::Ok)) sUpdater.OpenReleasePage();
        Tip("Open latest release on GitHub");
    }

    if (UiGhostButton("##gear", o + ImVec2(1152.f, 17.f), 30.f, UiGlyph_Gear)) OpenPopup("##settings");
    if (UiGhostButton("##min", o + ImVec2(1188.f, 17.f), 30.f, UiGlyph_Minimize)) ShowWindow(Native(), SW_MINIMIZE);
    if (UiGhostButton("##close", o + ImVec2(1224.f, 17.f), 30.f, UiGlyph_Close)) RequestClose();

    DrawSettingsPopup(o);
}

void MainWindow::DrawHeader(ImDrawList* dl, const ImVec2& o, const std::shared_ptr<Profile>& profile)
{
    const ImVec2 chipSize(216.f, 46.f);
    if (UiChipFrame("##profile", o + ImVec2(28.f, 66.f), chipSize)) OpenPopup("##profiles");
    UiChipLabel(o + ImVec2(44.f, 73.f), "PROFILE");
    if (profile)
        dl->AddText(UiFonts::Semi, 20.f, o + ImVec2(44.f, 85.f), UiCol::Text, profile->name.c_str());
    else
        dl->AddText(UiFonts::Semi, 20.f, o + ImVec2(44.f, 85.f), UiFlashDanger(), "NOT SET");
    const float chevT = UiAnim(GetID("##profiles.chev"), IsPopupOpen("##profiles") ? 1.f : 0.f, 16.f);
    AddChevronDown(dl, o + ImVec2(222.f, 89.f), UiMixColor(UiCol::Sub, UiCol::Text, chevT), chevT);
    DrawProfilesPopup(o);

    if (profile) {
        if (UiChipFrame("##apps", o + ImVec2(260.f, 66.f), chipSize)) OpenPopup("##appsmenu");
        UiChipLabel(o + ImVec2(276.f, 73.f), "ENABLE ONLY IN APPS");
        if (profile->apps.empty())
            dl->AddText(UiFonts::Semi, 18.f, o + ImVec2(276.f, 86.f), UiFlashWarn(), "ALL APPS");
        else
            dl->AddText(UiFonts::Semi, 18.f, o + ImVec2(276.f, 86.f), UiCol::Text,
                        EllipsizedAppList(profile->apps, 184.f).c_str());
        DrawAppsPopup(o, profile);

        if (UiLockChip("##winkey", o + ImVec2(700.f, 66.f), ImVec2(110.f, 46.f), "WIN KEY", profile->disableWin)) {
            profile->disableWin = !profile->disableWin;
            sConfig.MarkDirty();
        }
        if (UiLockChip("##altf4", o + ImVec2(824.f, 66.f), ImVec2(120.f, 46.f), "ALT + F4", profile->disableAltF4)) {
            profile->disableAltF4 = !profile->disableAltF4;
            sConfig.MarkDirty();
        }
        DrawPauseKeyChip(dl, o + ImVec2(958.f, 66.f), *profile);
    }

    if (UiEnablePill("##enable", o + ImVec2(1122.f, 66.f), ImVec2(132.f, 46.f), sApp.IsEnabled()))
        sApp.Enable(!sApp.IsEnabled());
}

void MainWindow::DrawPauseKeyChip(ImDrawList* dl, const ImVec2& pos, Profile& profile)
{
    const ImVec2 size(150.f, 46.f);
    if (UiChipFrame("##pause", pos, size)) {
        if (_editPause) {
            _editPause = false;
        } else {
            profile.vkPause = 0;
            sConfig.MarkDirty();
            sKeyboard.Attach(true);
            _editPause = true;
        }
    }
    UiChipLabel(pos + ImVec2(16.f, 7.f), "PAUSE KEY");

    char keycap[32] = "NOT SET";
    ImU32 keycapCol = UiFlashDanger();
    if (_editPause) {
        strcpy(keycap, "PRESS KEY");
        keycapCol = GetColorU32(FlashColor(1.f, .55f, .28f, 1.5f, .4f, 1.f));
    } else if (profile.vkPause) {
        const char* keyName = Keyboard::GetKeyName(GET_KEY_VKCODE(profile.vkPause));
        if (!keyName[0]) keyName = "UNKNOWN";
        const size_t len = std::min(strlen(keyName), sizeof(keycap) - 1);
        std::transform(keyName, keyName + len, keycap, [](unsigned char c) { return (char)toupper(c); });
        keycap[len] = '\0';
        keycapCol = UiCol::Sub;
    }
    AddKeycap(dl, pos + ImVec2(16.f, 19.f), pos + ImVec2(94.f, 37.f), keycap, keycapCol);
    if (_editPause) {
        const float p = 0.5f + 0.5f * sinf((float)GetTime() * 4.f);
        AddGlow(dl, pos, pos + size, UiCol::Spam, 10.f, 6, 0.08f + 0.14f * p);
    }
}

void MainWindow::DrawKeyboard(ImDrawList* dl, const ImVec2& o, const std::shared_ptr<Profile>& profile)
{
    const ImVec2 panelMin = o + kPanelMin;
    const ImVec2 panelMax = o + kPanelMax;
    AddPanel(dl, panelMin, panelMax, 16.f);

    const MouseForm mouseForm = sConfig.mouse;
    const float mouseW = 190.f;
    const float mouseLeft = panelMax.x - 16.f - mouseW;
    const ImVec2 start = panelMin + ImVec2(16.f, 104.f);
    const ImVec2 avail((mouseForm != MouseForm_Off ? mouseLeft - 24.f : panelMax.x - 16.f) - start.x,
                       panelMax.y - panelMin.y - 120.f);

    std::span<const KeyboardKey> layout = GetKeyboardLayout(sConfig.form, sConfig.variant);
    ImVec2 grid(0.f, 0.f);
    for (const KeyboardKey& item : layout)
        grid = ImMax(grid, ImVec2(item.x + item.w, item.y + item.h));

    const float keySize = ImFloor(ImMin((avail.x + kKeyGap) / grid.x, (avail.y + kKeyGap) / grid.y));
    const ImVec2 origin = start + (avail - (grid * keySize - ImVec2(kKeyGap, kKeyGap))) * .5f;

    // mouse gesture state carried across frames: paint on drag / click, erase with right button
    static struct {
        UINT pressVk = 0;
        bool pressInPanel = false;
        bool rightInPanel = false;
        bool brushMoved = false;
        bool leftDismiss = false; // the click that closes a popup must not paint
    } s_gesture;

    const bool anyPopup = IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    const ImGuiWindow* hoveredWnd = GImGui->HoveredWindow;
    const bool overPopup = anyPopup && hoveredWnd && (hoveredWnd->Flags & ImGuiWindowFlags_Popup);
    const bool inPanel = !overPopup && IsMouseHoveringRect(panelMin, panelMax);
    if (IsMouseClicked(ImGuiMouseButton_Left)) {
        s_gesture.leftDismiss = anyPopup;
        s_gesture.pressInPanel = inPanel;
        s_gesture.pressVk = 0;
        s_gesture.brushMoved = false;
    }
    if (IsMouseClicked(ImGuiMouseButton_Right)) s_gesture.rightInPanel = inPanel;
    const bool brushing =
        s_gesture.pressInPanel && !s_gesture.leftDismiss && IsMouseDragging(ImGuiMouseButton_Left, 4.f);
    if (brushing) s_gesture.brushMoved = true;
    const bool releasedLeft = IsMouseReleased(ImGuiMouseButton_Left);
    const bool erasingRight = s_gesture.rightInPanel && IsMouseDown(ImGuiMouseButton_Right);

    struct PressBadge {
        ImVec2 pos;
        unsigned rate;
        DWORD age;
    };
    boost::container::small_vector<PressBadge, 32> badges;
    const DWORD nowTicks = GetTickCount();
    const bool wndFocused = GetForegroundWindow() == Native();

    const unsigned mods = _editMods;
    const KeyMode* brushMode = FindKeyMode(_brushAction);
    const UiKeyStyle brushPreview = brushMode ? brushMode->keyStyle : UiKeyStyle_None;

    auto keyItem = [&](const char* id, const char* label, UINT vkCode, const ImVec2& pos, const ImVec2& size) {
        IM_ASSERT(vkCode < kKeyboardKeysCount);
        UiKeyDesc desc = {};
        desc.label = label;
        desc.locked = sKeyboard.IsModifier(vkCode) || vkCode == VK_LWIN || vkCode == VK_RWIN;
        desc.pressed = sKeyboard.IsPressed(vkCode) != 0;
        if (profile) {
            desc.preview = brushPreview;
            bool inherited = false;
            const KeyMode* mode = FindKeyMode(ResolveKeyAction(*profile, vkCode, mods, &inherited));
            desc.inherited = inherited;
            if (mode) desc.style = mode->keyStyle;

            if (mods == KeyMod_None) {
                const auto& layers = profile->keys[vkCode];
                for (const KeyMode& layerMode : KeyModes()) {
                    const bool onSomeLayer = std::any_of(layers.begin() + 1, layers.end(), [&](const KeyConfig& c) {
                        return c.action == layerMode.action;
                    });
                    if (onSomeLayer) desc.dots[desc.dotCount++] = layerMode.menuColor;
                }
            }
            TickSimulatedPresses(vkCode, nowTicks, profile->speed, wndFocused && desc.pressed && mode && mode->onTick);
        }

        UiKey(id, pos, size, desc);
        const ImVec2 keyMin = GetItemRectMin();
        const ImVec2 keyMax = GetItemRectMax();

        if (const unsigned rate = PressRate(vkCode, nowTicks); rate > 2)
            badges.push_back({ImVec2((keyMin.x + keyMax.x) * .5f, keyMin.y), rate, nowTicks - _pressTick[vkCode]});

        const bool hoverKey = IsMouseHoveringRect(keyMin, keyMax);
        if (s_gesture.pressInPanel && hoverKey && IsMouseClicked(ImGuiMouseButton_Left)) s_gesture.pressVk = vkCode;

        if (profile && !desc.locked && hoverKey) {
            KeyConfig& config = profile->keys[vkCode][mods];
            auto paint = [&](Action action) {
                if (config.action == action) return;
                config.action = action;
                sConfig.MarkDirty();
            };
            const bool clickedHere =
                releasedLeft && !s_gesture.leftDismiss && !s_gesture.brushMoved && s_gesture.pressVk == vkCode;
            if (brushing || clickedHere) paint(_brushAction);
            if (erasingRight) paint(Action_None);
        }
    };

    for (size_t i = 0; i < layout.size(); i++) {
        const KeyboardKey& key = layout[i];
        PushID((int)i);
        keyItem("##key", key.name, key.vkCode, origin + ImVec2(key.x, key.y) * keySize,
                ImVec2(key.w, key.h) * keySize - ImVec2(kKeyGap, kKeyGap));
        PopID();
    }

    if (mouseForm != MouseForm_Off) {
        const ImVec2 bodySize(150.f, 270.f);
        const ImVec2 body(mouseLeft + (mouseW - bodySize.x) * .5f + 8.f,
                          panelMin.y + (panelMax.y - panelMin.y - bodySize.y) * .5f);
        auto bp = [&](float x, float y) { return body + ImVec2(x, y); };
        auto bodyPath = [&] {
            dl->PathArcTo(bp(26.f, 26.f), 26.f, IM_PI, IM_PI * 1.5f);
            dl->PathArcTo(bp(124.f, 26.f), 26.f, IM_PI * 1.5f, IM_PI * 2.f);
            dl->PathBezierCubicCurveTo(bp(151.f, 86.f), bp(133.f, 119.f), bp(133.f, 151.f));
            dl->PathBezierCubicCurveTo(bp(133.f, 178.f), bp(151.f, 189.f), bp(150.f, 224.f));
            dl->PathBezierCubicCurveTo(bp(149.f, 256.f), bp(123.f, 270.f), bp(75.f, 270.f));
            dl->PathBezierCubicCurveTo(bp(27.f, 270.f), bp(1.f, 256.f), bp(0.f, 224.f));
            dl->PathBezierCubicCurveTo(bp(-1.f, 189.f), bp(17.f, 178.f), bp(17.f, 151.f));
            dl->PathBezierCubicCurveTo(bp(17.f, 119.f), bp(-1.f, 86.f), bp(0.f, 26.f));
        };
        bodyPath();
        dl->PathFillConcave(UiCol::PanelTop);
        bodyPath();
        dl->PathStroke(UiCol::Stroke, ImDrawFlags_Closed, 1.5f);

        // RGB led strip along the lower body, drawn as a wide dim line under a thin bright one
        const float now = (float)GetTime();
        const int stripSegs = 28;
        ImVec2 stripPrev;
        for (int i = 0; i <= stripSegs; i++) {
            const float a = IM_PI * (.15f + .7f * (float)i / stripSegs);
            const ImVec2 pt = bp(75.f + cosf(a) * 77.f, 200.f + sinf(a) * 62.f);
            if (i) {
                const ImU32 led = UiHsvColor((float)i / stripSegs * .6f - now * .1f, .9f, 1.f) & ~IM_COL32_A_MASK;
                dl->AddLine(stripPrev, pt, led | IM_COL32(0, 0, 0, 70), 8.f);
                dl->AddLine(stripPrev, pt, led | IM_COL32(0, 0, 0, 230), 3.f);
            }
            stripPrev = pt;
        }

        dl->AddBezierQuadratic(bp(10.f, 104.f), bp(75.f, 113.f), bp(140.f, 104.f), UiCol::StrokeSoft, 1.5f);

        keyItem("##mbL", "LMB", VK_LBUTTON, bp(8.f, 8.f), ImVec2(53.f, 92.f));
        keyItem("##mbM", "M3", VK_MBUTTON, bp(63.f, 22.f), ImVec2(24.f, 64.f));
        keyItem("##mbR", "RMB", VK_RBUTTON, bp(89.f, 8.f), ImVec2(53.f, 92.f));
        if (mouseForm == MouseForm_5) {
            keyItem("##mb5", "M5", VK_XBUTTON2, bp(6.f, 122.f), ImVec2(24.f, 38.f));
            keyItem("##mb4", "M4", VK_XBUTTON1, bp(6.f, 164.f), ImVec2(24.f, 38.f));
        }
    }

    for (const PressBadge& badge : badges) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u/s", badge.rate);
        const float pop = badge.age < 120 ? 1.3f - .3f * (badge.age / 120.f) : 1.f;
        const float alpha = badge.age > 700 ? 1.f - (badge.age - 700) / 300.f : 1.f;
        const ImVec2 ts = UiFonts::Bold->CalcTextSizeA(24.f * pop, FLT_MAX, 0.f, buf);
        const ImVec2 bmin(badge.pos.x - ts.x * .5f - 10.f, badge.pos.y - ts.y - 16.f);
        const ImVec2 bmax(badge.pos.x + ts.x * .5f + 10.f, badge.pos.y - 6.f);
        dl->AddRectFilled(bmin, bmax, UiWithAlpha(UiCol::KeyCap, alpha), 8.f);
        dl->AddRect(bmin, bmax, UiWithAlpha(UiCol::Spam, alpha), 8.f, 0, 2.f);
        dl->AddText(UiFonts::Bold, 24.f * pop, ImVec2(badge.pos.x - ts.x * .5f, bmin.y + 5.f),
                    UiWithAlpha(UiCol::SpamText, alpha), buf);
    }

    const float titleY = panelMin.y + 10.f;
    const float descY = panelMin.y + 26.f;
    const float barY = panelMin.y + 42.f;

    auto groupTitle = [&](float x, float w, const char* text) {
        const ImVec2 size = CalcTrackedTextSize(UiFonts::Semi, 12.f, text, 1.5f);
        AddTrackedText(dl, UiFonts::Semi, 12.f, ImVec2(x + (w - size.x) * .5f, titleY), UiCol::Text, text, 1.5f);
    };
    auto groupFade = [&](float x, float w, ImU32 accent) {
        const ImU32 gradTop = UiWithAlpha(accent, 0.2f);
        const ImU32 gradClear = UiWithAlpha(accent, 0.f);
        dl->AddRectFilledMultiColor(ImVec2(x - 3.f, panelMin.y), ImVec2(x + w + 3.f, panelMin.y + 96.f), gradTop,
                                    gradTop, gradClear, gradClear);
    };
    auto blockDesc = [&](float x, float w, const char* desc) {
        const float descW = UiFonts::Mono->CalcTextSizeA(13.f, FLT_MAX, 0.f, desc).x;
        dl->AddText(UiFonts::Mono, 13.f, ImVec2(x + (w - descW) * .5f, descY), UiCol::Mute, desc);
    };

    const float allX = panelMin.x + 16.f;
    const float modsX = allX + 176.f;
    groupFade(allX, 362.f, UiCol::Spam);
    groupTitle(allX, 362.f, "GLOBAL / OVERRIDE LAYERS");
    blockDesc(allX, 170.f, "paint the base layer");
    if (UiBrushChip("##layerall", ImVec2(allX, barY), ImVec2(170.f, 22.f), "GLOBAL", UiCol::Spam, _editMods == 0))
        _editMods = 0;

    static constexpr struct {
        const char* name;
        unsigned mod;
    } s_layers[] = {{"SHIFT", KeyMod_Shift}, {"CTRL", KeyMod_Ctrl}, {"ALT", KeyMod_Alt}};
    blockDesc(modsX, 186.f, "only when modifier held");
    float layerX = modsX;
    for (const auto& layer : s_layers) {
        PushID((int)layer.mod);
        if (UiBrushChip("##layer", ImVec2(layerX, barY), ImVec2(58.f, 22.f), layer.name, UiCol::Spam,
                        _editMods & layer.mod))
            _editMods ^= layer.mod;
        PopID();
        layerX += 64.f;
    }
    if (_editMods == 0 && _brushAction == Action_Disabled) _brushAction = Action_Spammy;

    const char* hint = "LMB paint / RMB erase";
    const float hintW = UiFonts::Mono->CalcTextSizeA(12.f, FLT_MAX, 0.f, hint).x;
    dl->AddText(UiFonts::Mono, 12.f, ImVec2((panelMin.x + panelMax.x - hintW) * .5f, titleY), UiCol::Text, hint);

    const float colW = 170.f;
    const int modeCount = _editMods ? 3 : 2;
    const float modesW = modeCount * colW + (modeCount - 1) * 6.f;
    groupFade(panelMax.x - 16.f - modesW, modesW, brushMode ? brushMode->menuColor : UiCol::Spam);
    groupTitle(panelMax.x - 16.f - modesW, modesW, "BRUSH");

    float modeX = panelMax.x - 16.f;
    auto brushChip = [&](Action act) {
        const KeyMode* mode = FindKeyMode(act);
        modeX -= colW;
        const float colX = modeX;
        const float chipW = 140.f;
        const float chipX = colX + (colW - chipW) * .5f;
        PushID((int)act);
        if (UiBrushChip("##brush", ImVec2(chipX, barY), ImVec2(chipW, 22.f), mode->name, mode->menuColor,
                        _brushAction == act))
            _brushAction = act;
        PopID();
        blockDesc(colX, colW, mode->desc);
        modeX -= 6.f;
        return chipX;
    };
    brushChip(Action_Speedy);
    const float spammyX = brushChip(Action_Spammy);
    if (_editMods) brushChip(Action_Disabled);

    if (profile) {
        const float rateY = barY + 26.f;
        const int speed = (int)profile->speed;
        int edited = speed;
        if (UiGhostButton("##ratedec", ImVec2(spammyX, rateY), 22.f, UiGlyph_Minus)) edited--;
        PushFont(UiFonts::Mono, 12.f);
        PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.f, 5.f));
        SetCursorScreenPos(ImVec2(spammyX + 26.f, rateY));
        SetNextItemWidth(88.f);
        SliderInt("##rate", &edited, PROFILE_SPEED_MIN, PROFILE_SPEED_MAX, "%d ms");
        PopStyleVar();
        PopFont();
        if (UiGhostButton("##rateinc", ImVec2(spammyX + 118.f, rateY), 22.f, UiGlyph_Plus)) edited++;
        edited = ImClamp(edited, PROFILE_SPEED_MIN, PROFILE_SPEED_MAX);
        if (edited != speed) {
            profile->speed = (unsigned)edited;
            sConfig.MarkDirty();
        }
    }
}

void MainWindow::DrawProfilesPopup(const ImVec2& o)
{
    SetNextWindowPos(o + ImVec2(28.f, 116.f));
    SetNextWindowSizeConstraints(ImVec2(260.f, 0.f), ImVec2(260.f, 544.f));
    SetNextWindowSize(ImVec2(260.f, 0.f));
    if (!UiBeginPopup("##profiles")) return;

    static char s_newName[64] = {0};
    static bool s_creating = false;
    static bool s_focusName = false;
    static std::string s_armedDelete;
    if (IsWindowAppearing()) {
        s_newName[0] = '\0';
        s_creating = false;
        s_armedDelete.clear();
    }

    ImDrawList* dl = GetWindowDrawList();
    const auto editing = sConfig.editingProfile;
    const char* pendingDelete = NULL;
    for (const std::shared_ptr<Profile>& item : sConfig.profiles) {
        PushID(item.get());
        const ImVec2 p = GetCursorScreenPos();
        const bool selected = item == editing;
        if (Selectable("##prow", selected, 0, ImVec2(200.f, 24.f))) sConfig.SetEditingProfile(item->name.c_str());
        dl->AddText(UiFonts::Semi, 18.f, p + ImVec2(8.f, 4.f), selected ? UiCol::SpamText : UiCol::Text,
                    item->name.c_str());
        char count[16];
        snprintf(count, sizeof(count), "%d", (int)item->apps.size());
        const ImVec2 countSize = UiFonts::Mono->CalcTextSizeA(13.f, FLT_MAX, 0.f, count);
        dl->AddText(UiFonts::Mono, 13.f, ImVec2(p.x + 200.f - countSize.x - 8.f, p.y + 6.f), UiCol::Mute, count);

        // delete needs a second click while the button flashes
        const bool armed = s_armedDelete == item->name;
        if (UiGhostButton("##del", p + ImVec2(208.f, 0.f), 24.f, UiGlyph_Close)) {
            if (armed)
                pendingDelete = item->name.c_str();
            else
                s_armedDelete = item->name;
        }
        if (armed)
            dl->AddRect(p + ImVec2(208.f, 0.f), p + ImVec2(232.f, 24.f),
                        GetColorU32(FlashColor(1.f, .3f, .37f, 1.5f, .5f, 1.f)), 6.f);
        PopID();
    }
    if (!sConfig.profiles.empty()) Separator();

    if (!s_creating) {
        if (UiMenuRow("+  NEW PROFILE", 0, false, true)) {
            s_creating = true;
            s_focusName = true;
        }
    } else {
        SetNextItemWidth(-52.f);
        if (s_focusName) {
            SetKeyboardFocusHere();
            s_focusName = false;
        }
        const bool enter = InputTextWithHint("##newname", "profile name", s_newName, sizeof(s_newName),
                                             ImGuiInputTextFlags_EnterReturnsTrue);
        const bool valid = strlen(s_newName) >= 3 && !sConfig.IsProfileExists(s_newName);
        SameLine(0.f, 6.f);
        BeginDisabled(!valid);
        const bool ok = Button("OK", ImVec2(-FLT_MIN, 0.f));
        EndDisabled();
        if ((enter || ok) && valid) {
            sConfig.CreateProfile(s_newName);
            CloseCurrentPopup();
        }
    }

    if (pendingDelete) sApp.DeleteProfile(pendingDelete);
    UiEndPopup();
}

void MainWindow::DrawAppsPopup(const ImVec2& o, const std::shared_ptr<Profile>& profile)
{
    SetNextWindowPos(o + ImVec2(260.f, 116.f));
    SetNextWindowSize(ImVec2(280.f, 0.f));
    if (!UiBeginPopup("##appsmenu")) return;

    static char s_search[64] = {0};
    static boost::container::flat_set<std::string, CaseInsensitiveLess> s_runningApps;
    if (IsWindowAppearing()) {
        s_search[0] = '\0';
        s_runningApps.clear();
        EnumWindows([this](HWND hwnd) -> BOOL {
            const std::filesystem::path path = GetProcessPath(hwnd);
            if (path.has_filename() && path != _appFilePath) s_runningApps.emplace(Utf8FileName(path));
            return TRUE;
        });
        SetKeyboardFocusHere();
    }
    SetNextItemWidth(-FLT_MIN);
    InputTextWithHint("##appsearch", "search", s_search, sizeof(s_search));

    SetNextWindowSizeConstraints(ImVec2(0.f, 0.f), ImVec2(FLT_MAX, 490.f));
    BeginChild("##applist", ImVec2(0.f, 0.f), ImGuiChildFlags_AutoResizeY);

    PushFont(UiFonts::Semi, 14.f);
    if (!profile->apps.empty()) {
        TextDisabled("BOUND");
        const char* unbindApp = NULL;
        for (const std::string& app : profile->apps) {
            if (!MatchesFilter(app, s_search)) continue;
            PushID(app.c_str());
            const ImVec2 p = GetCursorScreenPos();
            const float w = GetContentRegionAvail().x;
            UiMenuRow(app.c_str(), UiCol::Spam, false, true, true);
            if (UiGhostButton("##unbind", ImVec2(p.x + w - 24.f, p.y), 24.f, UiGlyph_Close)) unbindApp = app.c_str();
            PopID();
        }
        if (unbindApp) sConfig.UnbindProfile(profile->name.c_str(), unbindApp);
        Separator();
    }

    TextDisabled("RUNNING");
    int shown = 0;
    for (const std::string& app : s_runningApps) {
        if (!MatchesFilter(app, s_search) || profile->apps.contains(app)) continue;
        const auto appProfile = sConfig.FindProfileByApp(app.c_str());
        const bool boundElsewhere = appProfile && appProfile != profile;
        PushID(app.c_str());
        if (UiMenuRow(app.c_str(), 0, boundElsewhere, true)) sConfig.BindProfile(profile->name.c_str(), app.c_str());
        PopID();
        shown++;
    }
    if (!shown) TextDisabled(s_search[0] ? "no matches" : "nothing running");
    PopFont();
    EndChild();

    UiEndPopup();
}

void MainWindow::DrawSettingsPopup(const ImVec2& o)
{
    SetNextWindowPos(o + ImVec2(1252.f, 52.f), ImGuiCond_Always, ImVec2(1.f, 0.f));
    SetNextWindowSize(ImVec2(230.f, 0.f));
    if (!UiBeginPopup("##settings")) return;

    static bool s_autoStart = false;
    static int s_uiSize = 0;
    if (IsWindowAppearing()) {
        s_autoStart = sApp.IsAutoStartEnabled();
        s_uiSize = (int)sConfig.uiSize;
    }

    auto toggleSetting = [](const char* id, const char* label, auto& value) {
        if (!UiToggleRow(id, label, value)) return;
        value = !value;
        sConfig.MarkDirty();
    };
    auto stepEnum = [](const char* id, const char* label, auto name, int count, auto& value) {
        int index = (int)value;
        if (!UiStepperRow(id, label, name(value), count, index)) return;
        value = (std::remove_reference_t<decltype(value)>)index;
        sConfig.MarkDirty();
    };

    if (UiToggleRow("##autostart", "AUTO START", s_autoStart) && sApp.EnableAutoStart(!s_autoStart))
        s_autoStart = !s_autoStart;
    toggleSetting("##sounds", "ENABLE SOUNDS", sConfig.soundsEnabled);
    stepEnum("##close", "ON CLOSE", CloseActionName, CloseAction_Count, sConfig.closeAction);

    Separator();

    stepEnum("##form", "LAYOUT", KeyboardFormName, KeyboardForm_Count, sConfig.form);
    stepEnum("##variant", "VARIANT", KeyboardVariantName, KeyboardVariant_Count, sConfig.variant);
    stepEnum("##mouse", "MOUSE", MouseFormName, MouseForm_Count, sConfig.mouse);
    // rescaling the window mid-drag would fight the stepper, so apply on release
    bool released = false;
    UiStepperRow("##uisize", "UI SIZE", UiSizeName((UiSize)s_uiSize), UiSize_Count, s_uiSize, &released);
    if (released && (UiSize)s_uiSize != sConfig.uiSize) {
        sConfig.uiSize = (UiSize)s_uiSize;
        sConfig.MarkDirty();
        SetScaleFactor(UiSizeFactor(sConfig.uiSize));
    }

    Separator();
    if (UiMenuRow("GITHUB")) LaunchUrl(L"https://github.com/FrostAtom/spammy");

    UiEndPopup();
}

void MainWindow::DrawClosePopup(const ImVec2& o)
{
    const float contentW = 300.f;
    SetNextWindowPos(o + kWindowSize * .5f, ImGuiCond_Always, ImVec2(.5f, .5f));
    SetNextWindowSize(ImVec2(contentW + 20.f, 0.f));
    if (!UiBeginModal("##close")) return;

    static bool s_remember = false;
    if (IsWindowAppearing()) s_remember = false;

    ImDrawList* dl = GetWindowDrawList();
    const ImVec2 p = GetCursorScreenPos();
    AddTrackedText(dl, UiFonts::Semi, 12.f, p + ImVec2(8.f, 4.f), UiCol::Sub, "CLOSE WINDOW", 1.5f);
    dl->AddText(UiFonts::Semi, 20.f, p + ImVec2(8.f, 20.f), UiCol::Text, "Hide to tray or exit?");
    dl->AddText(UiFonts::Mono, 13.f, p + ImVec2(8.f, 46.f), UiCol::Mute, "hidden, spammy keeps running");

    const ImVec2 btnSize((contentW - 12.f) * .5f, 36.f);
    CloseAction chosen = CloseAction_Ask;
    if (UiDialogButton("##hide", p + ImVec2(0.f, 74.f), btnSize, "HIDE TO TRAY", UiCol::Spam))
        chosen = CloseAction_Hide;
    if (UiDialogButton("##exit", p + ImVec2(btnSize.x + 12.f, 74.f), btnSize, "EXIT", UiCol::Danger))
        chosen = CloseAction_Exit;

    SetCursorScreenPos(p + ImVec2(0.f, 122.f));
    Separator();
    if (UiToggleRow("##remember", "REMEMBER CHOICE", s_remember, contentW)) s_remember = !s_remember;

    // a modal ignores clicks outside and Escape by itself, so dismiss it the same way the other popups go away;
    // AllowWhenBlockedByActiveItem, or pressing any control inside would count as "outside" on the press frame
    const bool clickedOutside = IsMouseClicked(ImGuiMouseButton_Left) && !IsWindowAppearing() &&
                                !IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (clickedOutside || IsKeyPressed(ImGuiKey_Escape)) CloseCurrentPopup();

    if (chosen != CloseAction_Ask) {
        if (s_remember) {
            sConfig.closeAction = chosen;
            sConfig.MarkDirty();
        }
        CloseCurrentPopup();
        if (chosen == CloseAction_Hide)
            Hide();
        else
            Close();
    }
    UiEndPopup();
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
        if (wParam) _editMods = 0;
        break;
    }
    return false;
}
