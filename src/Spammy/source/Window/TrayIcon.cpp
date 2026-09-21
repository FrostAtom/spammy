#include "Window/TrayIcon.h"

TrayIconMenu::~TrayIconMenu()
{
    if (_hwnd) Cleanup();
}

bool TrayIconMenu::Create(HWND hwnd)
{
    _hwnd = hwnd;
    _menu = CreatePopupMenu();
    return _menu != NULL;
}

void TrayIconMenu::Cleanup()
{
    DestroyMenu(_menu);
    _menu = NULL;
    _hwnd = NULL;
    _items.clear();
}

void TrayIconMenu::Track(int x, int y)
{
    // without a foreground window the popup won't close when the user clicks elsewhere
    SetForegroundWindow(_hwnd);
    UINT flags = (GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN) | TPM_BOTTOMALIGN;
    TrackPopupMenuEx(_menu, flags, x, y, _hwnd, NULL);
}

void TrayIconMenu::Append(UINT flags, const wchar_t* text, Callback_t&& cb)
{
    if (_items.size() >= MaxItems) return;
    AppendMenuW(_menu, flags, _items.size(), text);
    _items.push_back(std::move(cb));
}

void TrayIconMenu::Fire(UINT id)
{
    if (id < _items.size() && _items[id]) _items[id]();
}

void TrayIconMenu::Button(const wchar_t* text, Callback_t&& cb)
{
    Append(MF_STRING, text, std::move(cb));
}

void TrayIconMenu::Toggle(const wchar_t* text, bool state, Callback_t&& cb)
{
    Append(MF_STRING | (state ? MF_CHECKED : 0), text, std::move(cb));
}

void TrayIconMenu::Disabled(const wchar_t* text)
{
    Append(MF_STRING | MF_GRAYED, text);
}

UINT TrayIcon::s_idCounter = 0;

TrayIcon::TrayIcon()
{
    Reset();
}

TrayIcon::~TrayIcon()
{
    Cleanup();
}

void TrayIcon::SetTip(const wchar_t* tip)
{
    wcsncpy_s(_data.szTip, tip, _TRUNCATE);
    _data.uFlags |= NIF_TIP | NIF_SHOWTIP;
    if (IsCreated()) Notify(NIM_MODIFY);
}

void TrayIcon::SetOnClick(ClickCallback_t&& func)
{
    _clickFunc = std::move(func);
}

void TrayIcon::SetMenu(MenuCallback_t&& func)
{
    _menuFunc = std::move(func);
}

void TrayIcon::ShowMenu(int x, int y)
{
    if (!_menuFunc || !_menu.Create(_data.hWnd)) return;
    _menuFunc(_menu);
    _menu.Track(x, y);
    _menu.Cleanup();
}

void TrayIcon::UpdateIcon(HICON icon)
{
    if (!IsCreated()) return;
    _data.hIcon = icon;
    if (icon)
        _data.uFlags |= NIF_ICON;
    else
        _data.uFlags &= ~NIF_ICON;
    Notify(NIM_MODIFY);
}

bool TrayIcon::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_TRAYCMD:
        // NOTIFYICON_VERSION_4: HIWORD(lParam) = icon id, LOWORD(lParam) = event, wParam = cursor position
        if (HIWORD(lParam) != _data.uID) return false;
        switch (LOWORD(lParam)) {
        case NIN_SELECT:
            if (_clickFunc) _clickFunc();
            return true;
        case WM_CONTEXTMENU: ShowMenu(GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)); return true;
        }
        return false;
    case WM_COMMAND:
        if (HIWORD(wParam) != 0) return false;
        _menu.Fire(LOWORD(wParam));
        return true;
    }
    return false;
}

bool TrayIcon::Create(HWND hwnd, HICON icon)
{
    if (IsCreated()) return false;
    _data.hWnd = hwnd;
    _data.hIcon = icon;
    _data.uFlags |= NIF_ICON;
    if (!Notify(NIM_ADD)) {
        _data.hWnd = NULL;
        return false;
    }
    if (!Notify(NIM_SETVERSION)) {
        Notify(NIM_DELETE);
        _data.hWnd = NULL;
        return false;
    }
    return true;
}

bool TrayIcon::Notify(DWORD message)
{
    return Shell_NotifyIconW(message, &_data);
}

void TrayIcon::Cleanup()
{
    if (!IsCreated()) return;
    Notify(NIM_DELETE);
    Reset();
}

void TrayIcon::Reset()
{
    _data = {};
    _data.cbSize = sizeof(_data);
    _data.uFlags = NIF_MESSAGE;
    _data.uVersion = NOTIFYICON_VERSION_4;
    _data.uCallbackMessage = WM_TRAYCMD;
    _data.uID = ++s_idCounter;
}
