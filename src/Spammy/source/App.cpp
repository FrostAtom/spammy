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

static constexpr uint32_t kToggleSoundRate = 44100;

struct MarimbaPartial
{
    float mult, amp, decay; // harmonic multiplier, amplitude, decay time relative to the fundamental's
};

// a struck-bar tone: the upper partials die out faster than the fundamental
static void AddMarimbaNote(std::vector<float>& buf, float start, float freq, float tau, float gain,
                           std::span<const MarimbaPartial> partials)
{
    constexpr double kTwoPi = 6.283185307179586;
    constexpr float kAttack = 0.0025f;
    const size_t first = (size_t)(start * kToggleSoundRate);
    const size_t count = std::min((size_t)(tau * 5 * kToggleSoundRate), buf.size() - std::min(first, buf.size()));
    for (size_t i = 0; i < count; i++) {
        const double t = (double)i / kToggleSoundRate;
        double s = 0;
        for (const MarimbaPartial& p : partials)
            s += p.amp * std::sin(kTwoPi * freq * p.mult * t) * std::exp(-t / (tau * p.decay));
        if (t < kAttack) s *= 0.5 - 0.5 * std::cos(kTwoPi / 2 * t / kAttack); // soft onset instead of a pop
        buf[first + i] += (float)s * gain;
    }
}

// 16-bit stereo WAV: two marimba strikes an octave apart (up for on, down for off), detuned a hair per ear for
// width, with a ping-pong echo of the landing note. On is brighter and louder, off duller and quieter
static std::vector<char> SynthToggleWav(bool on)
{
    static constexpr MarimbaPartial kBright[] = {{1, 1.f, 1.f}, {4, 0.30f, 0.3f}, {10, 0.06f, 0.15f}};
    static constexpr MarimbaPartial kDull[] = {{1, 1.f, 1.f}, {4, 0.12f, 0.3f}};
    constexpr float kLanding = 0.055f, kLength = 0.525f, kTailFade = 0.020f;
    constexpr uint32_t kCount = (uint32_t)(kToggleSoundRate * kLength);
    constexpr uint32_t kDataSize = kCount * 2 * sizeof(int16_t);

    const std::span<const MarimbaPartial> timbre =
        on ? std::span<const MarimbaPartial>(kBright) : std::span<const MarimbaPartial>(kDull);
    const float from = on ? 523.f : 1046.f, to = on ? 1046.f : 523.f;
    std::vector<float> channels[2] = {std::vector<float>(kCount), std::vector<float>(kCount)};
    for (int c = 0; c < 2; c++) {
        const float detune = std::exp2((c == 0 ? 5.f : -5.f) / 1200.f); // +-5 cents
        AddMarimbaNote(channels[c], 0, from * detune, 0.030f, 1.f, timbre);
        AddMarimbaNote(channels[c], kLanding, to * detune, 0.050f, 1.f, timbre);
        // right ear echoes first, then left
        AddMarimbaNote(channels[c], kLanding + (c == 1 ? 0.110f : 0.220f), to * detune, 0.050f,
                       c == 1 ? 0.28f : 0.12f, timbre);
    }

    float peak = 0;
    for (std::vector<float>& ch : channels) {
        for (uint32_t i = 0; i < kCount; i++) {
            const float left = (float)(kCount - 1 - i) / kToggleSoundRate;
            if (left < kTailFade) ch[i] *= left / kTailFade; // land on exact silence
            peak = std::max(peak, std::abs(ch[i]));
        }
    }
    const float scale = (on ? 0.2512f : 0.1884f) / std::max(peak, 1e-6f); // peak at -12 / -14.5 dB

    std::vector<char> wav(44 + kDataSize);
    char* p = wav.data();
    auto put = [&p](auto v) { memcpy(p, &v, sizeof(v)); p += sizeof(v); };
    auto tag = [&p](const char* s) { memcpy(p, s, 4); p += 4; };
    tag("RIFF"), put(36 + kDataSize), tag("WAVE");
    tag("fmt "), put(16u), put(uint16_t(WAVE_FORMAT_PCM)), put(uint16_t(2)), put(kToggleSoundRate),
        put(kToggleSoundRate * 4), put(uint16_t(4)), put(uint16_t(16));
    tag("data"), put(kDataSize);
    for (uint32_t i = 0; i < kCount; i++)
        for (const std::vector<float>& ch : channels)
            put(int16_t(std::clamp(ch[i] * scale, -1.f, 1.f) * 32767));
    return wav;
}

static void PlayEnabledSound(bool enabled)
{
    // SND_ASYNC | SND_MEMORY reads the buffer while playing, so it has to outlive the call
    static const std::vector<char> s_on = SynthToggleWav(true);
    static const std::vector<char> s_off = SynthToggleWav(false);
    PlaySoundW((LPCWSTR)(enabled ? s_on : s_off).data(), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
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
