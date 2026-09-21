#include "App.h"
#include "Modes.h"
#include "Updater.h"

static constexpr const wchar_t* AUTOSTART_REG_KEY = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
static constexpr const wchar_t* AUTOSTART_REG_VALUE = L"Spammy";

App& App::Instance()
{
    static App s_app;
    return s_app;
}

bool App::Init(int argc, char** argv)
{
    SetCurrentDirectoryW(GetModulePath().parent_path().c_str());

    bool firstRun = !std::filesystem::is_regular_file(CONFIG_FILE);
    if (!sConfig.Load()) {
        int action =
            MessageBoxW(NULL, L"Can't load config, reset to defaults?", L"" APP_NAME, MB_ICONQUESTION | MB_OKCANCEL);
        if (action != IDOK) return false;
    }
    if (firstRun) EnableAutoStart(true);

    if (sConfig.profiles.empty()) {
        sConfig.CreateProfile("UNNAMED");
        for (unsigned short vk = '1'; vk <= '5'; vk++)
            sConfig.editingProfile->keys[vk][KeyMod_None].action = Action_Spammy;
    }

    _mainWindow = std::make_unique<MainWindow>(L"" APP_NAME);
    if (!_mainWindow->Initialize()) return false;

    bool autolaunch = argc > 1 && strcmp(argv[1], "autolaunch") == 0;
    if (!autolaunch) {
        _mainWindow->Update();
        _mainWindow->Show();
    }

    StartInputWorker();
    sKeyboard.OnPress([this](UINT vkCode, bool repeat) { return OnKeyEvent(true, vkCode, repeat); });
    sKeyboard.OnRelease([this](UINT vkCode, bool repeat) { return OnKeyEvent(false, vkCode, repeat); });

    sUpdater.CheckAsync();
    return true;
}

void App::Uninit()
{
    sKeyboard.Detach();
    StopInputWorker();
    sConfig.Save();
    _mainWindow.reset();
}

void App::Run()
{
    while (!_mainWindow->MustQuit()) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (_mainWindow->WantQuit()) {
            if (sConfig.minimizeToTray)
                _mainWindow->Hide();
            else
                _mainWindow->Close();
        }
        if (_mainWindow->IsWndNormalized()) _mainWindow->Update();

        UpdateActiveTarget();
        sConfig.SaveIfDirty();

        Sleep(10); // don't abuse cpu X_x
    }
    _mainWindow->Cleanup();
}

void App::Enable(bool state)
{
    sConfig.enabled = state;
    sConfig.MarkDirty();
}

bool App::IsEnabled()
{
    return sConfig.enabled;
}

static const std::wstring& AutoStartCommand()
{
    static const std::wstring s_command = L"\"" + GetModulePath().wstring() + L"\" autolaunch";
    return s_command;
}

bool App::IsAutoStartEnabled()
{
    wchar_t value[MAX_PATH + 32] = {0};
    DWORD size = sizeof(value);
    LSTATUS status =
        RegGetValueW(HKEY_CURRENT_USER, AUTOSTART_REG_KEY, AUTOSTART_REG_VALUE, RRF_RT_REG_SZ, NULL, value, &size);
    return status == ERROR_SUCCESS && value == AutoStartCommand();
}

bool App::EnableAutoStart(bool state)
{
    if (!state) return RegDeleteKeyValueW(HKEY_CURRENT_USER, AUTOSTART_REG_KEY, AUTOSTART_REG_VALUE) == ERROR_SUCCESS;
    const std::wstring& command = AutoStartCommand();
    return RegSetKeyValueW(HKEY_CURRENT_USER, AUTOSTART_REG_KEY, AUTOSTART_REG_VALUE, REG_SZ, command.c_str(),
                           (DWORD)((command.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

static void PlayEnabledSound(bool enabled)
{
    wchar_t path[MAX_PATH];
    if (!GetWindowsDirectoryW(path, std::size(path))) return;
    wcscat_s(path, enabled ? L"\\Media\\Windows Hardware Insert.wav" : L"\\Media\\Windows Hardware Remove.wav");
    PlaySoundW(path, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

std::pair<std::shared_ptr<Profile>, HWND> App::ActiveTarget()
{
    std::lock_guard lock(_callbackMutex);
    return {_activeProfile, _activeHwnd};
}

bool App::OnKeyEvent(bool down, UINT vkCode, bool repeat)
{
    auto [profile, activeHwnd] = ActiveTarget();
    if (_mainWindow) {
        bool selfFocused = activeHwnd == _mainWindow->Native();
        if (down ? _mainWindow->HandleKeyPress(vkCode, repeat, selfFocused) : _mainWindow->HandleKeyRelease(vkCode))
            return true;
    }
    if (!profile) return false;

    unsigned mods = sKeyboard.TestModifiers();
    if (profile->vkPause == MAKE_KEY_BUNDLE(vkCode, mods)) {
        if (!down) PostInput({InputEvent::Kind_TogglePause});
        return true;
    }
    if (profile->disableWin && (vkCode == VK_RWIN || vkCode == VK_LWIN)) return true;
    if (profile->disableAltF4 && vkCode == VK_F4 && (mods & KeyMod_Alt)) return true;
    if (!sConfig.enabled) return false;

    const KeyMode* mode = FindKeyMode(ResolveKeyAction(*profile, vkCode, mods));
    if (!mode || !(down ? mode->onPress : mode->onRelease)) return false;
    // typematic auto-repeat of a swallowed key: nothing to do, don't wake the worker for it
    if (repeat) return true;
    PostInput({down ? InputEvent::Kind_Press : InputEvent::Kind_Release, repeat, (unsigned short)vkCode, mods,
               std::move(profile)});
    return true;
}

void App::PostInput(InputEvent&& ev)
{
    {
        std::lock_guard lock(_inputMutex);
        _inputQueue.push_back(std::move(ev));
    }
    SetEvent(_inputWake);
}

void App::StartInputWorker()
{
    _inputWake = CreateEventW(NULL, FALSE, FALSE, NULL);
    _inputThread = std::jthread([this](std::stop_token stop) { InputWorkerProc(stop); });
}

void App::StopInputWorker()
{
    if (_inputThread.joinable()) {
        _inputThread.request_stop();
        SetEvent(_inputWake);
        _inputThread.join();
    }
    if (_inputWake) {
        CloseHandle(_inputWake);
        _inputWake = NULL;
    }
    _inputQueue.clear();
}

void App::InputWorkerProc(std::stop_token stop)
{
    using Clock = std::chrono::steady_clock;
    using Ms = std::chrono::milliseconds;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    timeBeginPeriod(1); // 1ms granularity for the legacy-timer fallback (and Sleep elsewhere)

    // sub-millisecond wakeups on Win10 1803+, plain waitable timer otherwise
    HANDLE timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer) timer = CreateWaitableTimerW(NULL, TRUE, NULL);
    HANDLE waitables[] = {_inputWake, timer};

    std::vector<InputEvent> batch;
    Clock::time_point lastTick = Clock::now();
    while (!stop.stop_requested()) {
        std::shared_ptr<Profile> profile = ActiveProfile();
        Ms period(profile ? profile->speed : 0);

        if (profile) {
            Clock::time_point due = lastTick + period;
            Clock::time_point now = Clock::now();
            // fell behind by more than a period (stall, speed change): resync instead of firing a burst
            if (now - due > period) due = lastTick = now;
            auto rel = std::chrono::duration_cast<std::chrono::nanoseconds>(due - now).count();
            LARGE_INTEGER dueTime;
            dueTime.QuadPart = rel > 0 ? -(rel / 100) : 0; // negative = relative, 100ns units
            SetWaitableTimer(timer, &dueTime, 0, NULL, NULL, FALSE);
        } else {
            CancelWaitableTimer(timer);
        }
        WaitForMultipleObjects(std::size(waitables), waitables, FALSE, INFINITE);
        if (stop.stop_requested()) break;

        {
            std::lock_guard lock(_inputMutex);
            batch.swap(_inputQueue);
        }
        for (const InputEvent& ev : batch)
            HandleInput(ev);
        batch.clear();

        if (profile && Clock::now() - lastTick >= period) {
            if (sConfig.enabled) TickAutofire(*profile);
            lastTick += period; // phase-locked: wakeup jitter doesn't accumulate into drift
        }
    }

    CloseHandle(timer);
    timeEndPeriod(1);
}

void App::HandleInput(const InputEvent& ev)
{
    if (ev.kind == InputEvent::Kind_TogglePause) {
        sConfig.enabled = !sConfig.enabled;
        sConfig.MarkDirty();
        if (sConfig.soundsEnabled) PlayEnabledSound(sConfig.enabled);
        return;
    }
    // re-resolve against the profile snapshot taken at hook time so press/release always pair up
    const KeyMode* mode = FindKeyMode(ResolveKeyAction(*ev.profile, ev.vkCode, ev.mods));
    if (!mode) return;
    auto handler = ev.kind == InputEvent::Kind_Press ? mode->onPress : mode->onRelease;
    if (handler) handler({ev.vkCode, ev.repeat, *ev.profile});
}

void App::TickAutofire(const Profile& profile)
{
    unsigned mods = sKeyboard.TestModifiers();
    for (unsigned short vk = 1; vk < kKeyboardKeysCount; vk++) {
        if (!sKeyboard.IsPressed(vk)) continue;
        const KeyMode* mode = FindKeyMode(ResolveKeyAction(profile, vk, mods));
        if (mode && mode->onTick) mode->onTick({vk, false, profile});
    }
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

void App::DeleteProfile(const char* name)
{
    std::shared_ptr<Profile> profile = sConfig.FindProfile(name);
    if (!profile) return;
    {
        std::lock_guard lock(_callbackMutex);
        if (profile == _activeProfile) _activeProfile = nullptr;
    }
    sConfig.DeleteProfile(name);
}

void App::UpdateActiveTarget()
{
    HWND hwnd = GetForegroundWindow();
    if (hwnd == _activeHwnd) return; // only the main thread writes _activeHwnd, so this read needs no lock

    std::shared_ptr<Profile> profile;
    std::string app;
    if (std::filesystem::path path = hwnd ? GetProcessPath(hwnd) : std::filesystem::path(); path.has_filename()) {
        app = Utf8FileName(path);
        profile = sConfig.FindProfileByApp(app.c_str());
        // the global (unbound) profile applies to everything but ourselves
        if (!profile && path != GetModulePath()) profile = sConfig.FindGlobalProfile();
    }

    {
        std::lock_guard lock(_callbackMutex);
        _activeProfile = profile;
        _activeHwnd = hwnd;
        _activeApp = profile ? std::move(app) : std::string();
    }
    // presses queued for the previous window must not be injected into the new one
    {
        std::lock_guard lock(_inputMutex);
        _inputQueue.clear();
    }

    bool selfFocused = _mainWindow && hwnd == _mainWindow->Native();
    if (profile || selfFocused)
        sKeyboard.Attach(selfFocused || profile->UsesMouse()); // own window logs mouse presses for the key editor
    else
        sKeyboard.Detach();
}

int main(int argc, char** argv)
{
    bool ok = sApp.Init(argc, argv);
    if (ok) sApp.Run();
    sApp.Uninit();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
