#pragma once
#include "Apps.h"
#include "Config.h"
#include "ImGui.h"
#include "Profile.h"
#include "Window/Window.h"
#include <bit>
#include <tuple>

template <class T, unsigned N>
class SpscQueue {
    static_assert(std::has_single_bit(N), "free-running indices must wrap cleanly modulo N");

    std::array<T, N> _items = {};
    std::atomic<unsigned> _head = 0;
    std::atomic<unsigned> _tail = 0;

public:
    bool Push(const T& item)
    {
        const unsigned head = _head.load(std::memory_order_relaxed);
        const bool full = head - _tail.load(std::memory_order_acquire) == N;
        if (full) return false;
        _items[head % N] = item;
        _head.store(head + 1, std::memory_order_release);
        return true;
    }

    void Drain(auto&& consume)
    {
        const unsigned head = _head.load(std::memory_order_acquire);
        unsigned tail = _tail.load(std::memory_order_relaxed);
        for (; tail != head; tail++)
            consume(_items[tail % N]);
        _tail.store(tail, std::memory_order_release);
    }
};

class MainWindow : public Window {
    static constexpr ImVec2 kBaseWindowSize = {1280.f, 680.f};

    static constexpr const char* kPopupKeyMenu = "##keymenu";
    static constexpr const char* kPopupApps = "##apps";
    static constexpr const char* kPopupSettings = "##settings";
    static constexpr const char* kPopupTabMenu = "##tabmenu";
    static constexpr const char* kPopupClose = "##close";
    static constexpr const char* kAutoStartLabel = "Start with Windows";

    static constexpr UINT WM_USER_FOCUS = WM_APP + 0x20;
    // posted to itself: the file dialog runs a message loop of its own, which must not start inside a frame
    static constexpr UINT WM_USER_BROWSE_APP = WM_APP + 0x21;
    static constexpr UINT WM_USER_BROWSE_GAME = WM_APP + 0x22;

    static constexpr DWORD kRateWindowMs = 1000;
    using PressLog = std::array<DWORD, 128>;
    static_assert(std::tuple_size_v<PressLog> >= kProfileRateMax * kRateWindowMs / 1000);
    struct KeyFrame;

    struct PhysicalPress {
        unsigned short vkCode;
        DWORD ticks;
    };

    enum Screen : std::uint8_t {
        Screen_Editor,
        Screen_AddGame,
    };

    struct AddGameState {
        std::vector<RunningApp> runningApps;
        std::optional<DWORD> refreshTicks;
        std::optional<RunningApp> browsed;
        std::string selectedExe;
    };

    using IconsByExePath = boost::unordered_flat_map<std::wstring, std::optional<ImTextureRef>>;

    HICON _pausedIcon = nullptr;
    // shared with the hook thread
    static constexpr KeyBundle_t kPauseCaptureIdle = 0;
    static constexpr KeyBundle_t kPauseCaptureArmed = ~0u;
    // idle, armed, or the captured key until the UI takes it; only armed is ever cancelled, a captured key still lands
    std::atomic<KeyBundle_t> _pauseKeyCapture = kPauseCaptureIdle;
    SpscQueue<PhysicalPress, 64> _physicalPresses;

    // UI thread only
    ImVec2 _keyboardGrowth;
    bool _closePromptRequested = false;
    bool _closeChoiceRemembered = false;
    Screen _screen = Screen_Editor;
    AddGameState _addGame;
    KeyBundle_t _firstGamePauseKey = 0;
    bool _autoStart = false;
    unsigned _editMods = KeyMod_None;
    const char* _pendingPopup = nullptr;
    bool _themeStale = false;
    std::string _notice;
    DWORD _noticeShownTicks = 0;
    bool _leftPressClosedPopup = false;
    float _rateKnobT = 0.f;
    bool _rateKnobHeld = false;
    unsigned short _keyMenuVk = 0;
    ImVec2 _keyMenuKeyMin, _keyMenuKeyMax;
    bool _keyMenuShowsCombos = false;
    ImVec2 _appsPopupPos;
    char _appsSearch[64] = {};
    std::vector<RunningApp> _appsRunning;
    std::shared_ptr<Profile> _tabMenuProfile;
    ImVec2 _tabMenuPos;
    char _tabMenuName[kProfileNameMax + 1] = {};
    std::weak_ptr<Profile> _tabMenuRenameTarget;
    bool _tabMenuRenaming = false;
    bool _tabMenuFocusesName = false;
    bool _tabMenuRemoveArmed = false;
    std::array<PressLog, kKeyboardKeysCount> _pressLog = {};
    std::array<unsigned, kKeyboardKeysCount> _pressHead = {};
    std::array<DWORD, kKeyboardKeysCount> _lastPressTick = {};
    std::array<std::optional<DWORD>, kKeyboardKeysCount> _autofireReplayTick = {};
    boost::unordered_flat_map<int, IconsByExePath> _appIconsBySizePx;

public:
    using Window::Window;
    ~MainWindow() override;
    bool Setup();
    void RequestClose();
    void SyncTrayIcon();
    void NotifyKeyboardHookFailed();

    // hook thread
    bool HandleKeyPress(unsigned short vkCode, bool repeat, bool ownWindowFocused);
    bool HandleKeyRelease(unsigned short vkCode);

protected:
    void OnTrayClick();
    void OnTrayMenu(TrayIconMenu& menu);

    std::optional<float> Draw() final;
    bool HandleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result) final;

private:
    ImVec2 WindowSize() const { return kBaseWindowSize + _keyboardGrowth; }
    void FitWindowToKeyboard();
    void ApplyTheme();
    void ApplyHookEvents();
    void LogKeyPress(unsigned short vkCode, DWORD ticks);
    unsigned PressRate(unsigned short vkCode, DWORD nowTicks) const;
    void ReplayAutofirePresses(unsigned short vkCode, DWORD nowTicks, unsigned intervalMs, bool autofiring);

    // OpenPopup is deferred to Draw, where the id stack is at its root like the BeginPopup calls
    void RequestPopup(const char* id) { _pendingPopup = id; }
    void TogglePauseKeyCapture();
    void CancelPauseKeyCapture()
    {
        KeyBundle_t armed = kPauseCaptureArmed;
        _pauseKeyCapture.compare_exchange_strong(armed, kPauseCaptureIdle);
    }
    bool IsCapturingPauseKey() const { return _pauseKeyCapture == kPauseCaptureArmed; }
    void OpenAddGame();
    void CloseAddGame();
    std::optional<std::filesystem::path> BrowseForUnclaimedExe(const Profile* allowedOwner);
    void Notify(std::string notice);
    void DrawNotice(const ImVec2& o);
    void ToggleAutoStart();

    void DrawTitleBar(ImDrawList* dl, const ImVec2& o);
    void DrawEditor(ImDrawList* dl, const ImVec2& o, Profile& profile);
    void DrawStrip(ImDrawList* dl, const ImVec2& o, Profile& profile);
    void DrawKeyboard(ImDrawList* dl, const ImVec2& o, Profile& profile);
    void DrawKey(KeyFrame& frame, const char* id, const char* label, UINT vkCode, const ImVec2& pos, const ImVec2& size,
                 const float radii[ImGui::UiCorner_COUNT]);
    void DrawMouse(ImDrawList* dl, KeyFrame& frame, const ImVec2& origin, bool sideButtons);
    void DrawRail(ImDrawList* dl, const ImVec2& windowPos, Profile& profile);

    void DrawKeyMenu(const ImVec2& o, Profile& profile);
    void DrawAppsPopup(const ImVec2& o, Profile& profile);
    void DrawSettingsPopup(const ImVec2& o);
    void DrawTabMenu(const ImVec2& o);
    void DrawClosePopup(const ImVec2& o);

    void DrawAddGame(ImDrawList* dl, const ImVec2& o, bool firstRun);
    std::optional<ImTextureRef> AppIcon(const std::filesystem::path& exePath, int sizePx);
};

namespace MainWindowUtil {
template <class T>
bool SetSetting(T& field, std::type_identity_t<T> value);
template <class T>
bool SetSetting(std::atomic<T>& field, std::type_identity_t<T> value);
template <size_t N, class... Args>
const char* FormatInto(char (&buf)[N], fmt::format_string<Args...> format, Args&&... args);
const char* PauseKeyName(KeyBundle_t pauseKey, char* buf, size_t bufSize);
const char* KeyLabel(unsigned short vkCode, Keyboard::KeyNameBuf& buf);
const char* BuildLabel();
const char* ModsName(unsigned mods);
ImGui::UiKeyKind KeyKindOf(Action action);

struct FittedText {
    const char* fitting = nullptr;
    boost::container::small_vector<char, 64> elided;

    [[nodiscard]] const char* c_str() const { return fitting ? fitting : elided.data(); }
};
FittedText FitText(ImFont* font, float size, const char* text, float maxW, float tracking = 0.f);
} // namespace MainWindowUtil

template <class T>
bool MainWindowUtil::SetSetting(T& field, std::type_identity_t<T> value)
{
    if (field == value) return false;
    field = value;
    sConfig.MarkDirty();
    return true;
}

template <class T>
bool MainWindowUtil::SetSetting(std::atomic<T>& field, std::type_identity_t<T> value)
{
    if (field.exchange(value) == value) return false;
    sConfig.MarkDirty();
    return true;
}

template <size_t N, class... Args>
const char* MainWindowUtil::FormatInto(char (&buf)[N], fmt::format_string<Args...> format, Args&&... args)
{
    *fmt::format_to_n(buf, N - 1, format, std::forward<Args>(args)...).out = '\0';
    return buf;
}
