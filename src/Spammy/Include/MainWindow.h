#pragma once
#include "Config.h"
#include "ImGui.h"
#include "Profile.h"
#include "Utils.h"
#include "Window/Window.h"

// The window body is split over three files:
//   MainWindow.cpp         - frame dispatch, hooks, title bar with game tabs, the editor screen (strip, keyboard,
//                            mouse, bottom rail)
//   MainWindowPopups.cpp   - key menu, add-app popup, settings, game tab menu, close dialog
//   MainWindowAddGame.cpp  - the "add a game" screen and its first-run variant
class MainWindow : public Window {
public:
    static constexpr ImVec2 kWindowSize = {1280.f, 680.f};

    // popup ids; open them only through RequestPopup so OpenPopup runs at the root of the id stack
    static constexpr const char* kPopupKeyMenu = "##keymenu";
    static constexpr const char* kPopupApps = "##apps";
    static constexpr const char* kPopupSettings = "##settings";
    static constexpr const char* kPopupTabMenu = "##tabmenu";
    static constexpr const char* kPopupClose = "##close";

private:
    // one ring buffer of press timestamps per VK, sized for a full second at the fastest autofire rate
    using PressLog = std::array<DWORD, 128>;
    struct KeyFrame;

    struct PhysicalPress {
        unsigned short vkCode;
        DWORD ticks;
    };

    enum Screen {
        Screen_Editor,
        Screen_AddGame, // with no games at all the add screen is shown regardless, in its first-run variant
    };

    // the add-a-game screen, reset every time it opens
    struct AddGameState {
        std::vector<RunningApp> running; // refreshed when the screen opens and then every 2 s
        DWORD refreshTicks = 0;
        std::optional<RunningApp> browsed; // picked through "Browse for .exe", listed first
        std::string selectedExe;          // empty: nothing picked yet
        std::string selectedName;
        bool copyCurrent = false; // "Start from: Copy of <game>"
    };

    HICON _pausedIcon = nullptr; // grayscale copy of the app icon, shown in the tray while disabled
    // the hook thread only publishes into these two and the queue below; the UI thread applies them each frame
    std::atomic<bool> _capturingPauseKey = false; // armed by the UI, disarmed by the hook once a key is released
    std::atomic<unsigned> _capturedPauseKey = 0;  // key bundle of that release, 0 = nothing pending
    SpscQueue<PhysicalPress, 64> _physicalPresses; // presses made while our window is focused
    bool _askClose = false; // close requested while the user still has to pick hide vs exit

    // UI thread only
    Screen _screen = Screen_Editor;
    AddGameState _addGame;
    unsigned _setupPauseKey = 0; // first run: pause key picked before any game exists, handed to the first game
    unsigned _editMods = KeyMod_None; // the MODIFIERS switch: KeyMod_None, KeyMod_Shift, KeyMod_Ctrl or KeyMod_Alt
    const char* _pendingPopup = nullptr;
    unsigned short _menuVk = 0;       // key the key menu edits
    ImVec2 _menuKeyMin, _menuKeyMax;  // that key's rectangle, the menu is placed next to it
    bool _menuShowCombos = false;     // "+ 4 more combos" expanded
    ImVec2 _appsPopupPos;             // top-left of the add-app popup: under the "+ APP" button
    std::shared_ptr<Profile> _tabMenuProfile; // game whose tab was right-clicked
    ImVec2 _tabMenuPos;
    std::array<PressLog, kKeyboardKeysCount> _pressLog = {};
    std::array<unsigned, kKeyboardKeysCount> _pressHead = {};
    std::array<DWORD, kKeyboardKeysCount> _lastPressTick = {};
    std::array<DWORD, kKeyboardKeysCount> _simulatedTick = {};

public:
    using Window::Window;
    ~MainWindow() override;
    bool Initialize();
    // close button / Alt+F4 / WM_CLOSE: hides, exits or asks, depending on the remembered choice
    void RequestClose();
    // swaps the tray icon for its grayscale twin while spamming is paused; cheap enough to call every loop
    void SyncTrayIcon();

    // hook-thread callbacks; focused = our own window is the foreground one
    bool HandleKeyPress(unsigned short vkCode, bool repeat, bool focused);
    bool HandleKeyRelease(unsigned short vkCode);

protected:
    void OnTrayClick();
    void OnTrayMenu(TrayIconMenu& menu);

    void Draw() final;
    bool HandleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result) final;

private:
    void ApplyHookEvents();
    void LogKeyPress(unsigned short vkCode, DWORD ticks);
    unsigned PressRate(unsigned short vkCode, DWORD nowTicks) const;
    void TickSimulatedPresses(unsigned short vkCode, DWORD nowTicks, unsigned speed, bool firing);

    void RequestPopup(const char* id) { _pendingPopup = id; }
    // arms pause key capture (click again to cancel); the captured key lands in the editing profile, or in
    // _setupPauseKey while there is no game yet
    void TogglePauseKeyCapture();
    bool IsCapturingPauseKey() const { return _capturingPauseKey; }
    void OpenAddGame();
    void CloseAddGame();

    // MainWindow.cpp
    void DrawTitleBar(ImDrawList* dl, const ImVec2& o);
    void DrawEditor(ImDrawList* dl, const ImVec2& o, const std::shared_ptr<Profile>& profile);
    void DrawStrip(ImDrawList* dl, const ImVec2& o, Profile& profile);
    void DrawKeyboard(ImDrawList* dl, const ImVec2& o, Profile& profile);
    void DrawKey(KeyFrame& frame, const char* id, const char* label, UINT vkCode, const ImVec2& pos,
                 const ImVec2& size, const float* radii = nullptr);
    void DrawMouse(ImDrawList* dl, KeyFrame& frame, const ImVec2& origin, bool sideButtons);
    void DrawRail(ImDrawList* dl, const ImVec2& o, Profile& profile);

    // MainWindowPopups.cpp
    void DrawKeyMenu(const ImVec2& o, const std::shared_ptr<Profile>& profile);
    void DrawAppsPopup(const ImVec2& o, const std::shared_ptr<Profile>& profile);
    void DrawSettingsPopup(const ImVec2& o);
    void DrawTabMenu(const ImVec2& o);
    void DrawClosePopup(const ImVec2& o);

    // MainWindowAddGame.cpp
    void DrawAddGame(ImDrawList* dl, const ImVec2& o, bool firstRun);
};

// shared by the three MainWindow*.cpp files
namespace MainWindowUtil {
// assigns a persisted setting, flagging the config for saving only on an actual change
template <class T>
bool SetSetting(T& field, std::type_identity_t<T> value);
// the pause key as people read it ("NumDel", "Mouse 4", "Ctrl+F"), or NULL when unset
const char* PauseKeyName(unsigned keyBundle, char* buf, size_t bufSize);
// "no modifier", "Shift", "Ctrl+Alt"... for a KeyMod set
const char* ModsName(unsigned mods);
bool MatchesFilter(std::string_view name, std::string_view filter);
} // namespace MainWindowUtil

template <class T>
bool MainWindowUtil::SetSetting(T& field, std::type_identity_t<T> value)
{
    if (field == value) return false;
    field = value;
    sConfig.MarkDirty();
    return true;
}
