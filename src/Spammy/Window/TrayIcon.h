#pragma once
#include "../Headers.h"
#ifndef WM_TRAYCMD
#define WM_TRAYCMD (WM_APP + 0x10)
#endif

class TrayIconMenu {
    friend class TrayIcon;
public:
    using Callback_t = std::function<void()>;
private:
    HWND _hwnd;
    HMENU _menu;
    UINT _count;
    Callback_t _funcs[16];

public:
    TrayIconMenu();
    ~TrayIconMenu();

    void separator();
    void button(const wchar_t* text, Callback_t&& cb);
    void toggle(const wchar_t* text, bool state, Callback_t&& cb);
    void disabled(const wchar_t* text);

private:
    int append(UINT flags, LPCWSTR newItem);
    void fire(UINT id);
    bool create(HWND hwnd);
    void track(int x, int y);
    void cleanup();
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
    void setTip(const wchar_t* tip);
    const wchar_t* getTip();
    
    void setOnClick(ClickCallback_t&& func);
    void setMenu(MenuCallback_t&& func);
    void showMenu(int x, int y);

private:
    void updateIcon(HICON icon);
    bool handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    bool create(HWND hwnd, HICON icon);
    void cleanup();
    bool update(DWORD dwMessage);
    void reset();
};
