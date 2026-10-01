#include "App.h"
#include "Config.h"
#include "Updater.h"
#include "Utils.h"

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

    const bool firstRun = !std::filesystem::is_regular_file(CONFIG_FILE);
    if (!sConfig.Load()) {
        const int action =
            MessageBoxW(nullptr, L"Can't load config, reset to defaults?", L"" APP_NAME, MB_ICONQUESTION | MB_OKCANCEL);
        if (action != IDOK) return false;
    }
    if (firstRun) EnableAutoStart(true);

    if (sConfig.profiles.empty()) {
        sConfig.CreateProfile("UNNAMED");
        for (unsigned short vk = '1'; vk <= '5'; vk++)
            sConfig.editingProfile->keys[vk][KeyMod_None].action = Action_Spammy;
    }

    _mainWindow = std::make_unique<MainWindow>(L"" APP_NAME);
    if (!_mainWindow->Initialize()) {
        _mainWindow.reset();
        return false;
    }

    const bool autolaunch = argc > 1 && strcmp(argv[1], "autolaunch") == 0;
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
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (_mainWindow->WantQuit()) _mainWindow->RequestClose();
        if (_mainWindow->IsWndNormalized()) _mainWindow->Update();
        _mainWindow->SyncTrayIcon(); // pause can flip on the input worker, so poll it here on the UI thread

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

bool App::IsEnabled() const
{
    return sConfig.enabled;
}

static const std::wstring& AutoStartCommand()
{
    static const std::wstring s_command = L"\"" + GetModulePath().wstring() + L"\" autolaunch";
    return s_command;
}

bool App::IsAutoStartEnabled() const
{
    wchar_t value[MAX_PATH + 32] = {0};
    DWORD size = sizeof(value);
    const LSTATUS status =
        RegGetValueW(HKEY_CURRENT_USER, AUTOSTART_REG_KEY, AUTOSTART_REG_VALUE, RRF_RT_REG_SZ, nullptr, value, &size);
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
    PlaySoundW(path, nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

std::pair<std::shared_ptr<Profile>, HWND> App::ActiveTarget()
{
    std::scoped_lock lock(_targetMutex);
    return {_activeProfile, _activeHwnd};
}

bool App::OnKeyEvent(bool down, UINT vkCode, bool repeat)
{
    auto [profile, activeHwnd] = ActiveTarget();
    // the hooks are attached after _mainWindow is created and detached (joined) before it's destroyed
    const bool selfFocused = activeHwnd == _mainWindow->Native();
    if (down ? _mainWindow->HandleKeyPress(vkCode, repeat, selfFocused) : _mainWindow->HandleKeyRelease(vkCode))
        return true;
    // the pause key is decided on its down edge and that decision sticks until its up: letting go of a modifier
    // first (or grabbing one mid-hold) must neither drop the toggle nor leak the up to the game
    if (vkCode == _heldPauseVk) {
        if (!down) _heldPauseVk = 0;
        return true;
    }
    if (!profile) return false;

    const unsigned mods = sKeyboard.TestModifiers();
    // extra held modifiers (sprint on Shift, crouch on Ctrl, ...) don't get in the way, only the bound ones are required
    const unsigned pauseMods = KeyBundleMods(profile->vkPause);
    if (down && !repeat && KeyBundleVk(profile->vkPause) == vkCode && (mods & pauseMods) == pauseMods) {
        _heldPauseVk = (unsigned short)vkCode;
        PostInput({.handler = nullptr});
        return true;
    }
    if (profile->disableWin && (vkCode == VK_RWIN || vkCode == VK_LWIN)) return true;
    if (profile->disableAltF4 && vkCode == VK_F4 && (mods & KeyMod_Alt)) return true;
    if (!sConfig.enabled) return false;

    const KeyMode* mode = FindKeyMode(ResolveKeyAction(*profile, vkCode, mods));
    const KeyHandler_t handler = mode ? (down ? mode->onPress : mode->onRelease) : nullptr;
    if (!handler) return false;
    // typematic auto-repeat of a swallowed key: nothing to do, don't wake the worker for it
    if (!repeat) PostInput({handler, (unsigned short)vkCode});
    return true;
}

void App::PostInput(InputEvent ev)
{
    {
        std::scoped_lock lock(_inputMutex);
        _inputQueue.push_back(ev);
    }
    SetEvent(_inputWake);
}

void App::StartInputWorker()
{
    _inputWake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
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
        _inputWake = nullptr;
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
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer) timer = CreateWaitableTimerW(nullptr, TRUE, nullptr);
    HANDLE waitables[] = {_inputWake, timer};

    std::vector<InputEvent> batch;
    Clock::time_point lastTick = Clock::now();
    while (!stop.stop_requested()) {
        const std::shared_ptr<Profile> profile = ActiveProfile();
        const Ms period(profile ? profile->speed : 0);

        if (profile) {
            Clock::time_point due = lastTick + period;
            const Clock::time_point now = Clock::now();
            // fell behind by more than a period (stall, speed change): resync instead of firing a burst
            if (now - due > period) due = lastTick = now;
            auto rel = std::chrono::duration_cast<std::chrono::nanoseconds>(due - now).count();
            LARGE_INTEGER dueTime;
            dueTime.QuadPart = rel > 0 ? -(rel / 100) : 0; // negative = relative, 100ns units
            SetWaitableTimer(timer, &dueTime, 0, nullptr, nullptr, FALSE);
        } else {
            CancelWaitableTimer(timer);
        }
        WaitForMultipleObjects(std::size(waitables), waitables, FALSE, INFINITE);
        if (stop.stop_requested()) break;

        {
            std::scoped_lock lock(_inputMutex);
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
    if (!ev.IsPauseToggle()) {
        ev.handler(ev.vkCode);
        return;
    }
    sConfig.enabled = !sConfig.enabled;
    sConfig.MarkDirty();
    if (sConfig.soundsEnabled) PlayEnabledSound(sConfig.enabled);
}

void App::TickAutofire(const Profile& profile)
{
    const unsigned mods = sKeyboard.TestModifiers();
    for (unsigned short vk = 1; vk < kKeyboardKeysCount; vk++) {
        if (!sKeyboard.IsPressed(vk)) continue;
        const KeyMode* mode = FindKeyMode(ResolveKeyAction(profile, vk, mods));
        if (mode && mode->onTick) mode->onTick(vk);
    }
}

std::shared_ptr<Profile> App::ActiveProfile()
{
    std::scoped_lock lock(_targetMutex);
    return _activeProfile;
}

void App::DeleteProfile(const char* name)
{
    const std::shared_ptr<Profile> profile = sConfig.FindProfile(name);
    if (!profile) return;
    {
        std::scoped_lock lock(_targetMutex);
        if (profile == _activeProfile) _activeProfile = nullptr;
    }
    sConfig.DeleteProfile(name);
}

void App::UpdateActiveTarget()
{
    HWND hwnd = GetForegroundWindow();
    if (hwnd == _activeHwnd) return; // only the main thread writes _activeHwnd, so this read needs no lock

    std::shared_ptr<Profile> profile;
    if (const std::filesystem::path path = hwnd ? GetProcessPath(hwnd) : std::filesystem::path(); path.has_filename()) {
        profile = sConfig.FindProfileByApp(Utf8FileName(path).c_str());
        // the global (unbound) profile applies to everything but ourselves
        if (!profile && path != GetModulePath()) profile = sConfig.FindGlobalProfile();
    }

    {
        std::scoped_lock lock(_targetMutex);
        _activeProfile = profile;
        _activeHwnd = hwnd;
    }
    // presses queued for the previous window must not be injected into the new one; a pending pause toggle still counts
    {
        std::scoped_lock lock(_inputMutex);
        std::erase_if(_inputQueue, [](const InputEvent& ev) { return !ev.IsPauseToggle(); });
    }

    const bool selfFocused = hwnd == _mainWindow->Native();
    if (profile || selfFocused)
        sKeyboard.Attach(selfFocused || profile->UsesMouse()); // own window logs mouse presses for the key editor
    else
        sKeyboard.Detach();
}

int main(int argc, char** argv)
{
    // a failed Init leaves nothing to undo, and must not reach Uninit: saving there would overwrite the config
    // the user just refused to reset, or the one owned by the instance that is already running
    if (!sApp.Init(argc, argv)) return EXIT_FAILURE;
    sApp.Run();
    sApp.Uninit();
    return EXIT_SUCCESS;
}
