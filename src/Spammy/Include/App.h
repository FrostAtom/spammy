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
    void Run();

    void Enable(bool state = true);
    bool IsEnabled();

    bool IsAutoStartEnabled();
    bool EnableAutoStart(bool state);

    std::shared_ptr<Profile> ActiveProfile();
    std::string ActiveAppName();
    void DeleteProfile(const char* name);

private:
    // a physical key event the hook decided to swallow, handed off to the input worker
    struct InputEvent {
        enum Kind : unsigned char {
            Kind_Press,
            Kind_Release,
            Kind_TogglePause
        };
        Kind kind;
        bool repeat;
        unsigned short vkCode;
        unsigned mods;
        std::shared_ptr<Profile> profile;
    };

    void UpdateActiveTarget();
    std::pair<std::shared_ptr<Profile>, HWND> ActiveTarget();

    // hook thread: decide + enqueue only
    bool OnKeyEvent(bool down, UINT vkCode, bool repeat);
    void PostInput(InputEvent&& ev);

    // input worker thread: injection + autofire ticking, decoupled from rendering
    void StartInputWorker();
    void StopInputWorker();
    void InputWorkerProc(std::stop_token stop);
    void HandleInput(const InputEvent& ev);
    void TickAutofire(const Profile& profile);

private:
    std::unique_ptr<MainWindow> _mainWindow;
    HWND _activeHwnd = NULL;

    std::shared_ptr<Profile> _activeProfile;
    std::string _activeApp;
    // guards _activeProfile/_activeHwnd between the hook thread, the input worker and the main thread (UpdateActiveTarget)
    std::mutex _callbackMutex;

    std::jthread _inputThread;
    HANDLE _inputWake = NULL;
    std::mutex _inputMutex;
    std::vector<InputEvent> _inputQueue;
};
