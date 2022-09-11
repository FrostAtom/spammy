#include <nlohmann/json.hpp>
#include "MainWindow.h"
#include "Win32/Keyboard.h"
#include "Win32/Mouse.h"
#include "Win32/WinEvent.h"
#include "Profile.h"
#include "Utils.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <cmath>
#define CONFIG_FILE "Spammy.json"
#define sApp App::instance()

class App {
    static App* _self;
    MainWindow* _mainWindow;
    bool _isRunning;
    bool _autoStartEnabled;
    bool _enabled;
    DWORD _lastUpdate;
    HWND _activeHwnd;

    std::list<Profile> _profiles;
    Profile* _activeProfile, *_editingProfile;

public:
    App(int argc, char** argv);
    ~App();

    static App& instance();
    int run();

    void enable(bool state = true);
    bool isEnabled();

    bool isAutoStartEnabled();
    bool enableAutoStart(bool state);

    const std::list<Profile>& getProfiles();
    Profile* findProfile(const char* name);
    bool isProfileExists(const char* name);
    void createProfile(const char* name);
    void editingProfile(const char* name);
    Profile* editingProfile();
    void deleteProfile(const char* profile);
    Profile* findProfileByApp(const char* app);
    bool isProfileBinded(const char* name, const char* app);
    void bindProfile(const char* name, const char* app);
    void unbindProfile(const char* name, const char* app);

private:
    bool loadConfig();
    void saveConfig();

    void onFocusChanged();
    bool onKeyPress(UINT vkCode, bool repeat);
    bool onKeyRelease(UINT vkCode, bool repeat);
};
