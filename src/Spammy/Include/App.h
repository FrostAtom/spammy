#pragma once
#include "Headers.h"
#include "MainWindow.h"
#include "Modes.h"
#include "Profile.h"
#define sApp App::Instance()

class App {
    App() = default;

public:
    static App& Instance();
    bool Init(int argc, char** argv);
    void Run();
    void Uninit();

    void Enable(bool state);
    bool IsEnabled() const;

    bool IsAutoStartEnabled() const;
    bool EnableAutoStart(bool state);

    void DeleteProfile(const char* name);

private:
    // a physical key event the hook decided to swallow, handed off to the input worker; the handler is resolved at
    // hook time, so the worker runs exactly what the event was swallowed for even if the profile changed since
    struct InputEvent {
        KeyHandler_t handler; // NULL: the pause key was hit
        unsigned short vkCode;
        bool keep = false; // survives a foreground switch: dropping it would leave a key stuck down

        bool IsPauseToggle() const { return !handler; }
    };

    void UpdateActiveTarget();
    std::shared_ptr<Profile> ActiveProfile();
    std::pair<std::shared_ptr<Profile>, HWND> ActiveTarget();

    // hook thread: decide + enqueue only
    bool OnKeyEvent(bool down, UINT vkCode, bool repeat);
    void PostInput(InputEvent ev);

    // input worker thread: injection + autofire ticking, decoupled from rendering
    void StartInputWorker();
    void StopInputWorker();
    void InputWorkerProc(std::stop_token stop);
    void HandleInput(const InputEvent& ev);
    void TickAutofire(const Profile& profile);

private:
    std::unique_ptr<MainWindow> _mainWindow;
    HWND _activeHwnd = nullptr;

    std::shared_ptr<Profile> _activeProfile;
    // guards _activeProfile/_activeHwnd across the hook thread, the input worker and the main thread
    std::mutex _targetMutex;
    // pause key whose down toggled and whose up/repeats are still to be swallowed; hook thread only
    unsigned short _heldPauseVk = 0;
    // an Alt+F4 down went in as a plain F4 and its up must follow the same way; hook thread only
    bool _altF4Held = false;

    std::jthread _inputThread;
    HANDLE _inputWake = nullptr;
    std::mutex _inputMutex;
    std::vector<InputEvent> _inputQueue;
};
