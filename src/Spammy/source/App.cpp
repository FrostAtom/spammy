#include "App.h"
#include "Modes.h"
#include "Updater.h"

App& App::Instance()
{
    static App s_app;
    return s_app;
}

bool App::Init(int argc, char** argv)
{
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(NULL, buf, std::size(buf));
    SetCurrentDirectoryW(std::filesystem::path(buf).parent_path().wstring().c_str());

    bool firstRun = !std::filesystem::is_regular_file(CONFIG_FILE);
    if (!sConfig.Load()) {
        int action =
            MessageBoxW(NULL, L"Can't load config, reset to defaults?", L"" APP_NAME, MB_ICONQUESTION | MB_OKCANCEL);
        if (action != IDOK) return false;
    }
    if (firstRun) EnableAutoStart(true);
    _autoStartEnabled = IsAutoStartEnabled();

    if (sConfig.profiles.empty()) {
        sConfig.CreateProfile("UNNAMED");
        for (unsigned short vk = '1'; vk <= '5'; vk++)
            sConfig.editingProfile->keys[vk][KeyMod_None].action = Action_Spammy;
    }

    _mainWindow = new MainWindow(L"" APP_NAME);
    if (!_mainWindow->Initialize()) return false;

    if (!(argc > 1 && strcmp(argv[1], "autolaunch") == 0)) {
        _mainWindow->Update();
        _mainWindow->Show();
    }

    sKeyboard.OnPress([this](UINT vkCode, bool repeat) { return OnKeyEvent(true, vkCode, repeat); });
    sKeyboard.OnRelease([this](UINT vkCode, bool repeat) { return OnKeyEvent(false, vkCode, repeat); });

    sUpdater.CheckAsync();

    _isRunning = true;
    return true;
}

void App::Uninit()
{
    sKeyboard.Detach();
    sConfig.Save();
    if (_mainWindow) {
        delete _mainWindow;
        _mainWindow = NULL;
    }
}

bool App::Run()
{
    if (!_isRunning) return true;

    while (!_mainWindow->MustQuit()) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (_mainWindow->WantQuit()) {
            if (sConfig.minimizeToTray)
                _mainWindow->Hide();
            else
                _mainWindow->Close();
        }
        if (_mainWindow->IsWndNormalized()) _mainWindow->Update();

        CheckIsFocusChanged();

        std::shared_ptr<Profile> profile;
        HWND activeHwnd;
        {
            std::lock_guard lock(_callbackMutex);
            profile = _activeProfile;
            activeHwnd = _activeHwnd;
        }
        DWORD ticks = GetTickCount();
        sConfig.SaveIfDirty(ticks);

        if (sConfig.enabled && profile) {
            if (ticks - _lastUpdate >= profile->speed) {
                unsigned mods = sKeyboard.TestModifiers();
                for (unsigned short i = 0; i < sKeyboard.Count(); i++) {
                    if (!sKeyboard.IsPressed(i)) continue;
                    const KeyMode* mode = FindKeyMode(ResolveKeyAction(*profile, i, mods));
                    if (mode && mode->onTick) mode->onTick({activeHwnd, i, false, *profile});
                }
                _lastUpdate = ticks;
            }
        }

        Sleep(10); // don't abuse cpu X_x
    }
    _mainWindow->Cleanup();

    return true;
}

void App::Enable(bool state)
{
    sConfig.enabled = state;
    sConfig.MarkDirty();
}

bool App::IsEnabled()
{
    return sConfig.enabled;
}

static const wchar_t* getAutoStartCommand()
{
    static wchar_t command[512] = {0};
    if (!command[0]) {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(NULL, buf, std::size(buf));
        _snwprintf_s(command, std::size(command), L"\"%s\" autolaunch", buf);
    }
    return command;
}

bool App::IsAutoStartEnabled()
{
    HKEY hKey = NULL;
    LSTATUS status =
        RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey);
    if (status != ERROR_SUCCESS) return false;
    const wchar_t* command = getAutoStartCommand();
    bool result = false;

    DWORD regType = REG_SZ;
    wchar_t regPath[MAX_PATH] = {0};
    DWORD regReaded = sizeof(regPath);
    status = RegGetValueW(hKey, NULL, L"Spammy", RRF_RT_REG_SZ, &regType, (PVOID)regPath, &regReaded);
    if (status == ERROR_SUCCESS && wcscmp(regPath, command) == 0) result = true;
    RegCloseKey(hKey);
    return result;
}

bool App::EnableAutoStart(bool state)
{
    HKEY hKey = NULL;
    LSTATUS status =
        RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey);
    if (status != ERROR_SUCCESS) return false;

    if (state) {
        const wchar_t* command = getAutoStartCommand();
        status = RegSetValueExW(hKey, L"Spammy", NULL, REG_SZ, (const BYTE*)command, (lstrlenW(command) + 1) * 2);
    } else {
        status = RegDeleteValueW(hKey, L"Spammy");
    }
    return status == ERROR_SUCCESS;
}

static void PlayEnabledSound(bool enabled)
{
    wchar_t path[MAX_PATH];
    if (!GetWindowsDirectoryW(path, std::size(path))) return;
    wcscat_s(path, enabled ? L"\\Media\\Windows Hardware Insert.wav" : L"\\Media\\Windows Hardware Remove.wav");
    PlaySoundW(path, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

bool App::OnKeyEvent(bool down, UINT vkCode, bool repeat)
{
    if (_mainWindow &&
        (down ? _mainWindow->HandleKeyPress(vkCode, repeat) : _mainWindow->HandleKeyRelease(vkCode, repeat)))
        return true;

    std::shared_ptr<Profile> profile;
    HWND activeHwnd;
    {
        std::lock_guard lock(_callbackMutex);
        profile = _activeProfile;
        activeHwnd = _activeHwnd;
    }
    if (!profile) return false;

    unsigned mods = sKeyboard.TestModifiers();
    if (profile->vkPause == MAKE_KEY_BUNDLE(vkCode, mods)) {
        if (!down) {
            sConfig.enabled = !sConfig.enabled;
            sConfig.MarkDirty();
            if (sConfig.soundsEnabled) PlayEnabledSound(sConfig.enabled);
        }
        return true;
    }
    if (profile->disableWin && (vkCode == VK_RWIN || vkCode == VK_LWIN)) return true;
    if (profile->disableAltF4 && (vkCode == VK_F4 && sKeyboard.TestModifiers(KeyMod_Alt))) return true;
    if (!sConfig.enabled) return false;

    const KeyMode* mode = FindKeyMode(ResolveKeyAction(*profile, vkCode, mods));
    if (!mode) return false;
    KeyModeContext ctx = {activeHwnd, (unsigned short)vkCode, repeat, *profile};
    bool (*handler)(const KeyModeContext&) = down ? mode->onPress : mode->onRelease;
    return handler && handler(ctx);
}

std::shared_ptr<Profile> App::ActiveProfile()
{
    std::lock_guard lock(_callbackMutex);
    return _activeProfile;
}

std::string App::ActiveAppName()
{
    std::lock_guard lock(_callbackMutex);
    return _activeApp;
}

void App::DeleteProfile(const char* name)
{
    std::shared_ptr<Profile> profile = sConfig.FindProfile(name);
    if (!profile) return;
    if (profile == _activeProfile) {
        std::lock_guard lock(_callbackMutex);
        _activeProfile = NULL;
    }
    sConfig.DeleteProfile(name);
}

void App::CheckIsFocusChanged()
{
    static HWND s_prevFocus = NULL;
    HWND focus = GetForegroundWindow();
    if (s_prevFocus != focus) {
        OnFocusChanged();
        s_prevFocus = focus;
    }
}

void App::OnFocusChanged()
{
    HWND newHwnd = GetForegroundWindow();
    if (newHwnd == _activeHwnd) return;

    std::shared_ptr<Profile> newProfile;
    std::string newApp;
    if (std::filesystem::path path; newHwnd && (path = GetProcessPath(newHwnd)).has_filename()) {
        newApp = (const char*)path.filename().u8string().c_str();
        newProfile = sConfig.FindProfileByApp(newApp.c_str());
        if (!newProfile) {
            wchar_t selfPath[MAX_PATH] = {0};
            GetModuleFileNameW(NULL, selfPath, std::size(selfPath));
            if (path != selfPath) {
                auto it = std::find_if(sConfig.profiles.begin(), sConfig.profiles.end(),
                                       [](const std::shared_ptr<Profile>& item) { return item->apps.empty(); });
                if (it != sConfig.profiles.end()) newProfile = *it;
            }
        }
    }

    {
        std::lock_guard lock(_callbackMutex);
        _activeProfile = newProfile;
        _activeHwnd = newHwnd;
        _activeApp = newProfile ? newApp : std::string();
    }

    if (newProfile || (_mainWindow && newHwnd == _mainWindow->Native()))
        sKeyboard.Attach();
    else
        sKeyboard.Detach();
}

int main(int argc, char** argv)
{
    bool ret = sApp.Init(argc, argv) && sApp.Run();
    sApp.Uninit();
    return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
