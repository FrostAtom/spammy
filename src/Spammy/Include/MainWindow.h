#pragma once
#include "ImGui.h"
#include "KeyboardLayout.h"
#include "Profile.h"
#include "Utils.h"
#include "Window/Window.h"
#define WM_USER_FOCUS (WM_APP + 0x20)
#define sMainWindow MainWindow::Instance()

class MainWindow : public Window {
    static MainWindow* _self;
    std::filesystem::path _appFilePath;
    bool _editPause;
    std::set<UINT> _selection;
    Action _brushAction = Action_Spammy;
    std::array<std::array<DWORD, 128>, KEYBOARD_KEYS_COUNT> _pressLog = {};
    std::array<unsigned, KEYBOARD_KEYS_COUNT> _pressHead = {};
    std::array<DWORD, KEYBOARD_KEYS_COUNT> _pressTick = {};
    std::array<DWORD, KEYBOARD_KEYS_COUNT> _spamTick = {};

public:
    MainWindow(const wchar_t* className, const wchar_t* wndName = NULL);
    ~MainWindow();
    static MainWindow& Instance();
    bool Initialize();

    bool HandleKeyPress(unsigned short vkCode, bool repeat);
    bool HandleKeyRelease(unsigned short vkCode, bool repeat);

protected:
    void OnTrayClick();
    void OnTrayMenu(TrayIconMenu& menu);

    void LoadStyle();
    bool BeginFrame() final;
    void Draw() final;
    bool HandleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result) final;

private:
    void LogKeyPress(unsigned short vkCode, DWORD ticks);
    void DrawTitleBar(ImDrawList* dl, const ImVec2& o);
    void DrawHeader(ImDrawList* dl, const ImVec2& o, const std::shared_ptr<Profile>& profile);
    void DrawKeyboard(ImDrawList* dl, const ImVec2& o, const std::shared_ptr<Profile>& profile);
    void DrawFooter(ImDrawList* dl, const ImVec2& o);
    void DrawProfilesPopup(const ImVec2& o);
    void DrawAppsPopup(const ImVec2& o, const std::shared_ptr<Profile>& profile);
    void DrawSettingsPopup(const ImVec2& o);
    void DrawKeyMenuPopup(const std::shared_ptr<Profile>& profile, unsigned popupMods);
};
