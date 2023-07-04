#pragma once
#include "Headers.h"
#include "MainWindow.h"
#include "Win32/Keyboard.h"
#include "Win32/Mouse.h"
#include "Profile.h"
#include "Utils.h"
#define CONFIG_FILE "Spammy.json"
#define sApp App::instance()

class App {
public:
    ~App() = default;
    bool init(int argc, char** argv);
    void uninit();

    static App& instance();
    bool run();

    void enable(bool state = true);
    bool isEnabled();

    bool isAutoStartEnabled();
    bool enableAutoStart(bool state);

    const std::list<std::shared_ptr<Profile>>& getProfiles();
    std::shared_ptr<Profile> findProfile(const char* name);
    bool isProfileExists(const char* name);
    void createProfile(const char* name);
    void editingProfile(const char* name);
    std::shared_ptr<Profile> editingProfile();
    void deleteProfile(const char* profile);
    std::shared_ptr<Profile> findProfileByApp(const char* app);
    bool isProfileBinded(const char* name, const char* app);
    void bindProfile(const char* name, const char* app);
    void unbindProfile(const char* name, const char* app);

private:
    bool loadConfig();
    void saveConfig();

    void checkIsFocusChanged();
    void onFocusChanged();
    bool onKeyPress(UINT vkCode, bool repeat);
    bool onKeyRelease(UINT vkCode, bool repeat);

private:
    MainWindow* _mainWindow = NULL;
    bool _isRunning = false;
    bool _autoStartEnabled = false;
    bool _enabled = false;
    DWORD _lastUpdate = 0;
    HWND _activeHwnd = NULL;

    std::list<std::shared_ptr<Profile>> _profiles;
    std::shared_ptr<Profile> _activeProfile, _editingProfile;
};
