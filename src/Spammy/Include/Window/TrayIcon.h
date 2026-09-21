#pragma once
#include "Headers.h"
#ifndef WM_TRAYCMD
#define WM_TRAYCMD (WM_APP + 0x10)
#endif

class TrayIconMenu {
    friend class TrayIcon;

public:
    using Callback_t = std::function<void()>;
    static constexpr size_t MaxItems = 16;

private:
    HWND _hwnd = NULL;
    HMENU _menu = NULL;
    // menu item id == index; ids are handed to WM_COMMAND by the shell
    boost::container::static_vector<Callback_t, MaxItems> _items;

public:
    TrayIconMenu() = default;
    ~TrayIconMenu();

    void Button(const wchar_t* text, Callback_t&& cb);
    void Toggle(const wchar_t* text, bool state, Callback_t&& cb);
    void Disabled(const wchar_t* text);

private:
    void Append(UINT flags, const wchar_t* text, Callback_t&& cb = {});
    void Fire(UINT id);
    bool Create(HWND hwnd);
    void Track(int x, int y);
    void Cleanup();
};

class Window;
class TrayIcon {
    friend Window;

public:
    using MenuCallback_t = std::function<void(TrayIconMenu&)>;
    using ClickCallback_t = std::function<void()>;

private:
    static UINT s_idCounter;
    NOTIFYICONDATAW _data;
    ClickCallback_t _clickFunc;
    MenuCallback_t _menuFunc;
    TrayIconMenu _menu;

public:
    TrayIcon();
    ~TrayIcon();
    void SetTip(const wchar_t* tip);

    void SetOnClick(ClickCallback_t&& func);
    void SetMenu(MenuCallback_t&& func);
    void ShowMenu(int x, int y);

private:
    bool IsCreated() const { return _data.hWnd != NULL; }
    void UpdateIcon(HICON icon);
    bool HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    bool Create(HWND hwnd, HICON icon);
    void Cleanup();
    bool Notify(DWORD message);
    void Reset();
};
