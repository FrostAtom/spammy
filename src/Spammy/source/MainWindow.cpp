#include "MainWindow.h"
#include "App.h"
#include "Config.h"
#include "Modes.h"
#include "Resources.h"
#include "Updater.h"

using namespace ImGui;

MainWindow* MainWindow::_self = NULL;
MainWindow& MainWindow::Instance()
{
    return *_self;
}

MainWindow::MainWindow(const wchar_t* className, const wchar_t* wndName) : Window(className, wndName), _editPause(false)
{
    _self = this;

    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, path, std::size(path));
    _appFilePath = path;
}

MainWindow::~MainWindow() {}

bool MainWindow::Initialize()
{
    HWND hwnd = FindWindowW(L"" APP_NAME, L"" APP_NAME);
    if (hwnd) {
        PostMessageW(hwnd, WM_USER_FOCUS, NULL, NULL);
        return false;
    }

    ErrorCode ec = Window::Initialize();
    if (ec != ErrorCode_OK) {
        MessageBoxA(NULL, FormatError(ec), APP_NAME, MB_ICONERROR | MB_OK);
        return false;
    }

    SetIcon(IDI_ICON1);
    EnableMoving();
    EnableTitleBar(false);
    SetSize({1280, 720});
    ResetPosition();

    TrayIcon* trayIcon = new TrayIcon();
    trayIcon->SetTip(L"" APP_NAME);
    trayIcon->SetOnClick(std::bind_front(&MainWindow::OnTrayClick, this));
    trayIcon->SetMenu(std::bind_front(&MainWindow::OnTrayMenu, this));
    SetTrayIcon(trayIcon);
    if (!sConfig.showTrayIcon) ShowTrayIcon(false);

    LoadStyle();

    return true;
}

bool MainWindow::HandleKeyPress(unsigned short vkCode, bool repeat)
{
    if (!repeat && vkCode < KEYBOARD_KEYS_COUNT && GetForegroundWindow() == Native())
        LogKeyPress(vkCode, GetTickCount());
    if (Keyboard::IsMouseButton(vkCode)) return false;
    if (_editPause) return true;
    return false;
}

void MainWindow::LogKeyPress(unsigned short vkCode, DWORD ticks)
{
    _pressLog[vkCode][_pressHead[vkCode]++ % _pressLog[vkCode].size()] = ticks;
    _pressTick[vkCode] = ticks;
}

bool MainWindow::HandleKeyRelease(unsigned short vkCode, bool)
{
    if (Keyboard::IsMouseButton(vkCode)) return false;
    if (_editPause) {
        if (auto profile = sConfig.editingProfile) {
            profile->vkPause = MAKE_KEY_BUNDLE(vkCode, sKeyboard.TestModifiers());
            sConfig.MarkDirty();
        }
        _editPause = false;
        return true;
    }
    return false;
}

void MainWindow::OnTrayClick()
{
    if (!IsShown()) {
        Show();
        Focus();
    } else {
        Hide();
    }
}

void MainWindow::OnTrayMenu(TrayIconMenu& menu)
{
    menu.Disabled(L"" APP_NAME);
    if (!IsShown())
        menu.Button(L"Show", std::bind_front(&MainWindow::Show, this));
    else
        menu.Button(L"Hide", std::bind_front(&MainWindow::Hide, this));
    menu.Toggle(L"Enable", sApp.IsEnabled(), [] { sApp.Enable(!sApp.IsEnabled()); });
    menu.Button(L"Exit", std::bind_front(&MainWindow::Close, this));
}

void MainWindow::LoadStyle()
{
    ImGui::LoadUiStyle();
}

bool MainWindow::BeginFrame()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    bool result = Window::BeginFrame();
    ImGui::PopStyleVar();
    return result;
}

void MainWindow::Draw()
{
    auto profile = sConfig.editingProfile;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetWindowPos();

    DrawTitleBar(dl, o);
    DrawHeader(dl, o, profile);
    DrawKeyboard(dl, o, profile);
    DrawFooter(dl, o);
}

void MainWindow::DrawTitleBar(ImDrawList* dl, const ImVec2& o)
{
    AddAccentHairline(dl, o, 1280.f, 2.f);
    AddLogoMark(dl, ImVec2(o.x + 28.f, o.y + 20.f), 22.f);
    AddTrackedText(dl, UiFonts::Bold, 22.f, ImVec2(o.x + 60.f, o.y + 22.f), UiCol::Text, "SPAMMY", 4.f);
    dl->AddText(UiFonts::Mono, 12.f, ImVec2(o.x + 180.f, o.y + 29.f), UiCol::Mute, __DATE__);

    if (sUpdater.IsUpdateAvailable()) {
        char update[48];
        snprintf(update, sizeof(update), "NEW BUILD AVAIL %s", sUpdater.LatestDate());
        if (UiBadge("##update", ImVec2(o.x + 272.f, o.y + 19.f), update, UiCol::Ok)) sUpdater.OpenReleasePage();
        ImGui::Tip("Open latest release on GitHub");
    }

    if (UiGhostButton("##gear", ImVec2(o.x + 1152.f, o.y + 17.f), 30.f, UiGlyph_Gear)) ImGui::OpenPopup("##settings");
    if (UiGhostButton("##min", ImVec2(o.x + 1188.f, o.y + 17.f), 30.f, UiGlyph_Minimize))
        ShowWindow(Native(), SW_MINIMIZE);
    if (UiGhostButton("##close", ImVec2(o.x + 1224.f, o.y + 17.f), 30.f, UiGlyph_Close)) {
        if (sConfig.minimizeToTray)
            Hide();
        else
            Close();
    }

    DrawSettingsPopup(o);
}

void MainWindow::DrawHeader(ImDrawList* dl, const ImVec2& o, const std::shared_ptr<Profile>& profile)
{
    if (UiChipFrame("##profile", ImVec2(o.x + 28.f, o.y + 66.f), ImVec2(216.f, 46.f))) ImGui::OpenPopup("##profiles");
    UiChipLabel(ImVec2(o.x + 44.f, o.y + 73.f), "PROFILE");
    if (profile) {
        dl->AddText(UiFonts::Semi, 20.f, ImVec2(o.x + 44.f, o.y + 85.f), UiCol::Text, profile->name.c_str());
    } else {
        dl->AddText(UiFonts::Semi, 20.f, ImVec2(o.x + 44.f, o.y + 85.f), UiFlashDanger(), "NOT SET");
    }
    float chevT = UiAnim(ImGui::GetID("##profiles.chev"), ImGui::IsPopupOpen("##profiles") ? 1.f : 0.f, 16.f);
    AddChevronDown(dl, ImVec2(o.x + 222.f, o.y + 89.f), UiMixColor(UiCol::Sub, UiCol::Text, chevT), chevT);
    DrawProfilesPopup(o);

    if (profile) {
        if (UiChipFrame("##apps", ImVec2(o.x + 260.f, o.y + 66.f), ImVec2(216.f, 46.f))) ImGui::OpenPopup("##appsmenu");
        UiChipLabel(ImVec2(o.x + 276.f, o.y + 73.f), "ENABLE ONLY IN APPS");
        if (profile->apps.empty()) {
            dl->AddText(UiFonts::Semi, 18.f, ImVec2(o.x + 276.f, o.y + 86.f), UiFlashDanger(), "ALL APPS");
        } else {
            std::string apps;
            for (const std::string& app : profile->apps) {
                if (!apps.empty()) apps += ", ";
                apps += app;
            }
            const float maxW = 184.f;
            if (UiFonts::Semi->CalcTextSizeA(18.f, FLT_MAX, 0.f, apps.c_str()).x > maxW) {
                while (!apps.empty() &&
                       UiFonts::Semi->CalcTextSizeA(18.f, FLT_MAX, 0.f, (apps + "...").c_str()).x > maxW) {
                    apps.pop_back();
                    while (!apps.empty() && ((unsigned char)apps.back() & 0xC0) == 0x80)
                        apps.pop_back();
                }
                apps += "...";
            }
            dl->AddText(UiFonts::Semi, 18.f, ImVec2(o.x + 276.f, o.y + 86.f), UiCol::Text, apps.c_str());
        }
        DrawAppsPopup(o, profile);

        if (UiLockChip("##winkey", ImVec2(o.x + 700.f, o.y + 66.f), ImVec2(110.f, 46.f), "WIN KEY",
                       profile->disableWin)) {
            profile->disableWin = !profile->disableWin;
            sConfig.MarkDirty();
        }

        if (UiLockChip("##altf4", ImVec2(o.x + 824.f, o.y + 66.f), ImVec2(120.f, 46.f), "ALT + F4",
                       profile->disableAltF4)) {
            profile->disableAltF4 = !profile->disableAltF4;
            sConfig.MarkDirty();
        }

        if (UiChipFrame("##pause", ImVec2(o.x + 958.f, o.y + 66.f), ImVec2(150.f, 46.f))) {
            if (_editPause) {
                _editPause = false;
            } else {
                profile->vkPause = 0;
                sConfig.MarkDirty();
                sKeyboard.Attach();
                _editPause = true;
            }
        }
        UiChipLabel(ImVec2(o.x + 974.f, o.y + 73.f), "PAUSE KEY");
        char keycap[32];
        ImU32 keycapCol;
        if (_editPause) {
            strncpy(keycap, "PRESS KEY", sizeof(keycap));
            keycapCol = ImGui::GetColorU32(FlashColor(1.f, .55f, .28f, 1.5f, .4f, 1.f));
        } else if (profile->vkPause) {
            const char* keyName = sKeyboard.GetKeyName(GET_KEY_VKCODE(profile->vkPause));
            if (!keyName[0]) keyName = "UNKNOWN";
            size_t i = 0;
            for (; keyName[i] && i < sizeof(keycap) - 1; i++)
                keycap[i] = (keyName[i] >= 'a' && keyName[i] <= 'z') ? keyName[i] - 'a' + 'A' : keyName[i];
            keycap[i] = '\0';
            keycapCol = UiCol::Sub;
        } else {
            strncpy(keycap, "NOT SET", sizeof(keycap));
            keycapCol = UiFlashDanger();
        }
        AddKeycap(dl, ImVec2(o.x + 974.f, o.y + 85.f), ImVec2(o.x + 1052.f, o.y + 103.f), keycap, keycapCol);
        if (_editPause) {
            float p = 0.5f + 0.5f * sinf((float)ImGui::GetTime() * 4.f);
            AddGlow(dl, ImVec2(o.x + 958.f, o.y + 66.f), ImVec2(o.x + 1108.f, o.y + 112.f), UiCol::Spam, 10.f, 6,
                    0.08f + 0.14f * p);
        }
    }

    if (UiEnablePill("##enable", ImVec2(o.x + 1122.f, o.y + 66.f), ImVec2(132.f, 46.f), sApp.IsEnabled()))
        sApp.Enable(!sApp.IsEnabled());
}

void MainWindow::DrawKeyboard(ImDrawList* dl, const ImVec2& o, const std::shared_ptr<Profile>& profile)
{
    const ImVec2 panelMin = ImVec2(o.x + 20.f, o.y + 134.f);
    const ImVec2 panelMax = ImVec2(o.x + 1260.f, o.y + 660.f);
    AddPanel(dl, panelMin, panelMax, 16.f);

    const float gap = 7.f;
    const MouseForm mouseForm = sConfig.mouse;
    const float mouseW = 190.f;
    const float mouseLeft = panelMax.x - 16.f - mouseW;
    const ImVec2 start = ImVec2(panelMin.x + 16.f, panelMin.y + 16.f);
    const ImVec2 avail = ImVec2((mouseForm != MouseForm_Off ? mouseLeft - 24.f : panelMax.x - 16.f) - start.x,
                                panelMax.y - panelMin.y - 32.f);

    const std::vector<KeyboardKey>& layout = GetKeyboardLayout(sConfig.form, sConfig.variant);

    float gridW = 0.f;
    float gridH = 0.f;
    for (const KeyboardKey& item : layout) {
        gridW = ImMax(gridW, item.x + item.w);
        gridH = ImMax(gridH, item.y + item.h);
    }

    const float keySize = ImFloor(ImMin((avail.x + gap) / gridW, (avail.y + gap) / gridH));
    const ImVec2 origin = {start.x + (avail.x - (keySize * gridW - gap)) * .5f,
                           start.y + (avail.y - (keySize * gridH - gap)) * .5f};

    static unsigned s_popupMods = 0;
    static UINT s_pressVk = 0;
    static UINT s_rightVk = 0;
    static bool s_pressInPanel = false;
    static bool s_rightInPanel = false;
    static bool s_pressOnKey = false;
    static bool s_brushErase = false;
    static bool s_brushMoved = false;

    const bool anyPopup = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    const ImGuiWindow* hoveredWnd = GImGui->HoveredWindow;
    const bool overPopup = anyPopup && hoveredWnd && (hoveredWnd->Flags & ImGuiWindowFlags_Popup);
    static bool s_leftDismiss = false;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        s_leftDismiss = anyPopup;
        s_pressInPanel = !overPopup && ImGui::IsMouseHoveringRect(panelMin, panelMax);
        s_pressOnKey = false;
        s_pressVk = 0;
        s_brushErase = false;
        s_brushMoved = false;
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        s_rightInPanel = !overPopup && ImGui::IsMouseHoveringRect(panelMin, panelMax);
        s_rightVk = 0;
    }
    const bool brushing = s_pressInPanel && !s_leftDismiss && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.f);
    if (brushing) s_brushMoved = true;
    const bool releasedLeft = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    const bool releasedRight = ImGui::IsMouseReleased(ImGuiMouseButton_Right);

    struct PressBadge {
        ImVec2 pos;
        unsigned rate;
        DWORD age;
    };
    std::vector<PressBadge> badges;
    const DWORD nowTicks = GetTickCount();
    const bool wndFocused = GetForegroundWindow() == Native();

    unsigned mods = sKeyboard.TestModifiers();
    auto keyItem = [&](const char* id, const char* label, UINT vkCode, const ImVec2& pos, const ImVec2& size) {
        UiKeyDesc desc = {};
        desc.label = label;
        desc.locked = sKeyboard.IsModifier(vkCode) || vkCode == VK_LWIN || vkCode == VK_RWIN;
        desc.pressed = sKeyboard.IsPressed(vkCode) != 0;
        if (profile) {
            bool inherited = false;
            const KeyMode* mode = FindKeyMode(ResolveKeyAction(*profile, vkCode, mods, &inherited));
            desc.inherited = inherited;
            if (mode) desc.style = mode->keyStyle;

            if (vkCode < KEYBOARD_KEYS_COUNT) {
                if (wndFocused && desc.pressed && mode && mode->onTick) {
                    if (nowTicks - _spamTick[vkCode] > 1000) _spamTick[vkCode] = nowTicks - 1000;
                    while (nowTicks - _spamTick[vkCode] >= profile->speed) {
                        _spamTick[vkCode] += profile->speed;
                        LogKeyPress(vkCode, _spamTick[vkCode]);
                    }
                } else {
                    _spamTick[vkCode] = nowTicks;
                }
            }
        }

        UiKey(id, pos, size, desc);
        const ImVec2 keyMin = ImGui::GetItemRectMin();
        const ImVec2 keyMax = ImGui::GetItemRectMax();

        if (vkCode < KEYBOARD_KEYS_COUNT) {
            unsigned rate = 0;
            for (DWORD t : _pressLog[vkCode])
                if (t && nowTicks - t <= 1000) rate++;
            if (rate > 2)
                badges.push_back({ImVec2((keyMin.x + keyMax.x) * .5f, keyMin.y), rate, nowTicks - _pressTick[vkCode]});
        }

        const bool hoverKey = ImGui::IsMouseHoveringRect(keyMin, keyMax);
        const bool pressedHere = s_pressInPanel && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hoverKey;
        if (pressedHere) {
            s_pressOnKey = true;
            s_pressVk = vkCode;
        }

        if (profile && !desc.locked) {
            if (pressedHere && profile->keys[vkCode][mods].action == _brushAction) s_brushErase = true;

            if (brushing && hoverKey) {
                Action next = s_brushErase ? Action_None : _brushAction;
                if (profile->keys[vkCode][mods].action != next) {
                    profile->keys[vkCode][mods].action = next;
                    sConfig.MarkDirty();
                }
            }

            if (releasedLeft && !s_leftDismiss && !s_brushMoved && hoverKey && s_pressVk == vkCode) {
                KeyConfig& config = profile->keys[vkCode][mods];
                config.action = config.action == _brushAction ? Action_None : _brushAction;
                sConfig.MarkDirty();
            }

            if (s_rightInPanel && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && hoverKey) s_rightVk = vkCode;

            if (releasedRight && hoverKey && s_rightVk == vkCode) {
                if (!_selection.count(vkCode)) {
                    _selection.clear();
                    _selection.insert(vkCode);
                }
                s_popupMods = mods;
                ImGui::OpenPopup("##keymenu");
                s_rightVk = 0;
            }
        }
    };

    for (size_t i = 0; i < layout.size(); i++) {
        const KeyboardKey& key = layout[i];
        const ImVec2 pos = ImVec2(origin.x + key.x * keySize, origin.y + key.y * keySize);
        const ImVec2 size = ImVec2(key.w * keySize - gap, key.h * keySize - gap);
        char id[16];
        snprintf(id, sizeof(id), "##key%02x", (unsigned)i);
        keyItem(id, key.name, key.vkCode, pos, size);
    }

    if (mouseForm != MouseForm_Off) {
        dl->AddRectFilled(ImVec2(mouseLeft - 12.f, panelMin.y + 24.f), ImVec2(mouseLeft - 11.f, panelMax.y - 24.f),
                          UiCol::StrokeSoft);

        const ImVec2 bodySize = ImVec2(150.f, 270.f);
        const ImVec2 body = ImVec2(mouseLeft + (mouseW - bodySize.x) * .5f + 8.f,
                                   panelMin.y + (panelMax.y - panelMin.y - bodySize.y) * .5f);
        auto bp = [&](float x, float y) { return ImVec2(body.x + x, body.y + y); };
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

        const float now = (float)ImGui::GetTime();
        ImVec2 stripPrev;
        for (int i = 0; i <= 28; i++) {
            float a = IM_PI * (.15f + .7f * (float)i / 28.f);
            ImVec2 pt = bp(75.f + cosf(a) * 77.f, 200.f + sinf(a) * 62.f);
            if (i) {
                float hue = (float)i / 28.f * .6f - now * .1f;
                hue -= floorf(hue);
                float r, g, b;
                ImGui::ColorConvertHSVtoRGB(hue, .9f, 1.f, r, g, b);
                ImU32 led = IM_COL32((int)(r * 255.f), (int)(g * 255.f), (int)(b * 255.f), 255);
                dl->AddLine(stripPrev, pt, (led & ~IM_COL32_A_MASK) | IM_COL32(0, 0, 0, 70), 8.f);
                dl->AddLine(stripPrev, pt, (led & ~IM_COL32_A_MASK) | IM_COL32(0, 0, 0, 230), 3.f);
            }
            stripPrev = pt;
        }

        dl->AddBezierQuadratic(bp(10.f, 104.f), bp(75.f, 113.f), bp(140.f, 104.f), UiCol::StrokeSoft, 1.5f);

        keyItem("##mbL", "LMB", VK_LBUTTON, ImVec2(body.x + 8.f, body.y + 8.f), ImVec2(53.f, 92.f));
        keyItem("##mbM", "M3", VK_MBUTTON, ImVec2(body.x + 63.f, body.y + 22.f), ImVec2(24.f, 64.f));
        keyItem("##mbR", "RMB", VK_RBUTTON, ImVec2(body.x + 89.f, body.y + 8.f), ImVec2(53.f, 92.f));
        if (mouseForm == MouseForm_5) {
            keyItem("##mb5", "M5", VK_XBUTTON2, ImVec2(body.x + 6.f, body.y + 122.f), ImVec2(24.f, 38.f));
            keyItem("##mb4", "M4", VK_XBUTTON1, ImVec2(body.x + 6.f, body.y + 164.f), ImVec2(24.f, 38.f));
        }
    }

    for (const PressBadge& badge : badges) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u/s", badge.rate);
        float pop = badge.age < 120 ? 1.3f - .3f * (badge.age / 120.f) : 1.f;
        float alpha = badge.age > 700 ? 1.f - (badge.age - 700) / 300.f : 1.f;
        ImU32 a = (ImU32)(alpha * 255.f + .5f);
        auto fade = [a](ImU32 col) { return (col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT); };
        ImVec2 ts = UiFonts::Bold->CalcTextSizeA(24.f * pop, FLT_MAX, 0.f, buf);
        ImVec2 bmin = ImVec2(badge.pos.x - ts.x * .5f - 10.f, badge.pos.y - ts.y - 16.f);
        ImVec2 bmax = ImVec2(badge.pos.x + ts.x * .5f + 10.f, badge.pos.y - 6.f);
        dl->AddRectFilled(bmin, bmax, fade(UiCol::KeyCap), 8.f);
        dl->AddRect(bmin, bmax, fade(UiCol::Spam), 8.f, 0, 2.f);
        dl->AddText(UiFonts::Bold, 24.f * pop, ImVec2(badge.pos.x - ts.x * .5f, bmin.y + 5.f), fade(UiCol::SpamText),
                    buf);
    }

    if (releasedLeft && s_pressInPanel && !s_pressOnKey && !s_brushMoved) _selection.clear();
    if (releasedRight && s_rightInPanel && s_rightVk == 0 && !ImGui::IsPopupOpen("##keymenu")) _selection.clear();

    DrawKeyMenuPopup(profile, s_popupMods);
}

void MainWindow::DrawFooter(ImDrawList* dl, const ImVec2& o)
{
    dl->AddRectFilled(ImVec2(o.x + 20.f, o.y + 666.f), ImVec2(o.x + 1260.f, o.y + 667.f), UiCol::StrokeSoft);

    const char* status;
    ImU32 statusCol;
    bool glow = false;
    std::string detail;
    if (!sApp.IsEnabled()) {
        status = "PAUSED";
        statusCol = UiCol::Mute;
        detail = "hook disabled";
    } else if (auto active = sApp.ActiveProfile()) {
        status = "ACTIVE";
        statusCol = UiCol::Ok;
        glow = true;
        detail = "hooked -> " + sApp.ActiveAppName();
    } else {
        status = "READY";
        statusCol = UiCol::SpamText;
        detail = "waiting for bound app";
    }
    AddStatusDot(dl, ImVec2(o.x + 38.f, o.y + 689.f), 3.5f, statusCol, glow);
    AddTrackedText(dl, UiFonts::Bold, 15.f, ImVec2(o.x + 50.f, o.y + 682.f), statusCol, status, 1.5f);
    ImVec2 statusSize = CalcTrackedTextSize(UiFonts::Bold, 15.f, status, 1.5f);
    dl->AddText(UiFonts::Mono, 13.f, ImVec2(o.x + 50.f + statusSize.x + 16.f, o.y + 683.f), UiCol::Mute,
                detail.c_str());

    auto editing = sConfig.editingProfile;
    if (!editing) return;

    unsigned mins = (unsigned)(editing->activeMs / 60000);
    char value[32];
    if (mins >= 60)
        snprintf(value, sizeof(value), "%uh %02um", mins / 60, mins % 60);
    else
        snprintf(value, sizeof(value), "%um", mins);
    ImVec2 valueSize = UiFonts::Mono->CalcTextSizeA(14.f, FLT_MAX, 0.f, value);
    float x = o.x + 1252.f - valueSize.x;
    dl->AddText(UiFonts::Mono, 14.f, ImVec2(x, o.y + 681.f), UiCol::Sub, value);
    ImVec2 labelSize = CalcTrackedTextSize(UiFonts::Semi, 12.f, "ACTIVE TIME", 1.5f);
    AddTrackedText(dl, UiFonts::Semi, 12.f, ImVec2(x - 12.f - labelSize.x, o.y + 683.f), UiCol::Mute, "ACTIVE TIME",
                   1.5f);
}

void MainWindow::DrawProfilesPopup(const ImVec2& o)
{
    ImGui::SetNextWindowPos(ImVec2(o.x + 28.f, o.y + 116.f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(260.f, 0.f), ImVec2(260.f, 584.f));
    ImGui::SetNextWindowSize(ImVec2(260.f, 0.f));
    if (!ImGui::UiBeginPopup("##profiles")) return;

    static char s_newName[64] = {0};
    static bool s_creating = false;
    static bool s_focusName = false;
    static std::string s_armedDelete;
    if (ImGui::IsWindowAppearing()) {
        s_newName[0] = '\0';
        s_creating = false;
        s_armedDelete.clear();
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto editing = sConfig.editingProfile;
    const char* pendingDelete = NULL;
    int idx = 0;
    for (const std::shared_ptr<Profile>& item : sConfig.profiles) {
        ImGui::PushID(idx++);
        ImVec2 p = ImGui::GetCursorScreenPos();
        bool selected = item == editing;
        if (ImGui::Selectable("##prow", selected, 0, ImVec2(200.f, 24.f)))
            sConfig.SetEditingProfile(item->name.c_str());
        dl->AddText(UiFonts::Semi, 18.f, ImVec2(p.x + 8.f, p.y + 4.f), selected ? UiCol::SpamText : UiCol::Text,
                    item->name.c_str());
        char count[16];
        snprintf(count, sizeof(count), "%d", (int)item->apps.size());
        ImVec2 countSize = UiFonts::Mono->CalcTextSizeA(13.f, FLT_MAX, 0.f, count);
        dl->AddText(UiFonts::Mono, 13.f, ImVec2(p.x + 200.f - countSize.x - 8.f, p.y + 6.f), UiCol::Mute, count);

        bool armed = s_armedDelete == item->name;
        if (UiGhostButton("##del", ImVec2(p.x + 208.f, p.y), 24.f, UiGlyph_Close)) {
            if (armed)
                pendingDelete = item->name.c_str();
            else
                s_armedDelete = item->name;
        }
        if (armed)
            dl->AddRect(ImVec2(p.x + 208.f, p.y), ImVec2(p.x + 232.f, p.y + 24.f),
                        ImGui::GetColorU32(FlashColor(1.f, .3f, .37f, 1.5f, .5f, 1.f)), 6.f);
        ImGui::PopID();
    }
    if (idx) ImGui::Separator();

    if (!s_creating) {
        if (UiMenuRow("+  NEW PROFILE", 0, false, true)) {
            s_creating = true;
            s_focusName = true;
        }
    } else {
        ImGui::SetNextItemWidth(-52.f);
        if (s_focusName) {
            ImGui::SetKeyboardFocusHere();
            s_focusName = false;
        }
        bool enter = ImGui::InputTextWithHint("##newname", "profile name", s_newName, sizeof(s_newName),
                                              ImGuiInputTextFlags_EnterReturnsTrue);
        bool valid = strlen(s_newName) >= 3 && !sConfig.IsProfileExists(s_newName);
        ImGui::SameLine(0.f, 6.f);
        ImGui::BeginDisabled(!valid);
        bool ok = ImGui::Button("OK", ImVec2(-FLT_MIN, 0.f));
        ImGui::EndDisabled();
        if ((enter || ok) && valid) {
            sConfig.CreateProfile(s_newName);
            ImGui::CloseCurrentPopup();
        }
    }

    if (pendingDelete) sApp.DeleteProfile(pendingDelete);
    ImGui::UiEndPopup();
}

static bool MatchesFilter(const std::string& name, const char* filter)
{
    if (!filter[0]) return true;
    auto it = std::search(name.begin(), name.end(), filter, filter + strlen(filter),
                          [](char a, char b) { return tolower((unsigned char)a) == tolower((unsigned char)b); });
    return it != name.end();
}

void MainWindow::DrawAppsPopup(const ImVec2& o, const std::shared_ptr<Profile>& profile)
{
    ImGui::SetNextWindowPos(ImVec2(o.x + 260.f, o.y + 116.f));
    ImGui::SetNextWindowSize(ImVec2(280.f, 0.f));
    if (!ImGui::UiBeginPopup("##appsmenu")) return;

    static char s_search[64] = {0};
    static std::vector<std::string> s_appList;
    if (ImGui::IsWindowAppearing()) {
        s_search[0] = '\0';
        s_appList.clear();
        EnumWindows([this](HWND hwnd) -> BOOL {
            std::filesystem::path path = GetProcessPath(hwnd);
            if (!path.has_filename() || path == _appFilePath) return TRUE;
            std::string fileName = (const char*)path.filename().u8string().c_str();
            if (std::find(s_appList.begin(), s_appList.end(), fileName) != s_appList.end()) return TRUE;
            s_appList.emplace_back(std::move(fileName));
            return TRUE;
        });
        LexicographicalSort(s_appList);
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##appsearch", "search", s_search, sizeof(s_search));

    ImGui::SetNextWindowSizeConstraints(ImVec2(0.f, 0.f), ImVec2(FLT_MAX, 520.f));
    ImGui::BeginChild("##applist", ImVec2(0.f, 0.f), ImGuiChildFlags_AutoResizeY);

    ImGui::PushFont(UiFonts::Semi, 14.f);
    if (!profile->apps.empty()) {
        ImGui::TextDisabled("BOUND");
        const char* unbindApp = NULL;
        int idx = 0;
        for (const std::string& app : profile->apps) {
            if (!MatchesFilter(app, s_search)) continue;
            ImGui::PushID(idx++);
            ImVec2 p = ImGui::GetCursorScreenPos();
            float w = ImGui::GetContentRegionAvail().x;
            UiMenuRow(app.c_str(), UiCol::Spam, false, true, true);
            if (UiGhostButton("##unbind", ImVec2(p.x + w - 24.f, p.y), 24.f, UiGlyph_Close)) unbindApp = app.c_str();
            ImGui::PopID();
        }
        if (unbindApp) sConfig.UnbindProfile(profile->name.c_str(), unbindApp);
        ImGui::Separator();
    }

    ImGui::TextDisabled("RUNNING");
    int shown = 0;
    int idx = 1000;
    for (const std::string& app : s_appList) {
        if (!MatchesFilter(app, s_search)) continue;
        if (std::find(profile->apps.begin(), profile->apps.end(), app) != profile->apps.end()) continue;
        auto appProfile = sConfig.FindProfileByApp(app.c_str());
        bool boundElsewhere = appProfile && appProfile != profile;
        ImGui::PushID(idx++);
        if (UiMenuRow(app.c_str(), 0, boundElsewhere, true)) sConfig.BindProfile(profile->name.c_str(), app.c_str());
        ImGui::PopID();
        shown++;
    }
    if (!shown) ImGui::TextDisabled(s_search[0] ? "no matches" : "nothing running");
    ImGui::PopFont();
    ImGui::EndChild();

    ImGui::UiEndPopup();
}

void MainWindow::DrawSettingsPopup(const ImVec2& o)
{
    ImGui::SetNextWindowPos(ImVec2(o.x + 1252.f, o.y + 52.f), ImGuiCond_Always, ImVec2(1.f, 0.f));
    ImGui::SetNextWindowSize(ImVec2(230.f, 0.f));
    if (!ImGui::UiBeginPopup("##settings")) return;

    static bool s_autoStart = false;
    if (ImGui::IsWindowAppearing()) s_autoStart = sApp.IsAutoStartEnabled();

    if (UiToggleRow("##autostart", "AUTO START", s_autoStart)) {
        if (sApp.EnableAutoStart(!s_autoStart)) s_autoStart = !s_autoStart;
    }
    if (UiToggleRow("##tray", "HIDE ON CLOSE", sConfig.minimizeToTray)) {
        sConfig.minimizeToTray = !sConfig.minimizeToTray;
        sConfig.MarkDirty();
    }
    if (UiToggleRow("##trayicon", "SHOW TRAY ICON", sConfig.showTrayIcon)) {
        sConfig.showTrayIcon = !sConfig.showTrayIcon;
        ShowTrayIcon(sConfig.showTrayIcon);
        sConfig.MarkDirty();
    }
    if (UiToggleRow("##sounds", "ENABLE SOUNDS", sConfig.soundsEnabled)) {
        sConfig.soundsEnabled = !sConfig.soundsEnabled;
        sConfig.MarkDirty();
    }

    ImGui::Separator();

    int form = (int)sConfig.form;
    if (UiStepperRow("##form", "LAYOUT", KeyboardFormName(sConfig.form), KeyboardForm_Count, form)) {
        sConfig.form = (KeyboardForm)form;
        sConfig.MarkDirty();
    }
    int variant = (int)sConfig.variant;
    if (UiStepperRow("##variant", "VARIANT", KeyboardVariantName(sConfig.variant), KeyboardVariant_Count, variant)) {
        sConfig.variant = (KeyboardVariant)variant;
        sConfig.MarkDirty();
    }
    int mouse = (int)sConfig.mouse;
    if (UiStepperRow("##mouse", "MOUSE", MouseFormName(sConfig.mouse), MouseForm_Count, mouse)) {
        sConfig.mouse = (MouseForm)mouse;
        sConfig.MarkDirty();
    }

    ImGui::Separator();
    if (UiMenuRow("GITHUB")) LaunchUrl(L"https://github.com/FrostAtom/spammy");

    ImGui::UiEndPopup();
}

void MainWindow::DrawKeyMenuPopup(const std::shared_ptr<Profile>& profile, unsigned popupMods)
{
    if (!profile) return;
    ImGui::SetNextWindowSize(ImVec2(210.f, 0.f));
    if (!ImGui::UiBeginPopup("##keymenu")) return;

    ImGui::PushFont(UiFonts::Semi, 14.f);
    ImGui::TextDisabled("%d SELECTED", (int)_selection.size());
    ImGui::PopFont();
    ImGui::Separator();

    Action action = Action_None;
    bool apply = false;
    for (const KeyMode& mode : KeyModes()) {
        if (UiMenuRow(mode.name, mode.menuColor)) {
            action = mode.action;
            apply = true;
        }
    }
    if (apply) _brushAction = action;
    if (UiMenuRow("CLEAR", UiCol::Mute)) action = Action_None, apply = true;
    if (apply) {
        for (UINT vk : _selection)
            profile->keys[vk][popupMods].action = action;
        sConfig.MarkDirty();
    }

    ImGui::Separator();
    ImGui::PushFont(UiFonts::Semi, 14.f);
    ImGui::TextDisabled("RATE");
    ImGui::PopFont();
    int speed = (int)profile->speed;
    bool changed = false;
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float pad = (ImGui::GetFrameHeight() - 24.f) * .5f;
    if (UiGhostButton("##ratedec", ImVec2(p.x, p.y + pad), 24.f, UiGlyph_Minus)) speed--, changed = true;
    ImGui::SetCursorScreenPos(ImVec2(p.x + 28.f, p.y));
    ImGui::SetNextItemWidth(w - 56.f);
    if (ImGui::SliderInt("##rate", &speed, PROFILE_SPEED_MIN, PROFILE_SPEED_MAX, "%d ms")) changed = true;
    if (UiGhostButton("##rateinc", ImVec2(p.x + w - 24.f, p.y + pad), 24.f, UiGlyph_Plus)) speed++, changed = true;
    if (changed) {
        profile->speed = (unsigned)ImClamp(speed, PROFILE_SPEED_MIN, PROFILE_SPEED_MAX);
        sConfig.MarkDirty();
    }

    ImGui::UiEndPopup();
}

bool MainWindow::HandleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
    if (Window::HandleWndProc(msg, wParam, lParam, result)) return true;
    switch (msg) {
    case WM_USER_FOCUS: {
        if (!IsShown()) Show();
        Focus();
        return true;
    }
    }
    return false;
}
