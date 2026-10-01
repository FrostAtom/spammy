#pragma once
#include "Headers.h"

// popup menu built from scratch every time the tray icon is right-clicked
class TrayIconMenu {
    friend class TrayIcon;

public:
    using Callback_t = std::function<void()>;
    static constexpr size_t MaxItems = 16;

private:
    HWND _hwnd;
    HMENU _menu;
    // menu item id == index + 1, returned by TrackPopupMenuEx
    boost::container::static_vector<Callback_t, MaxItems> _items;

public:
    TrayIconMenu(const TrayIconMenu&) = delete;
    TrayIconMenu& operator=(const TrayIconMenu&) = delete;
    ~TrayIconMenu();

    void Button(const wchar_t* text, Callback_t&& cb);
    void Toggle(const wchar_t* text, bool state, Callback_t&& cb);
    void Disabled(const wchar_t* text);

private:
    explicit TrayIconMenu(HWND hwnd);
    void Append(UINT flags, const wchar_t* text, Callback_t&& cb = {});
    void Track(int x, int y);
};

class Window;
class TrayIcon {
    friend Window;

public:
    using MenuCallback_t = std::function<void(TrayIconMenu&)>;
    using ClickCallback_t = std::function<void()>;

private:
    inline static UINT s_idCounter = 0;
    NOTIFYICONDATAW _data;
    ClickCallback_t _clickFunc;
    MenuCallback_t _menuFunc;

public:
    TrayIcon();
    ~TrayIcon();
    void SetTip(const wchar_t* tip);

    void SetOnClick(ClickCallback_t&& func);
    void SetMenu(MenuCallback_t&& func);

private:
    bool IsCreated() const { return _data.hWnd != nullptr; }
    bool Create(HWND hwnd, HICON icon);
    void Cleanup();
    void Reset();
    bool Notify(DWORD message);
    void UpdateIcon(HICON icon);
    bool HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void ShowMenu(int x, int y);
};
