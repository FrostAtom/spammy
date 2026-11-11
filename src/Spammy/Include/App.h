#pragma once
#include "Headers.h"
#include "MainWindow.h"
#include "Modes.h"
#include "Profile.h"
#include <mutex>
#define sApp App::Instance()

class App {
    App() = default;

public:
    static App& Instance();
    bool Init(int argc, char** argv);
    void Run();
    void Uninit();

    void Enable(bool state);
    [[nodiscard]] bool IsEnabled() const;

    [[nodiscard]] bool IsAutoStartEnabled() const;
    bool EnableAutoStart(bool state);

    void DeleteProfile(const std::shared_ptr<Profile>& profile);
    std::shared_ptr<Profile> LastActiveProfile();

private:
    struct InputEvent {
        KeyHandler_t handler = nullptr;
        unsigned short vkCode = 0;
        bool survivesFocusSwitch = false;
    };

    void OnForegroundChange(HWND shownOrActivated);
    void OnDesktopSwitch();
    void SetActiveTarget(HWND hwnd);
    std::pair<std::shared_ptr<Profile>, HWND> ActiveTarget();

    // hook thread: decide and enqueue only
    bool OnKeyEvent(unsigned short vkCode, bool down, bool repeat);
    void OnKeyPressOrRelease();
    void PostInput(InputEvent ev);
    void ForgetHeldKeys();

    // input worker thread
    void StartInputWorker();
    void StopInputWorker();
    void InputWorkerProc(const std::stop_token& stop);

    std::unique_ptr<MainWindow> _mainWindow;
    std::weak_ptr<Profile> _lastActiveProfile; // main thread only
    HWND _targetLeftAtDesktopSwitch = nullptr; // main thread only
    std::array<HWINEVENTHOOK, 3> _winEventHooks = {};
    // the detached updater thread may signal it after App is gone
    std::shared_ptr<void> _uiStale{CreateEventW(nullptr, FALSE, FALSE, nullptr), &CloseHandle};

    // guards the two below; only the main thread writes them, so it may read them unlocked
    std::mutex _targetMutex;
    std::shared_ptr<Profile> _activeProfile;
    HWND _activeHwnd = nullptr;

    // hook thread only; ForgetHeldKeys resets them while there is no hook thread
    unsigned short _heldPauseVk = 0;
    bool _injectedF4Held = false;

    std::jthread _inputThread;
    HANDLE _inputWake = nullptr;
    std::mutex _inputMutex;
    std::vector<InputEvent> _inputQueue;
};
