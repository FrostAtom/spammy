#pragma once
#include "Config.h"
#include "Headers.h"
#include "MainWindow.h"
#include "Profile.h"
#include "Utils.h"
#include "Win32/Keyboard.h"
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
    void DeleteProfile(const char* name);

private:
    void CheckIsFocusChanged();
    void OnFocusChanged();
    bool OnKeyEvent(bool down, UINT vkCode, bool repeat);

private:
    MainWindow* _mainWindow = NULL;
    bool _isRunning = false;
    bool _autoStartEnabled = false;
    DWORD _lastUpdate = 0;
    DWORD _statTicks = 0;
    HWND _activeHwnd = NULL;

    std::shared_ptr<Profile> _activeProfile;
    std::string _activeApp;
    // guards _activeProfile/_activeHwnd between the keyboard-hook thread (callbacks) and the main thread (OnFocusChanged)
    std::mutex _callbackMutex;
};
