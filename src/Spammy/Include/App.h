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

    void DeleteProfile(const std::shared_ptr<Profile>& profile);
    std::shared_ptr<Profile> LastActiveProfile();

private:
    struct InputEvent {
        KeyHandler_t handler;
        unsigned short vkCode;
        bool survivesFocusSwitch = false;

        static InputEvent PauseToggle() { return {.handler = nullptr, .vkCode = 0, .survivesFocusSwitch = true}; }
        bool IsPauseToggle() const { return !handler; }
    };

    void UpdateActiveTarget();
    std::pair<std::shared_ptr<Profile>, HWND> ActiveTarget();

    // hook thread: decide and enqueue only
    bool OnKeyEvent(bool down, UINT vkCode, bool repeat);
    void PostInput(InputEvent ev);

    // input worker thread
    void StartInputWorker();
    void StopInputWorker();
    void InputWorkerProc(std::stop_token stop);
    void HandleInput(const InputEvent& ev);
    void TickAutofire(const Profile& profile);

private:
    std::unique_ptr<MainWindow> _mainWindow;
    std::weak_ptr<Profile> _lastActiveProfile; // main thread only

    // guards the two below; only the main thread writes them, so it may read them unlocked
    std::mutex _targetMutex;
    std::shared_ptr<Profile> _activeProfile;
    HWND _activeHwnd = nullptr;

    // hook thread only
    unsigned short _heldPauseVk = 0;
    bool _injectedF4Held = false;

    std::jthread _inputThread;
    HANDLE _inputWake = nullptr;
    std::mutex _inputMutex;
    std::vector<InputEvent> _inputQueue;
};
