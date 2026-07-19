#include "MainWindow.h"
#include "App.h"
#include "Resources.h"

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
    if (!sApp.IsShowTrayIcon()) ShowTrayIcon(false);

    LoadStyle();

    return true;
}

bool MainWindow::HandleKeyPress(unsigned short vkCode, bool)
{
    if (Keyboard::IsMouseButton(vkCode)) return false;
    if (_editPause) return true;
    return false;
}

bool MainWindow::HandleKeyRelease(unsigned short vkCode, bool)
{
    if (Keyboard::IsMouseButton(vkCode)) return false;
    if (_editPause) {
        if (auto profile = sApp.EditingProfile()) profile->vkPause = MAKE_KEY_BUNDLE(vkCode, sKeyboard.TestModifiers());
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
    auto profile = sApp.EditingProfile();
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

    if (UiGhostButton("##gear", ImVec2(o.x + 1152.f, o.y + 17.f), 30.f, UiGlyph_Gear)) ImGui::OpenPopup("##settings");
    if (UiGhostButton("##min", ImVec2(o.x + 1188.f, o.y + 17.f), 30.f, UiGlyph_Minimize))
        ShowWindow(Native(), SW_MINIMIZE);
    if (UiGhostButton("##close", ImVec2(o.x + 1224.f, o.y + 17.f), 30.f, UiGlyph_Close)) {
        if (sApp.IsMinimizeToTray())
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
        ImU32 flash = ImGui::GetColorU32(FlashColor(1.f, .3f, .37f, 2.f, .4f, 1.f));
        dl->AddText(UiFonts::Semi, 20.f, ImVec2(o.x + 44.f, o.y + 85.f), flash, "NOT SET");
    }
    float chevT = UiAnim(ImGui::GetID("##profiles.chev"), ImGui::IsPopupOpen("##profiles") ? 1.f : 0.f, 16.f);
    AddChevronDown(dl, ImVec2(o.x + 222.f, o.y + 89.f), UiMixColor(UiCol::Sub, UiCol::Text, chevT), chevT);
    DrawProfilesPopup(o);

    if (profile) {
        if (UiChipFrame("##apps", ImVec2(o.x + 260.f, o.y + 66.f), ImVec2(110.f, 46.f))) ImGui::OpenPopup("##appsmenu");
        UiChipLabel(ImVec2(o.x + 276.f, o.y + 73.f), "APPS");
        if (profile->apps.empty()) {
            ImU32 flash = ImGui::GetColorU32(FlashColor(1.f, .3f, .37f, 2.f, .4f, 1.f));
            dl->AddText(UiFonts::Semi, 18.f, ImVec2(o.x + 276.f, o.y + 86.f), flash, "NONE");
        } else {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d bound", (int)profile->apps.size());
            dl->AddText(UiFonts::Semi, 18.f, ImVec2(o.x + 276.f, o.y + 86.f), UiCol::Text, buf);
        }
        DrawAppsPopup(o, profile);

        if (UiChipFrame("##winkey", ImVec2(o.x + 700.f, o.y + 66.f), ImVec2(110.f, 46.f)))
            profile->disableWin = !profile->disableWin;
        UiChipLabel(ImVec2(o.x + 716.f, o.y + 73.f), "WIN KEY");
        float winT = UiAnim(ImGui::GetID("##winkey.t"), profile->disableWin ? 1.f : 0.f, 14.f);
        AddStatusDot(dl, ImVec2(o.x + 722.f, o.y + 96.f), 3.5f, UiMixColor(UiCol::Mute, UiCol::Spam, winT),
                     profile->disableWin);
        dl->AddText(UiFonts::Semi, 18.f, ImVec2(o.x + 732.f, o.y + 86.f), UiMixColor(UiCol::Sub, UiCol::Text, winT),
                    profile->disableWin ? "LOCKED" : "FREE");

        if (UiChipFrame("##altf4", ImVec2(o.x + 824.f, o.y + 66.f), ImVec2(120.f, 46.f)))
            profile->disableAltF4 = !profile->disableAltF4;
        UiChipLabel(ImVec2(o.x + 840.f, o.y + 73.f), "ALT + F4");
        float altT = UiAnim(ImGui::GetID("##altf4.t"), profile->disableAltF4 ? 1.f : 0.f, 14.f);
        AddStatusDot(dl, ImVec2(o.x + 846.f, o.y + 96.f), 3.5f, UiMixColor(UiCol::Mute, UiCol::Spam, altT),
                     profile->disableAltF4);
        dl->AddText(UiFonts::Semi, 18.f, ImVec2(o.x + 856.f, o.y + 86.f), UiMixColor(UiCol::Sub, UiCol::Text, altT),
                    profile->disableAltF4 ? "LOCKED" : "FREE");

        if (UiChipFrame("##pause", ImVec2(o.x + 958.f, o.y + 66.f), ImVec2(150.f, 46.f))) {
            if (_editPause) {
                _editPause = false;
            } else {
                profile->vkPause = 0;
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
            keycapCol = UiCol::Mute;
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
    const MouseForm mouseForm = sApp.Mouse();
    const float mouseW = 190.f;
    const float mouseLeft = panelMax.x - 16.f - mouseW;
    const ImVec2 start = ImVec2(panelMin.x + 16.f, panelMin.y + 16.f);
    const ImVec2 avail = ImVec2((mouseForm != MouseForm_Off ? mouseLeft - 24.f : panelMax.x - 16.f) - start.x,
                                panelMax.y - panelMin.y - 32.f);

    const std::vector<KeyboardKey>& layout = GetKeyboardLayout(sApp.Form(), sApp.Variant());

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

    unsigned mods = sKeyboard.TestModifiers();
    auto keyItem = [&](const char* id, const char* label, UINT vkCode, const ImVec2& pos, const ImVec2& size) {
        KeyConfig* cfg = profile ? &profile->keys[vkCode][mods] : NULL;

        UiKeyDesc desc = {};
        desc.label = label;
        desc.locked = sKeyboard.IsModifier(vkCode) || vkCode == VK_LWIN || vkCode == VK_RWIN;
        desc.pressed = sKeyboard.IsPressed(vkCode) != 0;
        desc.selected = _selection.count(vkCode) != 0;
        if (cfg) {
            Action action = cfg->action;
            if (action == Action_None && mods != KeyMod_None) {
                Action base = profile->keys[vkCode][KeyMod_None].action;
                if (base != Action_None) {
                    action = base;
                    desc.inherited = true;
                }
            }
            switch (action) {
            case Action_Spammy: desc.style = UiKeyStyle_Spam; break;
            case Action_Speedy: desc.style = UiKeyStyle_Speedy; break;
            case Action_Disabled: desc.style = UiKeyStyle_Blocked; break;
            default: break;
            }
        }

        UiKey(id, pos, size, desc);
        const ImVec2 keyMin = ImGui::GetItemRectMin();
        const ImVec2 keyMax = ImGui::GetItemRectMax();

        const bool hoverKey = ImGui::IsMouseHoveringRect(keyMin, keyMax);
        const bool pressedHere = s_pressInPanel && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hoverKey;
        if (pressedHere) {
            s_pressOnKey = true;
            s_pressVk = vkCode;
        }

        if (cfg && !desc.locked) {
            if (pressedHere && _selection.count(vkCode)) s_brushErase = true;

            if (brushing && hoverKey) {
                if (s_brushErase)
                    _selection.erase(vkCode);
                else
                    _selection.insert(vkCode);
            }

            if (releasedLeft && !s_leftDismiss && !s_brushMoved && hoverKey && s_pressVk == vkCode) {
                if (_selection.count(vkCode))
                    _selection.erase(vkCode);
                else
                    _selection.insert(vkCode);
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
        detail = "hooked ?? " + sApp.ActiveAppName();
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

    static DWORD s_started = GetTickCount();
    unsigned mins = (GetTickCount() - s_started) / 60000;
    char value[32];
    if (mins >= 60)
        snprintf(value, sizeof(value), "%uh %02um", mins / 60, mins % 60);
    else
        snprintf(value, sizeof(value), "%um", mins);
    ImVec2 valueSize = UiFonts::Mono->CalcTextSizeA(14.f, FLT_MAX, 0.f, value);
    float x = o.x + 1252.f - valueSize.x;
    dl->AddText(UiFonts::Mono, 14.f, ImVec2(x, o.y + 681.f), UiCol::Sub, value);
    ImVec2 labelSize = CalcTrackedTextSize(UiFonts::Semi, 12.f, "SESSION", 1.5f);
    AddTrackedText(dl, UiFonts::Semi, 12.f, ImVec2(x - 12.f - labelSize.x, o.y + 683.f), UiCol::Mute, "SESSION", 1.5f);
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
    auto editing = sApp.EditingProfile();
    const char* pendingDelete = NULL;
    int idx = 0;
    for (const std::shared_ptr<Profile>& item : sApp.GetProfiles()) {
        ImGui::PushID(idx++);
        ImVec2 p = ImGui::GetCursorScreenPos();
        bool selected = item == editing;
        if (ImGui::Selectable("##prow", selected, 0, ImVec2(200.f, 24.f))) sApp.EditingProfile(item->name.c_str());
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
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (s_focusName) {
            ImGui::SetKeyboardFocusHere();
            s_focusName = false;
        }
        bool enter = ImGui::InputTextWithHint("##newname", "profile name", s_newName, sizeof(s_newName),
                                              ImGuiInputTextFlags_EnterReturnsTrue);
        bool valid = strlen(s_newName) >= 3 && !sApp.IsProfileExists(s_newName);
        if (enter && valid) {
            sApp.CreateProfile(s_newName);
            ImGui::CloseCurrentPopup();
        }
    }

    if (pendingDelete) sApp.DeleteProfile(pendingDelete);
    ImGui::UiEndPopup();
}

void MainWindow::DrawAppsPopup(const ImVec2& o, const std::shared_ptr<Profile>& profile)
{
    ImGui::SetNextWindowPos(ImVec2(o.x + 260.f, o.y + 116.f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(280.f, 0.f), ImVec2(280.f, 584.f));
    ImGui::SetNextWindowSize(ImVec2(280.f, 0.f));
    if (!ImGui::UiBeginPopup("##appsmenu")) return;

    static std::vector<std::string> s_appList;
    if (ImGui::IsWindowAppearing()) {
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
    }

    ImGui::PushFont(UiFonts::Semi, 14.f);
    if (!profile->apps.empty()) {
        ImGui::TextDisabled("BOUND");
        const char* unbindApp = NULL;
        int idx = 0;
        for (const std::string& app : profile->apps) {
            ImGui::PushID(idx++);
            if (UiMenuRow(app.c_str(), UiCol::Spam, false, true)) unbindApp = app.c_str();
            ImGui::PopID();
        }
        if (unbindApp) sApp.UnbindProfile(profile->name.c_str(), unbindApp);
        ImGui::Separator();
    }

    ImGui::TextDisabled("RUNNING");
    int shown = 0;
    int idx = 1000;
    for (const std::string& app : s_appList) {
        if (std::find(profile->apps.begin(), profile->apps.end(), app) != profile->apps.end()) continue;
        auto appProfile = sApp.FindProfileByApp(app.c_str());
        bool boundElsewhere = appProfile && appProfile != profile;
        ImGui::PushID(idx++);
        if (UiMenuRow(app.c_str(), 0, boundElsewhere, true)) sApp.BindProfile(profile->name.c_str(), app.c_str());
        ImGui::PopID();
        shown++;
    }
    if (!shown) ImGui::TextDisabled("nothing running");
    ImGui::PopFont();

    ImGui::UiEndPopup();
}

void MainWindow::DrawSettingsPopup(const ImVec2& o)
{
    ImGui::SetNextWindowPos(ImVec2(o.x + 1252.f, o.y + 52.f), ImGuiCond_Always, ImVec2(1.f, 0.f));
    ImGui::SetNextWindowSize(ImVec2(230.f, 0.f));
    if (!ImGui::UiBeginPopup("##settings")) return;

    static bool s_autoStart = false;
    if (ImGui::IsWindowAppearing()) s_autoStart = sApp.IsAutoStartEnabled();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(210.f, 28.f));
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(p.x + 8.f, p.y + 6.f), UiCol::Text, "AUTO START");
    if (UiToggle("##autostart", ImVec2(p.x + 164.f, p.y + 4.f), s_autoStart)) {
        if (sApp.EnableAutoStart(!s_autoStart)) s_autoStart = !s_autoStart;
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 34.f));

    ImVec2 t = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(210.f, 28.f));
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(t.x + 8.f, t.y + 6.f), UiCol::Text, "HIDE ON CLOSE");
    bool tray = sApp.IsMinimizeToTray();
    if (UiToggle("##tray", ImVec2(t.x + 164.f, t.y + 4.f), tray)) sApp.SetMinimizeToTray(!tray);
    ImGui::SetCursorScreenPos(ImVec2(t.x, t.y + 34.f));

    ImVec2 ti = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(210.f, 28.f));
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(ti.x + 8.f, ti.y + 6.f), UiCol::Text, "SHOW TRAY ICON");
    bool trayIcon = sApp.IsShowTrayIcon();
    if (UiToggle("##trayicon", ImVec2(ti.x + 164.f, ti.y + 4.f), trayIcon)) sApp.SetShowTrayIcon(!trayIcon);
    ImGui::SetCursorScreenPos(ImVec2(ti.x, ti.y + 34.f));

    ImVec2 sn = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(210.f, 28.f));
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(sn.x + 8.f, sn.y + 6.f), UiCol::Text, "ENABLE SOUNDS");
    bool sounds = sApp.IsSoundsEnabled();
    if (UiToggle("##sounds", ImVec2(sn.x + 164.f, sn.y + 4.f), sounds)) sApp.SetSoundsEnabled(!sounds);
    ImGui::SetCursorScreenPos(ImVec2(sn.x, sn.y + 34.f));

    ImGui::Separator();

    ImVec2 f = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(210.f, 24.f));
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(f.x + 8.f, f.y + 3.f), UiCol::Text, "LAYOUT");
    const char* formName = KeyboardFormName(sApp.Form());
    ImVec2 fSize = UiFonts::Semi->CalcTextSizeA(18.f, FLT_MAX, 0.f, formName);
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(f.x + 202.f - fSize.x, f.y + 3.f), UiCol::SpamText, formName);
    int form = (int)sApp.Form();
    if (UiStepper("##form", ImVec2(f.x + 8.f, f.y + 26.f), 194.f, KeyboardForm_Count, form))
        sApp.SetForm((KeyboardForm)form);
    ImGui::SetCursorScreenPos(ImVec2(f.x, f.y + 54.f));

    ImVec2 v = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(210.f, 24.f));
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(v.x + 8.f, v.y + 3.f), UiCol::Text, "VARIANT");
    const char* variantName = KeyboardVariantName(sApp.Variant());
    ImVec2 vSize = UiFonts::Semi->CalcTextSizeA(18.f, FLT_MAX, 0.f, variantName);
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(v.x + 202.f - vSize.x, v.y + 3.f), UiCol::SpamText, variantName);
    int variant = (int)sApp.Variant();
    if (UiStepper("##variant", ImVec2(v.x + 8.f, v.y + 26.f), 194.f, KeyboardVariant_Count, variant))
        sApp.SetVariant((KeyboardVariant)variant);
    ImGui::SetCursorScreenPos(ImVec2(v.x, v.y + 54.f));

    ImVec2 mo = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(210.f, 24.f));
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(mo.x + 8.f, mo.y + 3.f), UiCol::Text, "MOUSE");
    const char* mouseName = MouseFormName(sApp.Mouse());
    ImVec2 moSize = UiFonts::Semi->CalcTextSizeA(18.f, FLT_MAX, 0.f, mouseName);
    dl->AddText(UiFonts::Semi, 18.f, ImVec2(mo.x + 202.f - moSize.x, mo.y + 3.f), UiCol::SpamText, mouseName);
    int mouse = (int)sApp.Mouse();
    if (UiStepper("##mouse", ImVec2(mo.x + 8.f, mo.y + 26.f), 194.f, MouseForm_Count, mouse))
        sApp.SetMouse((MouseForm)mouse);
    ImGui::SetCursorScreenPos(ImVec2(mo.x, mo.y + 54.f));

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
    if (UiMenuRow("SPAMMY", UiCol::Spam)) action = Action_Spammy, apply = true;
    if (UiMenuRow("SPEEDY", UiCol::Speedy)) action = Action_Speedy, apply = true;
    if (UiMenuRow("DISABLED", UiCol::Danger)) action = Action_Disabled, apply = true;
    if (UiMenuRow("CLEAR", UiCol::Mute)) action = Action_None, apply = true;
    if (apply)
        for (UINT vk : _selection)
            profile->keys[vk][popupMods].action = action;

    ImGui::Separator();
    ImGui::PushFont(UiFonts::Semi, 14.f);
    ImGui::TextDisabled("RATE");
    ImGui::PopFont();
    ImGui::SetNextItemWidth(-FLT_MIN);
    int speed = (int)profile->speed;
    if (ImGui::SliderInt("##rate", &speed, 10, 1000, "%d ms")) profile->speed = (unsigned)speed;

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
