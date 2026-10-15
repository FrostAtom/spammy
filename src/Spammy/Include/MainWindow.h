#pragma once
#include "Config.h"
#include "ImGui.h"
#include "Profile.h"
#include "Utils.h"
#include "Window/Window.h"

class MainWindow : public Window {
    static constexpr ImVec2 kBaseWindowSize = {1280.f, 680.f};

    static constexpr const char* kPopupKeyMenu = "##keymenu";
    static constexpr const char* kPopupApps = "##apps";
    static constexpr const char* kPopupSettings = "##settings";
    static constexpr const char* kPopupTabMenu = "##tabmenu";
    static constexpr const char* kPopupClose = "##close";

    static constexpr DWORD kRateWindowMs = 1000;
    using PressLog = std::array<DWORD, 128>;
    static_assert(std::tuple_size_v<PressLog> >= kProfileRateMax * kRateWindowMs / 1000);
    struct KeyFrame;

    struct PhysicalPress {
        unsigned short vkCode;
        DWORD ticks;
    };

    enum Screen {
        Screen_Editor,
        Screen_AddGame,
    };

    struct TimedNote {
        static constexpr DWORD kShowMs = 4000;
        std::string text;
        DWORD shownTicks = 0;

        void Show(std::string note)
        {
            text = std::move(note);
            shownTicks = GetTickCount();
        }
        const char* Visible(DWORD nowTicks) const
        {
            const bool shown = !text.empty() && nowTicks - shownTicks < kShowMs;
            return shown ? text.c_str() : nullptr;
        }
    };

    struct AddGameState {
        std::vector<RunningApp> runningApps;
        std::optional<DWORD> refreshTicks;
        std::optional<RunningApp> browsed;
        std::string selectedExe;
        TimedNote browseRejection;
    };

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
    Screen _screen = Screen_Editor;
    AddGameState _addGame;
    KeyBundle_t _firstGamePauseKey = 0;
    unsigned _editMods = KeyMod_None;
    const char* _pendingPopup = nullptr;
    bool _themeStale = false;
    unsigned short _keyMenuVk = 0;
    ImVec2 _keyMenuKeyMin, _keyMenuKeyMax;
    bool _keyMenuShowsCombos = false;
    ImVec2 _appsPopupPos;
    TimedNote _appsBrowseRejection;
    std::shared_ptr<Profile> _tabMenuProfile;
    ImVec2 _tabMenuPos;
    std::array<PressLog, kKeyboardKeysCount> _pressLog = {};
    std::array<unsigned, kKeyboardKeysCount> _pressHead = {};
    std::array<DWORD, kKeyboardKeysCount> _lastPressTick = {};
    std::array<DWORD, kKeyboardKeysCount> _autofireReplayTick = {};
    std::map<std::pair<std::wstring, int>, std::optional<ImTextureRef>> _appIcons;

public:
    using Window::Window;
    ~MainWindow() override;
    bool Initialize();
    void RequestClose();
    void SyncTrayIcon();

    // hook thread
    bool HandleKeyPress(unsigned short vkCode, bool repeat, bool ownWindowFocused);
    bool HandleKeyRelease(unsigned short vkCode);

protected:
    void OnTrayClick();
    void OnTrayMenu(TrayIconMenu& menu);

    void Draw() final;
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
    std::optional<std::filesystem::path> BrowseForUnclaimedExe(const Profile* allowedOwner, TimedNote& rejection);

    // MainWindow.cpp
    void DrawTitleBar(ImDrawList* dl, const ImVec2& o);
    void DrawEditor(ImDrawList* dl, const ImVec2& o, Profile& profile);
    void DrawStrip(ImDrawList* dl, const ImVec2& o, Profile& profile);
    void DrawKeyboard(ImDrawList* dl, const ImVec2& o, Profile& profile);
    void DrawKey(KeyFrame& frame, const char* id, const char* label, UINT vkCode, const ImVec2& pos, const ImVec2& size,
                 const float radii[ImGui::UiCorner_COUNT] = nullptr);
    void DrawMouse(ImDrawList* dl, KeyFrame& frame, const ImVec2& origin, bool sideButtons);
    void DrawRail(ImDrawList* dl, const ImVec2& windowPos, Profile& profile);

    // MainWindowPopups.cpp
    void DrawKeyMenu(const ImVec2& o, Profile& profile);
    void DrawAppsPopup(const ImVec2& o, Profile& profile);
    void DrawSettingsPopup(const ImVec2& o);
    void DrawTabMenu(const ImVec2& o);
    void DrawClosePopup(const ImVec2& o);

    // MainWindowAddGame.cpp
    void DrawAddGame(ImDrawList* dl, const ImVec2& o, bool firstRun);
    std::optional<ImTextureRef> AppIcon(const std::filesystem::path& exePath, int sizePx);
};

namespace MainWindowUtil {
template <class T>
bool SetSetting(T& field, std::type_identity_t<T> value);
const char* PauseKeyName(KeyBundle_t pauseKey, char* buf, size_t bufSize);
const char* KeyLabel(unsigned short vkCode, char* buf, size_t bufSize);
const char* BuildLabel();
const char* ModsName(unsigned mods);
ImGui::UiKeyKind KeyKindOf(Action action);
template <class WidthOf>
std::string FitText(std::string text, float maxW, WidthOf&& widthOf);
std::string FitText(ImFont* font, float size, std::string text, float maxW, float tracking = 0.f);
} // namespace MainWindowUtil

template <class T>
bool MainWindowUtil::SetSetting(T& field, std::type_identity_t<T> value)
{
    if (field == value) return false;
    field = value;
    sConfig.MarkDirty();
    return true;
}

template <class WidthOf>
std::string MainWindowUtil::FitText(std::string text, float maxW, WidthOf&& widthOf)
{
    constexpr const char* kEllipsis = "\xE2\x80\xA6";
    if (widthOf(text.c_str()) <= maxW) return text;
    while (!text.empty()) {
        size_t cut = text.size() - 1;
        while (cut && IsUtf8ContinuationByte(text[cut]))
            cut--;
        text.erase(cut);
        while (!text.empty() && text.back() == ' ')
            text.pop_back();
        std::string candidate = text + kEllipsis;
        if (widthOf(candidate.c_str()) <= maxW) return candidate;
    }
    return kEllipsis;
}
