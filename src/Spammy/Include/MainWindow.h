#pragma once
#include "ImGui.h"
#include "Profile.h"
#include "Utils.h"
#include "Window/Window.h"

class MainWindow : public Window {
    // one ring buffer of press timestamps per VK, sized for a full second at the fastest autofire rate
    using PressLog = std::array<DWORD, 128>;
    struct KeyFrame;

    struct PhysicalPress {
        unsigned short vkCode;
        DWORD ticks;
    };

    // mouse gesture carried across frames: paint on drag / click, erase with right button
    struct BrushGesture {
        unsigned pressVk = 0;
        bool pressInPanel = false;
        bool rightInPanel = false;
        bool moved = false;
        bool leftDismiss = false; // the click that closes a popup must not paint
    };

    HICON _pausedIcon = nullptr; // grayscale copy of the app icon, shown in the tray while disabled
    // the hook thread only publishes into these two and the queue below; the UI thread applies them each frame
    std::atomic<bool> _capturingPauseKey = false; // armed by the UI, disarmed by the hook once a key is released
    std::atomic<unsigned> _capturedPauseKey = 0; // key bundle of that release, 0 = nothing pending
    SpscQueue<PhysicalPress, 64> _physicalPresses; // presses made while our window is focused
    Action _brushAction = Action_Spammy;
    unsigned _editMods = 0;
    bool _askClose = false; // close requested while the user still has to pick hide vs exit
    BrushGesture _gesture;
    // UI thread only
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

    void DrawTitleBar(ImDrawList* dl, const ImVec2& o);
    void DrawHeader(ImDrawList* dl, const ImVec2& o, const std::shared_ptr<Profile>& profile);
    void DrawPauseKeyChip(ImDrawList* dl, const ImVec2& pos, Profile& profile);
    void DrawKeyboard(ImDrawList* dl, const ImVec2& o, const std::shared_ptr<Profile>& profile);
    void UpdateBrushGesture(KeyFrame& frame, const ImVec2& panelMin, const ImVec2& panelMax);
    void DrawKey(KeyFrame& frame, const char* id, const char* label, UINT vkCode, const ImVec2& pos,
                 const ImVec2& size);
    void DrawMouse(ImDrawList* dl, KeyFrame& frame, const ImVec2& body, bool sideButtons);
    void DrawBrushBar(ImDrawList* dl, const ImVec2& panelMin, const ImVec2& panelMax, ImU32 brushColor,
                      Profile* profile);
    void DrawProfilesPopup(const ImVec2& o);
    void DrawAppsPopup(const ImVec2& o, const std::shared_ptr<Profile>& profile);
    void DrawSettingsPopup(const ImVec2& o);
    void DrawClosePopup(const ImVec2& o);
};
