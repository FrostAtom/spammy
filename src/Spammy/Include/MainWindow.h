#pragma once
#include "ImGui.h"
#include "Profile.h"
#include "Window/Window.h"

class MainWindow : public Window {
    // one ring buffer of press timestamps per VK, sized for a full second at the fastest autofire rate
    using PressLog = std::array<DWORD, 128>;

    std::filesystem::path _appFilePath;
    HICON _pausedIcon = NULL; // grayscale copy of the app icon, shown in the tray while disabled
    std::atomic<bool> _editPause = false;
    Action _brushAction = Action_Spammy;
    unsigned _editMods = 0;
    bool _askClose = false; // close requested while the user still has to pick hide vs exit
    std::array<PressLog, kKeyboardKeysCount> _pressLog = {};
    std::array<unsigned, kKeyboardKeysCount> _pressHead = {};
    std::array<DWORD, kKeyboardKeysCount> _pressTick = {};
    std::array<DWORD, kKeyboardKeysCount> _spamTick = {};

public:
    MainWindow(const wchar_t* className, const wchar_t* wndName = NULL);
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
    void LogKeyPress(unsigned short vkCode, DWORD ticks);
    unsigned PressRate(unsigned short vkCode, DWORD nowTicks) const;
    void TickSimulatedPresses(unsigned short vkCode, DWORD nowTicks, unsigned speed, bool firing);

    void DrawTitleBar(ImDrawList* dl, const ImVec2& o);
    void DrawHeader(ImDrawList* dl, const ImVec2& o, const std::shared_ptr<Profile>& profile);
    void DrawPauseKeyChip(ImDrawList* dl, const ImVec2& pos, Profile& profile);
    void DrawKeyboard(ImDrawList* dl, const ImVec2& o, const std::shared_ptr<Profile>& profile);
    void DrawProfilesPopup(const ImVec2& o);
    void DrawAppsPopup(const ImVec2& o, const std::shared_ptr<Profile>& profile);
    void DrawSettingsPopup(const ImVec2& o);
    void DrawClosePopup(const ImVec2& o);
};
