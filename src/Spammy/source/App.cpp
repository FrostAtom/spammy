#include "App.h"
#include "Apps.h"
#include "Config.h"
#include "Updater.h"
#include <cstdlib>
#include <mmsystem.h>

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

    if (!sConfig.Load()) {
        const int action =
            MessageBoxW(nullptr, L"Can't load config, reset to defaults?", L"" APP_NAME, MB_ICONQUESTION | MB_OKCANCEL);
        if (action != IDOK) return false;
    }

    _mainWindow = std::make_unique<MainWindow>(L"" APP_NAME);
    if (!_mainWindow->Setup()) {
        _mainWindow.reset();
        return false;
    }

    sKeyboard.Listen(std::bind_front(&App::OnKeyEvent, this), std::bind_front(&App::OnKeyPressOrRelease, this),
                     std::bind_front(&App::ForgetHeldKeys, this));
    WINEVENTPROC onForegroundChange = [](HWINEVENTHOOK, DWORD, HWND shownOrActivated, LONG, LONG, DWORD, DWORD) {
        sApp.OnForegroundChange(shownOrActivated);
    };
    WINEVENTPROC onDesktopSwitch = [](HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) {
        sApp.OnDesktopSwitch();
    };
    auto hook = [](DWORD event, WINEVENTPROC handler) {
        return SetWinEventHook(event, event, nullptr, handler, 0, 0, WINEVENT_OUTOFCONTEXT);
    };
    _winEventHooks = {
        hook(EVENT_SYSTEM_FOREGROUND, onForegroundChange),
        // EVENT_SYSTEM_FOREGROUND is not sent for a window that Alt+Tab restores from minimized
        hook(EVENT_SYSTEM_MINIMIZEEND, onForegroundChange),
        hook(EVENT_SYSTEM_DESKTOPSWITCH, onDesktopSwitch),
    };
    if (std::ranges::contains(_winEventHooks, nullptr)) {
        std::ranges::for_each(_winEventHooks, &UnhookWinEvent);
        MessageBoxW(nullptr, L"Can't track the active window", L"" APP_NAME, MB_ICONERROR | MB_OK);
        _mainWindow.reset();
        return false;
    }

    const bool autolaunch = argc > 1 && strcmp(argv[1], "autolaunch") == 0;
    if (!autolaunch) {
        _mainWindow->Update();
        _mainWindow->Show();
    }

    StartInputWorker();
    SetActiveTarget(GetForegroundWindow());

    sUpdater.CheckAsync([uiStale = _uiStale] { SetEvent(uiStale.get()); });
    return true;
}

void App::Uninit()
{
    std::ranges::for_each(_winEventHooks, &UnhookWinEvent);
    sKeyboard.Detach();
    StopInputWorker();
    sConfig.Save();
    _mainWindow.reset();
}

void App::Run()
{
    HANDLE uiStale = _uiStale.get();
    for (;;) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (_mainWindow->WantQuit()) _mainWindow->RequestClose();
        const DWORD untilNextFrameMs = _mainWindow->RenderDueFrame();
        if (_mainWindow->MustQuit()) return;

        DWORD untilSaveMs = sConfig.MsUntilSaveDue();
        if (untilSaveMs == 0) {
            sConfig.Save();
            untilSaveMs = sConfig.MsUntilSaveDue();
        }
        const DWORD untilNextWorkMs = std::min(untilNextFrameMs, untilSaveMs);
        const DWORD wokenBy =
            MsgWaitForMultipleObjectsEx(1, &uiStale, untilNextWorkMs, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (wokenBy == WAIT_OBJECT_0) {
            _mainWindow->SyncTrayIcon();
            _mainWindow->RequestFrame();
        }
    }
}

void App::Enable(bool state)
{
    sConfig.enabled = state;
    sConfig.MarkDirty();
    SetEvent(_inputWake);
    SetEvent(_uiStale.get());
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
    DWORD storedBytes = (DWORD)stored.size() * (DWORD)sizeof(wchar_t);
    const LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, AUTOSTART_REG_KEY, AUTOSTART_REG_VALUE, RRF_RT_REG_SZ,
                                        nullptr, stored.data(), &storedBytes);
    return status == ERROR_SUCCESS && wcscmp(stored.c_str(), command.c_str()) == 0;
}

bool App::EnableAutoStart(bool state)
{
    if (!state) {
        const LSTATUS status = RegDeleteKeyValueW(HKEY_CURRENT_USER, AUTOSTART_REG_KEY, AUTOSTART_REG_VALUE);
        return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }
    const std::wstring& command = AutoStartCommand();
    return RegSetKeyValueW(HKEY_CURRENT_USER, AUTOSTART_REG_KEY, AUTOSTART_REG_VALUE, REG_SZ, command.c_str(),
                           (DWORD)(command.size() + 1) * (DWORD)sizeof(wchar_t)) == ERROR_SUCCESS;
}

static constexpr uint32_t kToggleSoundRate = 44100;
static constexpr float kToggleSoundSec = 0.525f;
static constexpr auto kToggleSoundFrames = (uint32_t)(kToggleSoundRate * kToggleSoundSec);
static constexpr uint32_t kToggleSoundBytes = kToggleSoundFrames * 2 * sizeof(int16_t);
static constexpr size_t kWavHeaderBytes = 44;
using ToggleWav = std::array<char, kWavHeaderBytes + kToggleSoundBytes>;

namespace {
struct MarimbaPartial {
    float harmonic;
    float amplitude;
    float decayScale;
};
} // namespace

static void AddMarimbaNote(std::vector<float>& buf, float start, float freq, float tau, float gain,
                           std::span<const MarimbaPartial> partials)
{
    constexpr double kTwoPi = 6.283185307179586;
    constexpr float kFadeIn = 0.0025f;
    const auto first = (size_t)(start * kToggleSoundRate);
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

static ToggleWav SynthToggleWav(bool on)
{
    static constexpr MarimbaPartial kBright[] = {{1, 1.f, 1.f}, {4, 0.30f, 0.3f}, {10, 0.06f, 0.15f}};
    static constexpr MarimbaPartial kDull[] = {{1, 1.f, 1.f}, {4, 0.12f, 0.3f}};
    constexpr float kC5 = 523.f;
    constexpr float kC6 = 1046.f;
    constexpr float kDetuneCents = 5.f;
    constexpr float kMinus12Db = 0.2512f;
    constexpr float kMinus14_5Db = 0.1884f;
    constexpr float kLanding = 0.055f;
    constexpr float kTailFade = 0.020f;
    enum : std::uint8_t {
        Left,
        Right
    };

    const std::span<const MarimbaPartial> timbre =
        on ? std::span<const MarimbaPartial>(kBright) : std::span<const MarimbaPartial>(kDull);
    const float from = on ? kC5 : kC6;
    const float to = on ? kC6 : kC5;
    std::vector<float> channels[2] = {std::vector<float>(kToggleSoundFrames), std::vector<float>(kToggleSoundFrames)};
    for (const auto side : {Left, Right}) {
        const bool right = side == Right;
        const float detune = std::exp2((right ? -kDetuneCents : kDetuneCents) / 1200.f);
        const float echoDelay = right ? 0.110f : 0.220f;
        const float echoGain = right ? 0.28f : 0.12f;
        AddMarimbaNote(channels[side], 0, from * detune, 0.030f, 1.f, timbre);
        AddMarimbaNote(channels[side], kLanding, to * detune, 0.050f, 1.f, timbre);
        AddMarimbaNote(channels[side], kLanding + echoDelay, to * detune, 0.050f, echoGain, timbre);
    }

    float peak = 0;
    for (std::vector<float>& ch : channels) {
        for (uint32_t i = 0; i < kToggleSoundFrames; i++) {
            const float remaining = (float)(kToggleSoundFrames - 1 - i) / kToggleSoundRate;
            if (remaining < kTailFade) ch[i] *= remaining / kTailFade;
            peak = std::max(peak, std::abs(ch[i]));
        }
    }
    const float scale = (on ? kMinus12Db : kMinus14_5Db) / std::max(peak, 1e-6f);

    ToggleWav wav{};
    char* p = wav.data();
    auto put = [&p](auto v) {
        memcpy(p, &v, sizeof(v));
        p += sizeof(v);
    };
    auto tag = [&p](const char* s) {
        memcpy(p, s, 4);
        p += 4;
    };
    tag("RIFF"), put(36 + kToggleSoundBytes), tag("WAVE");
    tag("fmt "), put(16u), put(uint16_t(WAVE_FORMAT_PCM)), put(uint16_t(2)), put(kToggleSoundRate),
        put(kToggleSoundRate * 4), put(uint16_t(4)), put(uint16_t(16));
    tag("data"), put(kToggleSoundBytes);
    for (uint32_t i = 0; i < kToggleSoundFrames; i++)
        for (const std::vector<float>& ch : channels)
            put(int16_t(std::clamp(ch[i] * scale, -1.f, 1.f) * 32767));
    return wav;
}

static void PlayEnabledSound(bool enabled)
{
    // SND_ASYNC keeps reading the SND_MEMORY buffer after returning, even while statics die at exit
    static_assert(std::is_trivially_destructible_v<ToggleWav>);
    static const ToggleWav s_on = SynthToggleWav(true);
    static const ToggleWav s_off = SynthToggleWav(false);
    PlaySoundW((LPCWSTR)(enabled ? s_on : s_off).data(), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

static void TogglePause(unsigned short /*pauseVk*/)
{
    const bool enabled = !sApp.IsEnabled();
    sApp.Enable(enabled);
    if (sConfig.soundsEnabled) PlayEnabledSound(enabled);
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

void App::OnKeyPressOrRelease()
{
    SetEvent(_inputWake);
    if (_mainWindow->IsOnScreen()) SetEvent(_uiStale.get());
}

bool App::OnKeyEvent(unsigned short vkCode, bool down, bool repeat)
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
    const KeyBundle_t pauseKey = profile->pauseKey;
    const unsigned requiredPauseMods = KeyBundleMods(pauseKey);
    const bool requiredPauseModsHeld = (mods & requiredPauseMods) == requiredPauseMods;
    if (down && !repeat && KeyBundleVk(pauseKey) == vkCode && requiredPauseModsHeld) {
        _heldPauseVk = vkCode;
        PostInput({.handler = &TogglePause, .vkCode = vkCode, .survivesFocusSwitch = true});
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
    if (!mode) return false;
    const KeyHandler_t handler = down ? mode->onPress : mode->onRelease;
    if (!handler) return false;
    if (!repeat) PostInput({.handler = handler, .vkCode = vkCode});
    return true;
}

void App::ForgetHeldKeys()
{
    _heldPauseVk = 0;
    _injectedF4Held = false;
}

void App::PostInput(InputEvent ev)
{
    {
        std::scoped_lock lock(_inputMutex);
        _inputQueue.push_back(ev);
    }
    SetEvent(_inputWake);
}

template <class OnTickingKey>
static void ForEachTickingKey(const Profile& profile, OnTickingKey&& onTickingKey)
{
    const unsigned mods = sKeyboard.TestModifiers();
    for (unsigned short vk = 1; vk < kKeyboardKeysCount; vk++) {
        if (!sKeyboard.IsPressed(vk)) continue;
        const bool tickWouldSendAltF4 = vk == VK_F4 && (mods & KeyMod_Alt);
        if (tickWouldSendAltF4 && profile.disableAltF4) continue;
        const KeyMode* mode = FindKeyMode(ResolveKeyAction(profile, vk, mods));
        if (mode && mode->onTick) onTickingKey(vk, mode->onTick);
    }
}

static bool AnyKeyTicks(const Profile& profile)
{
    bool any = false;
    ForEachTickingKey(profile, [&any](unsigned short, KeyHandler_t) { any = true; });
    return any;
}

static void TickAutofire(const Profile& profile)
{
    ForEachTickingKey(profile, [](unsigned short vk, KeyHandler_t onTick) { onTick(vk); });
}

void App::StartInputWorker()
{
    _inputWake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    _inputThread = std::jthread([this](const std::stop_token& stop) { InputWorkerProc(stop); });
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

void App::InputWorkerProc(const std::stop_token& stop)
{
    using Clock = std::chrono::steady_clock;
    using Ms = std::chrono::milliseconds;
    using Ticks100ns = std::chrono::duration<LONGLONG, std::ratio<1, 10'000'000>>;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    // the high resolution timer needs Win10 1803+; an ordinary one is only as fine as the system timer period
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    const bool needsFineSystemTimer = !timer;
    if (needsFineSystemTimer) {
        timeBeginPeriod(1);
        timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }
    HANDLE waitables[] = {_inputWake, timer};
    const DWORD waitCount = timer ? 2 : 1;

    std::vector<InputEvent> batch;
    Clock::time_point lastTickDue;
    bool ticking = false;
    while (!stop.stop_requested()) {
        const std::shared_ptr<Profile> profile = ActiveTarget().first;
        const bool keysTick = profile && sConfig.enabled && AnyKeyTicks(*profile);
        const Ms period(keysTick ? profile->intervalMs.load() : 0u);

        DWORD timeout = INFINITE;
        if (keysTick) {
            const Clock::time_point now = Clock::now();
            const bool firstTickAPeriodFromNow = !std::exchange(ticking, true);
            if (firstTickAPeriodFromNow) lastTickDue = now;
            Clock::time_point due = lastTickDue + period;
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
        } else if (std::exchange(ticking, false) && timer) {
            CancelWaitableTimer(timer);
        }
        WaitForMultipleObjects(waitCount, waitables, FALSE, timeout);
        if (stop.stop_requested()) break;

        {
            std::scoped_lock lock(_inputMutex);
            batch.swap(_inputQueue);
        }
        for (const InputEvent& ev : batch)
            ev.handler(ev.vkCode);
        batch.clear();

        const bool stillActive = ActiveTarget().first == profile;
        if (keysTick && stillActive && Clock::now() - lastTickDue >= period) {
            if (sConfig.enabled) TickAutofire(*profile);
            lastTickDue += period;
        }
    }

    if (timer) CloseHandle(timer);
    if (needsFineSystemTimer) timeEndPeriod(1);
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
    const std::shared_ptr<Profile> profile = _lastActiveProfile.lock();
    const bool deletedButStillAlive = profile && !std::ranges::contains(sConfig.profiles, profile);
    return deletedButStillAlive ? nullptr : profile;
}

// the secure desktop (lock screen, UAC prompt) refuses its handle to an ordinary process
static bool IsSecureDesktopUp()
{
    HDESK inputDesktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!inputDesktop) return true;
    CloseDesktop(inputDesktop);
    return false;
}

// GetForegroundWindow is NULL until the thread that owns the new foreground window has activated it
void App::OnForegroundChange(HWND shownOrActivated)
{
    HWND foreground = GetForegroundWindow();
    SetActiveTarget(foreground ? foreground : shownOrActivated);
}

void App::OnDesktopSwitch()
{
    // the hooks see no input on the secure desktop: a key released there would stay pressed for them
    if (_activeHwnd) _targetLeftAtDesktopSwitch = _activeHwnd;
    SetActiveTarget(nullptr);
    if (IsSecureDesktopUp()) return;
    HWND resumed = std::exchange(_targetLeftAtDesktopSwitch, nullptr);
    HWND foreground = GetForegroundWindow();
    SetActiveTarget(foreground ? foreground : resumed);
}

void App::SetActiveTarget(HWND hwnd)
{
    if (hwnd == _activeHwnd) return;

    std::shared_ptr<Profile> profile;
    if (const std::filesystem::path path = hwnd ? GetProcessPath(hwnd) : std::filesystem::path(); path.has_filename())
        profile = sConfig.FindProfileByApp(Utf8FileName(path));
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

    const bool keyEditorLogsPresses = hwnd == _mainWindow->Native();
    if (!profile && !keyEditorLogsPresses)
        sKeyboard.Detach();
    else if (!sKeyboard.Attach(keyEditorLogsPresses || profile->UsesMouse()))
        _mainWindow->NotifyKeyboardHookFailed();

    SetEvent(_inputWake);
    SetEvent(_uiStale.get());
}

int main(int argc, char** argv) // NOLINT(bugprone-exception-escape): running out of memory or threads ends the process
{
    // no Uninit after a failed Init: its Save would overwrite a config we refused to reset or another instance owns
    if (!sApp.Init(argc, argv)) return EXIT_FAILURE;
    sApp.Run();
    sApp.Uninit();
    return EXIT_SUCCESS;
}
