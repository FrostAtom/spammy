#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <nlohmann/json.hpp>
#include "resources/resources.h"
#include "Window/Window.h"
#include "ImGui.h"
#include "Win32/Keyboard.h"
#include "Win32/Mouse.h"
#include "Win32/WinEvent.h"
#include "KeyboardLayout.h"
#include "Profile.h"
#include "Utils.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <cmath>
#define CONFIG_FILE "Spammy.json"
#define sApp App::instance()

class App {
    static const char* _buildTime;
    static App* _self;
    std::filesystem::path _appFilePath;
    bool _isRunning;
    Window* _window;
    bool _enabled;
    Action _selectedAction;
    bool _autoStartEnabled;
    bool _editPause;
    DWORD _lastUpdate;
    HWND _activeHwnd;

    std::list<Profile> _profiles;
    Profile* _activeProfile, *_editingProfile;

public:
    App(int argc, wchar_t* argv[]);
    ~App();

    static App& instance();
    int run();

private:
    void initUIStyle();

    bool loadConfig();
    void saveConfig();

    bool isAutoStartEnabled();
    bool enableAutoStart(bool state);

    void onFocusChanged();
    void onTrayClick();
    void onTrayMenu(TrayIconMenu& menu);
    void onWindowUpdate();
    bool onKeyPress(UINT vkCode, bool repeat);
    bool onKeyRelease(UINT vkCode, bool repeat);

    Profile* findProfile(const char* name);
    bool isProfileExists(const char* name);
    void createProfile(const char* name);
    void editProfile(const char* name);
    void deleteProfile(const char* profile);
    Profile* findProfileByApp(const char* app);
    bool isProfileBinded(const char* name, const char* app);
    void bindProfile(const char* name, const char* app);
    void unbindProfile(const char* name, const char* app);
};
