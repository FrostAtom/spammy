#include "Window/TrayIcon.h"

TrayIconMenu::TrayIconMenu() : _hwnd(NULL), _menu(NULL), _count(0) {}
TrayIconMenu::~TrayIconMenu()
{
    if (_hwnd) Cleanup();
}

bool TrayIconMenu::Create(HWND hwnd)
{
    _hwnd = hwnd;
    return (_menu = CreatePopupMenu());
}

void TrayIconMenu::Cleanup()
{
    DestroyMenu(_menu);
    _menu = NULL;
    _hwnd = NULL;
    _count = 0;
}

void TrayIconMenu::Track(int x, int y)
{
    SetForegroundWindow(_hwnd);
    UINT uFlags = (GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN) | TPM_BOTTOMALIGN;
    TrackPopupMenuEx(_menu, uFlags, x, y, _hwnd, NULL);
}

int TrayIconMenu::Append(UINT flags, LPCWSTR newItem)
{
    UINT id = _count++;
    if (id >= std::size(_funcs)) return -1;
    AppendMenuW(_menu, flags, id, newItem);
    return id;
}

void TrayIconMenu::Fire(UINT id)
{
    if (id > std::size(_funcs)) return;
    Callback_t& func = _funcs[id];
    if (func) func();
}

void TrayIconMenu::Button(const wchar_t* text, Callback_t&& cb)
{
    int idx = Append(MF_STRING, text);
    if (idx >= 0) _funcs[idx] = std::forward<Callback_t>(cb);
}

void TrayIconMenu::Toggle(const wchar_t* text, bool state, Callback_t&& cb)
{
    UINT flags = MF_STRING;
    if (state) flags |= MF_CHECKED;
    int idx = Append(flags, text);
    if (idx >= 0) _funcs[idx] = std::forward<Callback_t>(cb);
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
    if (_data.uID) Cleanup();
}

void TrayIcon::SetTip(const wchar_t* tip)
{
    wcsncpy(_data.szTip, tip, std::size(_data.szTip));
    _data.uFlags |= (NIF_TIP | NIF_SHOWTIP);
    if (_data.uID) Update(NIM_MODIFY);
}

void TrayIcon::SetOnClick(ClickCallback_t&& func)
{
    _clickFunc = std::forward<ClickCallback_t>(func);
}

void TrayIcon::SetMenu(MenuCallback_t&& func)
{
    _menuFunc = std::forward<MenuCallback_t>(func);
}

void TrayIcon::ShowMenu(int x, int y)
{
    if (!_menuFunc) return;
    if (!_menu.Create(_data.hWnd)) return;
    _menuFunc(_menu);
    _menu.Track(x, y);
    _menu.Cleanup();
}

void TrayIcon::UpdateIcon(HICON icon)
{
    if (!_data.uID) return;
    _data.hIcon = icon;
    if (icon)
        _data.uFlags |= NIF_ICON;
    else
        _data.uFlags &= ~NIF_ICON;
    Update(NIM_MODIFY);
}

bool TrayIcon::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_TRAYCMD: {
        WORD hi = HIWORD(lParam), lo = LOWORD(lParam);
        if (hi != _data.uID) return false;
        switch (lo) {
        case NIN_SELECT: {
            if (_clickFunc) _clickFunc();
            return true;
        }
        case WM_CONTEXTMENU: {
            ShowMenu(GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
            return true;
        }
        }
        break;
    }
    case WM_COMMAND: {
        if (HIWORD(wParam) == 0) {
            _menu.Fire(LOWORD(wParam));
            return true;
        }
        break;
    }
    }
    return false;
}

bool TrayIcon::Create(HWND hwnd, HICON icon)
{
    if (_data.hWnd) return false;
    _data.hWnd = hwnd;
    _data.hIcon = icon;
    _data.uFlags |= NIF_ICON;
    bool added = Update(NIM_ADD);
    if (!added || !Update(NIM_SETVERSION)) {
        if (added) Update(NIM_DELETE);
        return false;
    }
    return true;
}

bool TrayIcon::Update(DWORD dwMessage)
{
    return Shell_NotifyIconW(dwMessage, &_data);
}

void TrayIcon::Cleanup()
{
    if (!_data.uID) return;
    Update(NIM_DELETE);
    Reset();
}

void TrayIcon::Reset()
{
    memset(&_data, NULL, sizeof(_data));
    _data.cbSize = sizeof(_data);
    _data.uFlags = NIF_MESSAGE;
    _data.uVersion = NOTIFYICON_VERSION_4;
    _data.uCallbackMessage = WM_TRAYCMD;
    _data.uID = ++s_idCounter;
}
