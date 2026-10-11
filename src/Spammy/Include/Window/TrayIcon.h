#pragma once
#include "Headers.h"

class TrayIconMenu {
    friend class TrayIcon;

public:
    using Callback_t = std::function<void()>;
    static constexpr size_t MaxItems = 16;

private:
    HWND _hwnd;
    HMENU _menu;
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

class TrayIcon {
    friend class Window;

public:
    using MenuCallback_t = std::function<void(TrayIconMenu&)>;

private:
    inline static UINT s_idCounter = 0;
    NOTIFYICONDATAW _data;
    std::function<void()> _clickFunc;
    MenuCallback_t _menuFunc;

public:
    TrayIcon();
    ~TrayIcon();
    void SetTip(const wchar_t* tip);

    void SetOnClick(std::function<void()>&& func);
    void SetMenu(MenuCallback_t&& func);

private:
    bool IsAttached() const { return _data.hWnd != nullptr; }
    void Attach(HWND hwnd, HICON icon);
    void TryAddToTray();
    static UINT TaskbarCreatedMsg();
    bool Notify(DWORD message);
    void UpdateIcon(HICON icon);
    bool HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void ShowMenu(int x, int y);
};
