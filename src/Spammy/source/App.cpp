#include "App.h"

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

    if (!LoadConfig()) {
        int action =
            MessageBoxW(NULL, L"Can't load config, reset to defaults?", L"" APP_NAME, MB_ICONQUESTION | MB_OKCANCEL);
        if (action != IDOK) return false;
    }
    _autoStartEnabled = IsAutoStartEnabled();

    _mainWindow = new MainWindow(L"" APP_NAME);
    if (!_mainWindow->Initialize()) return false;

    if (argc > 1 && strcmp(argv[1], "autolaunch") == 0) _mainWindow->Hide();

    sKeyboard.OnPress(std::bind_front(&App::OnKeyPress, this));
    sKeyboard.OnRelease(std::bind_front(&App::OnKeyRelease, this));

    _isRunning = true;
    return true;
}

void App::Uninit()
{
    sKeyboard.Detach();
    SaveConfig();
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
        if (_mainWindow->WantQuit()) _mainWindow->Hide();
        if (_mainWindow->IsWndNormalized()) _mainWindow->Update();

        CheckIsFocusChanged();

        std::shared_ptr<Profile> profile;
        HWND activeHwnd;
        {
            std::lock_guard lock(_callbackMutex);
            profile = _activeProfile;
            activeHwnd = _activeHwnd;
        }
        if (_enabled && profile) {
            DWORD ticks = GetTickCount();
            if (ticks - _lastUpdate >= profile->speed) {
                DWORD mods = sKeyboard.TestModifiers();
                for (size_t i = 0; i < sKeyboard.Count(); i++) {
                    if (sKeyboard.IsPressed(i)) {
                        bool spammy = false;
                        if (mods != KeyMod_None) {
                            if (profile->keys[i][mods].action == Action_None)
                                if (profile->keys[i][KeyMod_None].action == Action_Spammy) spammy = true;
                        } else {
                            if (profile->keys[i][KeyMod_None].action == Action_Spammy) spammy = true;
                        }
                        if (spammy) sKeyboard.Press(activeHwnd, i);
                    }
                }
                _lastUpdate = ticks;
            }
        }

        Sleep(10); // don't abuse cpu X_x
    }

    return true;
}

void App::Enable(bool state)
{
    _enabled = state;
}

bool App::IsEnabled()
{
    return _enabled;
}

bool App::LoadConfig()
{
    if (!std::filesystem::is_regular_file(CONFIG_FILE)) return true;
    std::ifstream file(CONFIG_FILE);
    if (!file) return false;
    try {
        nlohmann::json json = nlohmann::json::parse(file);
        if (auto enabled = json["enabled"]; enabled.is_boolean()) _enabled = enabled.get<bool>();
        if (auto profiles = json["profiles"]; profiles.is_array())
            _profiles = json["profiles"].get<std::list<std::shared_ptr<Profile>>>();
        if (auto editingProfile = json["editingProfile"]; editingProfile.is_string())
            _editingProfile = FindProfile(editingProfile.get_ref<const std::string&>().c_str());
    } catch (nlohmann::json::exception&) {
        return false;
    }
    return true;
}

void App::SaveConfig()
{
    std::ofstream file(CONFIG_FILE);
    if (!file) return;

    nlohmann::json json = nlohmann::json::object();
    if (_enabled) json["enabled"] = true;
    if (_editingProfile) json["editingProfile"] = _editingProfile->name;
    if (!_profiles.empty()) json["profiles"] = _profiles;

    file << json.dump(2, ' ');
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

bool App::OnKeyPress(UINT vkCode, bool repeat)
{
    if (_mainWindow && _mainWindow->HandleKeyPress(vkCode, repeat)) return true;

    std::lock_guard lock(_callbackMutex);
    unsigned mods = sKeyboard.TestModifiers();
    if (_activeProfile) {
        if (_activeProfile->vkPause == MAKE_KEY_BUNDLE(vkCode, mods)) return true;
        if (_activeProfile->disableWin && (vkCode == VK_RWIN || vkCode == VK_LWIN)) return true;
        if (_activeProfile->disableAltF4 && (vkCode == VK_F4 && sKeyboard.TestModifiers(KeyMod_Alt))) return true;
        if (_enabled) {
            KeyConfig& cfg = _activeProfile->keys[vkCode][mods];
            switch (cfg.action) {
            case Action_Spammy:
            case Action_Speedy:
                if (!repeat) sKeyboard.Press(_activeHwnd, vkCode);
                return true;
            }
        }
    }
    return false;
}

bool App::OnKeyRelease(UINT vkCode, bool repeat)
{
    if (_mainWindow && _mainWindow->HandleKeyRelease(vkCode, repeat)) return true;

    std::lock_guard lock(_callbackMutex);
    unsigned mods = sKeyboard.TestModifiers();
    if (_activeProfile && _activeProfile->vkPause == MAKE_KEY_BUNDLE(vkCode, mods)) {
        _enabled = !_enabled;
        return true;
    }

    if (_activeProfile) {
        if (_activeProfile->disableWin && (vkCode == VK_RWIN || vkCode == VK_LWIN)) return true;
        if (_activeProfile->disableAltF4 && (vkCode == VK_F4 && sKeyboard.TestModifiers(KeyMod_Alt))) return true;
        if (_enabled) {
            KeyConfig& cfg = _activeProfile->keys[vkCode][mods];
            switch (cfg.action) {
            case Action_Spammy:
            case Action_Speedy: return true;
            }
        }
    }
    return false;
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

const std::list<std::shared_ptr<Profile>>& App::GetProfiles()
{
    return _profiles;
}

std::shared_ptr<Profile> App::FindProfile(const char* name)
{
    auto it = std::find_if(_profiles.begin(), _profiles.end(),
                           [=](const std::shared_ptr<Profile>& item) { return item->name == name; });
    return it != _profiles.end() ? *it : NULL;
}

bool App::IsProfileExists(const char* name)
{
    return (bool)FindProfile(name);
}

void App::CreateProfile(const char* name)
{
    std::shared_ptr<Profile> profile = std::make_shared<Profile>();
    profile->name = name;

    _profiles.emplace_back(profile);
    _editingProfile = profile;
}

void App::EditingProfile(const char* name)
{
    _editingProfile = FindProfile(name);
}

std::shared_ptr<Profile> App::EditingProfile()
{
    return _editingProfile;
}

void App::DeleteProfile(const char* name)
{
    auto it = std::find_if(_profiles.begin(), _profiles.end(),
                           [name](const std::shared_ptr<Profile>& item) { return item->name == name; });
    if (it == _profiles.end()) return;
    if (*it == _activeProfile) {
        std::lock_guard lock(_callbackMutex);
        _activeProfile = NULL;
    }
    if (*it == _editingProfile) _editingProfile = NULL;
    _profiles.erase(it);
}

std::shared_ptr<Profile> App::FindProfileByApp(const char* app)
{
    auto it = std::find_if(_profiles.begin(), _profiles.end(), [app](const std::shared_ptr<Profile>& item) {
        return std::find(item->apps.begin(), item->apps.end(), app) != item->apps.end();
    });
    return it != _profiles.end() ? *it : NULL;
}

bool App::IsProfileBinded(const char* name, const char* app)
{
    if (std::shared_ptr<Profile> profile = FindProfile(name))
        return std::find(profile->apps.begin(), profile->apps.end(), app) != profile->apps.end();
    return false;
}

void App::BindProfile(const char* name, const char* app)
{
    std::shared_ptr<Profile> profile = FindProfile(name);
    if (!profile || std::find(profile->apps.begin(), profile->apps.end(), app) != profile->apps.end()) return;
    profile->apps.emplace_back(app);
    LexicographicalSort(profile->apps);
}

void App::UnbindProfile(const char* name, const char* app)
{
    std::shared_ptr<Profile> profile = FindProfile(name);
    if (!profile) return;
    profile->apps.erase(std::remove(profile->apps.begin(), profile->apps.end(), app), profile->apps.end());
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
        newProfile = FindProfileByApp(newApp.c_str());
    }

    const bool changed = (newProfile != _activeProfile);
    {
        std::lock_guard lock(_callbackMutex);
        _activeProfile = newProfile;
        _activeHwnd = newHwnd;
        _activeApp = newProfile ? newApp : std::string();
    }

    // attach/detach outside the lock: Detach() joins the hook thread, which may be
    // blocked on _callbackMutex inside a running callback -> deadlock if held here
    if (changed) {
        if (newProfile)
            sKeyboard.Attach();
        else
            sKeyboard.Detach();
    }
}

int main(int argc, char** argv)
{
    bool ret = sApp.Init(argc, argv) && sApp.Run();
    sApp.Uninit();
    return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
