#pragma once
#include "Window/Window.h"
#include "resources/resources.h"
#include "ImGui.h"
#include "KeyboardLayout.h"
#include "Utils.h"
#include "Profile.h"
#define WM_USER_FOCUS (WM_APP + 0x20)
#define sMainWindow MainWindow::instance()

class MainWindow : public Window {
    static MainWindow* _self;
    std::filesystem::path _appFilePath;
    Action _selectedAction;
    bool _editPause;

public:
    MainWindow(const wchar_t* className, const wchar_t* wndName = NULL);
    ~MainWindow();
    static MainWindow& instance();
    bool initialize();

    bool handleKeyPress(unsigned short vkCode, bool repeat);
    bool handleKeyRelease(unsigned short vkCode, bool repeat);
protected:
    void onTrayClick();
    void onTrayMenu(TrayIconMenu& menu);

    void loadStyle();
    void draw() final;
    bool handleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result) final;
};