#include "MainWindow.h"
#include "App.h"
#include "Resources.h"

using namespace ImGui;

MainWindow* MainWindow::_self = NULL;
MainWindow& MainWindow::Instance()
{
    return *_self;
}

MainWindow::MainWindow(const wchar_t* className, const wchar_t* wndName)
    : Window(className, wndName), _editPause(false), _editPauseHooked(false)
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

    LoadStyle();

    return true;
}

bool MainWindow::HandleKeyPress(unsigned short vkCode, bool)
{
    if (_editPause) return true;
    return false;
}

bool MainWindow::HandleKeyRelease(unsigned short vkCode, bool)
{
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
    if (_editPauseHooked && !_editPause) {
        if (!sApp.ActiveProfile()) sKeyboard.Detach();
        _editPauseHooked = false;
    }

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
    if (UiGhostButton("##close", ImVec2(o.x + 1224.f, o.y + 17.f), 30.f, UiGlyph_Close)) Hide();

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
    AddChevronDown(dl, ImVec2(o.x + 222.f, o.y + 89.f), UiCol::Sub);
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
        AddStatusDot(dl, ImVec2(o.x + 722.f, o.y + 96.f), 3.5f, profile->disableWin ? UiCol::Spam : UiCol::Mute,
                     profile->disableWin);
        dl->AddText(UiFonts::Semi, 18.f, ImVec2(o.x + 732.f, o.y + 86.f),
                    profile->disableWin ? UiCol::Text : UiCol::Sub, profile->disableWin ? "LOCKED" : "FREE");

        if (UiChipFrame("##altf4", ImVec2(o.x + 824.f, o.y + 66.f), ImVec2(120.f, 46.f)))
            profile->disableAltF4 = !profile->disableAltF4;
        UiChipLabel(ImVec2(o.x + 840.f, o.y + 73.f), "ALT + F4");
        AddStatusDot(dl, ImVec2(o.x + 846.f, o.y + 96.f), 3.5f, profile->disableAltF4 ? UiCol::Spam : UiCol::Mute,
                     profile->disableAltF4);
        dl->AddText(UiFonts::Semi, 18.f, ImVec2(o.x + 856.f, o.y + 86.f),
                    profile->disableAltF4 ? UiCol::Text : UiCol::Sub, profile->disableAltF4 ? "LOCKED" : "FREE");

        if (UiChipFrame("##pause", ImVec2(o.x + 958.f, o.y + 66.f), ImVec2(150.f, 46.f))) {
            if (_editPause) {
                _editPause = false;
            } else {
                profile->vkPause = 0;
                _editPauseHooked = sKeyboard.Attach();
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
    const ImVec2 avail = ImVec2(panelMax.x - panelMin.x - 32.f, panelMax.y - panelMin.y - 32.f);
    const ImVec2 start = ImVec2(panelMin.x + 16.f, panelMin.y + 16.f);

    float gridW = 0.f;
    float gridH = 0.f;
    for (const KeyboardKey& item : KeyboardLayout) {
        gridW = ImMax(gridW, item.x + item.w);
        gridH = ImMax(gridH, item.y + item.h);
    }

    const float keySize = ImFloor(ImMin((avail.x + gap) / gridW, (avail.y + gap) / gridH));
    const ImVec2 origin = {start.x + (avail.x - (keySize * gridW - gap)) * .5f,
                           start.y + (avail.y - (keySize * gridH - gap)) * .5f};

    static unsigned s_popupMods = 0;
    static int s_brushButton = -1;
    static UINT s_pressVk = 0;
    static bool s_pressInPanel = false;
    static bool s_pressOnKey = false;
    static bool s_brushErase = false;
    static bool s_brushMoved = false;

    const bool anyPopup = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    const ImGuiWindow* hoveredWnd = GImGui->HoveredWindow;
    const bool overPopup = anyPopup && hoveredWnd && (hoveredWnd->Flags & ImGuiWindowFlags_Popup);
    for (int btn = ImGuiMouseButton_Left; btn <= ImGuiMouseButton_Right; btn++) {
        if (!ImGui::IsMouseClicked((ImGuiMouseButton)btn)) continue;
        if (overPopup) continue;
        if (btn == ImGuiMouseButton_Left && anyPopup) continue;
        s_brushButton = btn;
        s_pressInPanel = ImGui::IsMouseHoveringRect(panelMin, panelMax);
        s_pressOnKey = false;
        s_pressVk = 0;
        s_brushErase = false;
        s_brushMoved = false;
    }
    const bool brushing =
        s_brushButton >= 0 && s_pressInPanel && ImGui::IsMouseDragging((ImGuiMouseButton)s_brushButton, 4.f);
    if (brushing) s_brushMoved = true;
    const bool released = s_brushButton >= 0 && ImGui::IsMouseReleased((ImGuiMouseButton)s_brushButton);

    unsigned mods = sKeyboard.TestModifiers();
    for (size_t i = 0; i < std::size(KeyboardLayout); i++) {
        const KeyboardKey& key = KeyboardLayout[i];
        const ImVec2 pos = ImVec2(origin.x + key.x * keySize, origin.y + key.y * keySize);
        const ImVec2 size = ImVec2(key.w * keySize - gap, key.h * keySize - gap);

        KeyConfig* cfg = profile ? &profile->keys[key.vkCode][mods] : NULL;

        UiKeyDesc desc = {};
        desc.label = key.name;
        desc.locked = sKeyboard.IsModifier(key.vkCode) || key.vkCode == VK_LWIN || key.vkCode == VK_RWIN;
        desc.pressed = sKeyboard.IsPressed(key.vkCode) != 0;
        desc.selected = _selection.count(key.vkCode) != 0;
        if (cfg) {
            Action action = cfg->action;
            if (action == Action_None && mods != KeyMod_None) {
                Action base = profile->keys[key.vkCode][KeyMod_None].action;
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

        char id[16];
        snprintf(id, sizeof(id), "##key%02x", (unsigned)i);
        UiKey(id, pos, size, desc);
        const ImVec2 keyMin = ImGui::GetItemRectMin();
        const ImVec2 keyMax = ImGui::GetItemRectMax();

        const bool hoverKey = ImGui::IsMouseHoveringRect(keyMin, keyMax);
        const bool pressedHere =
            s_brushButton >= 0 && s_pressInPanel && ImGui::IsMouseClicked((ImGuiMouseButton)s_brushButton) && hoverKey;
        if (pressedHere) {
            s_pressOnKey = true;
            s_pressVk = key.vkCode;
        }

        if (cfg && !desc.locked) {
            if (pressedHere && _selection.count(key.vkCode)) s_brushErase = true;

            if (brushing && hoverKey) {
                if (s_brushErase)
                    _selection.erase(key.vkCode);
                else
                    _selection.insert(key.vkCode);
            }

            if (released && !s_brushMoved && hoverKey && s_pressVk == key.vkCode) {
                if (_selection.count(key.vkCode))
                    _selection.erase(key.vkCode);
                else
                    _selection.insert(key.vkCode);
            }
        }
    }

    if (released) {
        if (s_pressInPanel && !s_pressOnKey && !s_brushMoved) _selection.clear();
        if (s_brushButton == ImGuiMouseButton_Right && s_pressInPanel && !_selection.empty()) {
            s_popupMods = mods;
            ImGui::OpenPopup("##keymenu");
        }
        s_brushButton = -1;
    }

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
    ImGui::SetNextWindowSize(ImVec2(260.f, 0.f));
    if (!ImGui::BeginPopup("##profiles")) return;

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
        if (armed) dl->AddRect(ImVec2(p.x + 208.f, p.y), ImVec2(p.x + 232.f, p.y + 24.f), UiCol::Danger, 6.f);
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
    ImGui::EndPopup();
}

void MainWindow::DrawAppsPopup(const ImVec2& o, const std::shared_ptr<Profile>& profile)
{
    ImGui::SetNextWindowPos(ImVec2(o.x + 260.f, o.y + 116.f));
    ImGui::SetNextWindowSize(ImVec2(280.f, 0.f));
    if (!ImGui::BeginPopup("##appsmenu")) return;

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

    ImGui::EndPopup();
}

void MainWindow::DrawSettingsPopup(const ImVec2& o)
{
    ImGui::SetNextWindowPos(ImVec2(o.x + 1252.f, o.y + 52.f), ImGuiCond_Always, ImVec2(1.f, 0.f));
    ImGui::SetNextWindowSize(ImVec2(230.f, 0.f));
    if (!ImGui::BeginPopup("##settings")) return;

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

    ImGui::Separator();
    if (UiMenuRow("GITHUB")) LaunchUrl(L"https://github.com/FrostAtom/spammy");
    if (UiMenuRow("DISCORD")) LaunchUrl(L"https://discord.gg/NNnBTK5c8e");
    if (UiMenuRow("AUTHOR")) LaunchUrl(L"https://t.me/boredatom");

    ImGui::EndPopup();
}

void MainWindow::DrawKeyMenuPopup(const std::shared_ptr<Profile>& profile, unsigned popupMods)
{
    if (!profile) return;
    ImGui::SetNextWindowSize(ImVec2(210.f, 0.f));
    if (!ImGui::BeginPopup("##keymenu")) return;

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

    ImGui::EndPopup();
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
