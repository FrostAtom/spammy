#pragma once
#include "Headers.h"
#include "MainWindow.h"
#include "Profile.h"
#include "Utils.h"
#include "Win32/Keyboard.h"
#define CONFIG_FILE "Spammy.json"
#define sApp App::Instance()

class App {
public:
    ~App() = default;
    bool Init(int argc, char** argv);
    void Uninit();

    static App& Instance();
    bool Run();

    void Enable(bool state = true);
    bool IsEnabled();

    bool IsAutoStartEnabled();
    bool EnableAutoStart(bool state);

    std::shared_ptr<Profile> ActiveProfile();
    std::string ActiveAppName();

    const std::list<std::shared_ptr<Profile>>& GetProfiles();
    std::shared_ptr<Profile> FindProfile(const char* name);
    bool IsProfileExists(const char* name);
    void CreateProfile(const char* name);
    void EditingProfile(const char* name);
    std::shared_ptr<Profile> EditingProfile();
    void DeleteProfile(const char* profile);
    std::shared_ptr<Profile> FindProfileByApp(const char* app);
    bool IsProfileBinded(const char* name, const char* app);
    void BindProfile(const char* name, const char* app);
    void UnbindProfile(const char* name, const char* app);

private:
    bool LoadConfig();
    void SaveConfig();

    void CheckIsFocusChanged();
    void OnFocusChanged();
    bool OnKeyPress(UINT vkCode, bool repeat);
    bool OnKeyRelease(UINT vkCode, bool repeat);

private:
    MainWindow* _mainWindow = NULL;
    bool _isRunning = false;
    bool _autoStartEnabled = false;
    std::atomic<bool> _enabled = false;
    DWORD _lastUpdate = 0;
    HWND _activeHwnd = NULL;

    std::list<std::shared_ptr<Profile>> _profiles;
    std::shared_ptr<Profile> _activeProfile, _editingProfile;
    std::string _activeApp;
    // guards _activeProfile/_activeHwnd between the keyboard-hook thread (callbacks) and the main thread (OnFocusChanged)
    std::mutex _callbackMutex;
};
