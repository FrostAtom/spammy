#include "App.h"
#include "Config.h"
#include "Updater.h"
#include "Utils.h"

static constexpr const wchar_t* AUTOSTART_REG_KEY = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
static constexpr const wchar_t* AUTOSTART_REG_VALUE = L"Spammy";
static constexpr DWORD MAIN_LOOP_SLEEP_MS = 10;

App& App::Instance()
{
    static App s_app;
    return s_app;
}

bool App::Init(int argc, char** argv)
{
    SetCurrentDirectoryW(GetModulePath().parent_path().c_str());

    if (!sConfig.Load()) {
        const int action =
            MessageBoxW(nullptr, L"Can't load config, reset to defaults?", L"" APP_NAME, MB_ICONQUESTION | MB_OKCANCEL);
        if (action != IDOK) return false;
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
        if (_mainWindow->IsOnScreen()) _mainWindow->Update();
        _mainWindow->SyncTrayIcon();

        UpdateActiveTarget();
        sConfig.SaveIfDirty();

        Sleep(MAIN_LOOP_SLEEP_MS);
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
    const std::wstring& command = AutoStartCommand();
    // a stored value longer than ours fails with ERROR_MORE_DATA: it isn't ours either
    std::wstring stored(command.size() + 1, L'\0');
    DWORD storedBytes = (DWORD)(stored.size() * sizeof(wchar_t));
    const LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, AUTOSTART_REG_KEY, AUTOSTART_REG_VALUE, RRF_RT_REG_SZ,
                                        nullptr, stored.data(), &storedBytes);
    return status == ERROR_SUCCESS && wcscmp(stored.c_str(), command.c_str()) == 0;
}

bool App::EnableAutoStart(bool state)
{
    if (!state) return RegDeleteKeyValueW(HKEY_CURRENT_USER, AUTOSTART_REG_KEY, AUTOSTART_REG_VALUE) == ERROR_SUCCESS;
    const std::wstring& command = AutoStartCommand();
    return RegSetKeyValueW(HKEY_CURRENT_USER, AUTOSTART_REG_KEY, AUTOSTART_REG_VALUE, REG_SZ, command.c_str(),
                           (DWORD)((command.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

static constexpr uint32_t kToggleSoundRate = 44100;

struct MarimbaPartial {
    float harmonic;
    float amplitude;
    float decayScale;
};

static void AddMarimbaNote(std::vector<float>& buf, float start, float freq, float tau, float gain,
                           std::span<const MarimbaPartial> partials)
{
    constexpr double kTwoPi = 6.283185307179586;
    constexpr float kFadeIn = 0.0025f;
    const size_t first = (size_t)(start * kToggleSoundRate);
    const size_t count = std::min((size_t)(tau * 5 * kToggleSoundRate), buf.size() - std::min(first, buf.size()));
    for (size_t i = 0; i < count; i++) {
        const double t = (double)i / kToggleSoundRate;
        double s = 0;
        for (const MarimbaPartial& p : partials)
            s += p.amplitude * std::sin(kTwoPi * freq * p.harmonic * t) * std::exp(-t / (tau * p.decayScale));
        if (t < kFadeIn) s *= 0.5 - 0.5 * std::cos(kTwoPi / 2 * t / kFadeIn);
        buf[first + i] += (float)s * gain;
    }
}

static std::vector<char> SynthToggleWav(bool on)
{
    static constexpr MarimbaPartial kBright[] = {{1, 1.f, 1.f}, {4, 0.30f, 0.3f}, {10, 0.06f, 0.15f}};
    static constexpr MarimbaPartial kDull[] = {{1, 1.f, 1.f}, {4, 0.12f, 0.3f}};
    constexpr float kC5 = 523.f, kC6 = 1046.f;
    constexpr float kDetuneCents = 5.f;
    constexpr float kMinus12Db = 0.2512f, kMinus14_5Db = 0.1884f;
    constexpr float kLanding = 0.055f, kLength = 0.525f, kTailFade = 0.020f;
    constexpr uint32_t kCount = (uint32_t)(kToggleSoundRate * kLength);
    constexpr uint32_t kDataSize = kCount * 2 * sizeof(int16_t);
    enum {
        Left,
        Right
    };

    const std::span<const MarimbaPartial> timbre =
        on ? std::span<const MarimbaPartial>(kBright) : std::span<const MarimbaPartial>(kDull);
    const float from = on ? kC5 : kC6, to = on ? kC6 : kC5;
    std::vector<float> channels[2] = {std::vector<float>(kCount), std::vector<float>(kCount)};
    for (int c : {Left, Right}) {
        const bool right = c == Right;
        const float detune = std::exp2((right ? -kDetuneCents : kDetuneCents) / 1200.f);
        const float echoDelay = right ? 0.110f : 0.220f, echoGain = right ? 0.28f : 0.12f;
        AddMarimbaNote(channels[c], 0, from * detune, 0.030f, 1.f, timbre);
        AddMarimbaNote(channels[c], kLanding, to * detune, 0.050f, 1.f, timbre);
        AddMarimbaNote(channels[c], kLanding + echoDelay, to * detune, 0.050f, echoGain, timbre);
    }

    float peak = 0;
    for (std::vector<float>& ch : channels) {
        for (uint32_t i = 0; i < kCount; i++) {
            const float remaining = (float)(kCount - 1 - i) / kToggleSoundRate;
            if (remaining < kTailFade) ch[i] *= remaining / kTailFade;
            peak = std::max(peak, std::abs(ch[i]));
        }
    }
    const float scale = (on ? kMinus12Db : kMinus14_5Db) / std::max(peak, 1e-6f);

    std::vector<char> wav(44 + kDataSize);
    char* p = wav.data();
    auto put = [&p](auto v) {
        memcpy(p, &v, sizeof(v));
        p += sizeof(v);
    };
    auto tag = [&p](const char* s) {
        memcpy(p, s, 4);
        p += 4;
    };
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
    // SND_ASYNC keeps reading the SND_MEMORY buffer after returning
    static const std::vector<char> s_on = SynthToggleWav(true);
    static const std::vector<char> s_off = SynthToggleWav(false);
    PlaySoundW((LPCWSTR)(enabled ? s_on : s_off).data(), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

static void PressWithoutAlt(unsigned short vkCode)
{
    sKeyboard.KeyDownWithoutAlt(vkCode);
}

static void ReleaseKey(unsigned short vkCode)
{
    sKeyboard.KeyUp(vkCode);
}

std::pair<std::shared_ptr<Profile>, HWND> App::ActiveTarget()
{
    std::scoped_lock lock(_targetMutex);
    return {_activeProfile, _activeHwnd};
}

bool App::OnKeyEvent(bool down, UINT vkCode, bool repeat)
{
    // first: an up must pair with how its down was handled, even across a focus switch or a pause key capture
    if (vkCode == _heldPauseVk) {
        if (!down) _heldPauseVk = 0;
        return true;
    }
    if (vkCode == VK_F4 && _injectedF4Held) {
        if (!down) {
            _injectedF4Held = false;
            PostInput({.handler = &ReleaseKey, .vkCode = VK_F4, .survivesFocusSwitch = true});
        }
        return true;
    }

    auto [profile, activeHwnd] = ActiveTarget();
    const bool selfFocused = activeHwnd == _mainWindow->Native();
    if (down ? _mainWindow->HandleKeyPress(vkCode, repeat, selfFocused) : _mainWindow->HandleKeyRelease(vkCode))
        return true;
    if (!profile) return false;

    const unsigned mods = sKeyboard.TestModifiers();
    const unsigned requiredPauseMods = KeyBundleMods(profile->pauseKey);
    const bool requiredPauseModsHeld = (mods & requiredPauseMods) == requiredPauseMods;
    if (down && !repeat && KeyBundleVk(profile->pauseKey) == vkCode && requiredPauseModsHeld) {
        _heldPauseVk = (unsigned short)vkCode;
        PostInput(InputEvent::PauseToggle());
        return true;
    }
    if (profile->disableWin && (vkCode == VK_RWIN || vkCode == VK_LWIN)) return true;
    if (profile->disableAltF4 && vkCode == VK_F4 && (mods & KeyMod_Alt)) {
        if (down && !repeat) {
            _injectedF4Held = true;
            sKeyboard.MaskAlt();
            PostInput({.handler = &PressWithoutAlt, .vkCode = VK_F4, .survivesFocusSwitch = true});
        }
        return true;
    }
    if (!sConfig.enabled) return false;

    const KeyMode* mode = FindKeyMode(ResolveKeyAction(*profile, vkCode, mods));
    const KeyHandler_t handler = mode ? (down ? mode->onPress : mode->onRelease) : nullptr;
    if (!handler) return false;
    if (!repeat) PostInput({.handler = handler, .vkCode = (unsigned short)vkCode});
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
    using Ticks100ns = std::chrono::duration<LONGLONG, std::ratio<1, 10'000'000>>;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    timeBeginPeriod(1);

    // the high resolution timer needs Win10 1803+
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer) timer = CreateWaitableTimerW(nullptr, TRUE, nullptr);
    HANDLE waitables[] = {_inputWake, timer};
    const DWORD waitCount = timer ? 2 : 1;

    std::vector<InputEvent> batch;
    Clock::time_point lastTickDue = Clock::now();
    while (!stop.stop_requested()) {
        const std::shared_ptr<Profile> profile = ActiveTarget().first;
        const Ms period(profile ? profile->intervalMs : 0);

        DWORD timeout = INFINITE;
        if (profile) {
            Clock::time_point due = lastTickDue + period;
            const Clock::time_point now = Clock::now();
            const bool fellBehindByAPeriod = now - due > period;
            if (fellBehindByAPeriod) due = lastTickDue = now;
            const std::chrono::nanoseconds untilDue = due - now;
            const bool overdue = untilDue <= Clock::duration::zero();
            if (timer) {
                LARGE_INTEGER relativeDueTime;
                relativeDueTime.QuadPart = overdue ? 0 : -std::chrono::duration_cast<Ticks100ns>(untilDue).count();
                SetWaitableTimer(timer, &relativeDueTime, 0, nullptr, nullptr, FALSE);
            } else {
                timeout = overdue ? 0 : (DWORD)std::chrono::ceil<Ms>(untilDue).count();
            }
        } else if (timer) {
            CancelWaitableTimer(timer);
        }
        WaitForMultipleObjects(waitCount, waitables, FALSE, timeout);
        if (stop.stop_requested()) break;

        {
            std::scoped_lock lock(_inputMutex);
            batch.swap(_inputQueue);
        }
        for (const InputEvent& ev : batch)
            HandleInput(ev);
        batch.clear();

        const bool stillActive = ActiveTarget().first == profile;
        if (profile && stillActive && Clock::now() - lastTickDue >= period) {
            if (sConfig.enabled) TickAutofire(*profile);
            lastTickDue += period;
        }
    }

    if (timer) CloseHandle(timer);
    timeEndPeriod(1);
}

void App::HandleInput(const InputEvent& ev)
{
    if (!ev.IsPauseToggle()) {
        ev.handler(ev.vkCode);
        return;
    }
    const bool enabled = !IsEnabled();
    Enable(enabled);
    if (sConfig.soundsEnabled) PlayEnabledSound(enabled);
}

void App::TickAutofire(const Profile& profile)
{
    const unsigned mods = sKeyboard.TestModifiers();
    for (unsigned short vk = 1; vk < kKeyboardKeysCount; vk++) {
        if (!sKeyboard.IsPressed(vk)) continue;
        const bool tickWouldSendAltF4 = vk == VK_F4 && (mods & KeyMod_Alt);
        if (tickWouldSendAltF4 && profile.disableAltF4) continue;
        const KeyMode* mode = FindKeyMode(ResolveKeyAction(profile, vk, mods));
        if (mode && mode->onTick) mode->onTick(vk);
    }
}

void App::DeleteProfile(const std::shared_ptr<Profile>& profile)
{
    {
        std::scoped_lock lock(_targetMutex);
        if (profile == _activeProfile) _activeProfile = nullptr;
    }
    sConfig.DeleteProfile(profile);
}

std::shared_ptr<Profile> App::LastActiveProfile()
{
    std::shared_ptr<Profile> profile = _lastActiveProfile.lock();
    const bool deletedButStillAlive = profile && std::ranges::find(sConfig.profiles, profile) == sConfig.profiles.end();
    return deletedButStillAlive ? nullptr : profile;
}

void App::UpdateActiveTarget()
{
    HWND hwnd = GetForegroundWindow();
    if (hwnd == _activeHwnd) return;

    std::shared_ptr<Profile> profile;
    if (const std::filesystem::path path = hwnd ? GetProcessPath(hwnd) : std::filesystem::path(); path.has_filename())
        profile = sConfig.FindProfileByApp(Utf8FileName(path).c_str());
    if (profile) _lastActiveProfile = profile;

    {
        std::scoped_lock lock(_targetMutex);
        _activeProfile = profile;
        _activeHwnd = hwnd;
    }
    {
        std::scoped_lock lock(_inputMutex);
        std::erase_if(_inputQueue, [](const InputEvent& ev) { return !ev.survivesFocusSwitch; });
    }
    SetEvent(_inputWake); // the worker's timer still runs at the old profile's interval

    const bool keyEditorLogsPresses = hwnd == _mainWindow->Native();
    if (profile || keyEditorLogsPresses)
        sKeyboard.Attach(keyEditorLogsPresses || profile->UsesMouse());
    else
        sKeyboard.Detach();
}

int main(int argc, char** argv)
{
    // no Uninit after a failed Init: its Save would overwrite a config we refused to reset or another instance owns
    if (!sApp.Init(argc, argv)) return EXIT_FAILURE;
    sApp.Run();
    sApp.Uninit();
    return EXIT_SUCCESS;
}
