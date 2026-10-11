#include "Window/TrayIcon.h"

static constexpr UINT WM_TRAYCMD = WM_APP + 0x10;

static constexpr UINT kNothingPicked = 0;
static constexpr UINT kFirstItemId = kNothingPicked + 1;

TrayIconMenu::TrayIconMenu(HWND hwnd) : _hwnd(hwnd), _menu(CreatePopupMenu()) {}

TrayIconMenu::~TrayIconMenu()
{
    if (_menu) DestroyMenu(_menu);
}

void TrayIconMenu::Track(int x, int y)
{
    // KB135788: the menu closes on an outside click only while its owner is foreground,
    // and the posted WM_NULL lets the next one dismiss properly
    SetForegroundWindow(_hwnd);
    const UINT flags = (GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN) | TPM_BOTTOMALIGN |
                       TPM_RETURNCMD | TPM_NONOTIFY;
    const UINT picked = (UINT)TrackPopupMenuEx(_menu, flags, x, y, _hwnd, nullptr);
    PostMessageW(_hwnd, WM_NULL, 0, 0);
    if (picked == kNothingPicked) return;
    const size_t index = picked - kFirstItemId;
    if (index < _items.size() && _items[index]) _items[index]();
}

void TrayIconMenu::Append(UINT flags, const wchar_t* text, Callback_t&& cb)
{
    if (_items.size() >= MaxItems) return;
    AppendMenuW(_menu, flags, kFirstItemId + _items.size(), text);
    _items.push_back(std::move(cb));
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

TrayIcon::TrayIcon() : _data{.cbSize = sizeof(_data)}
{
    _data.uFlags = NIF_MESSAGE;
    _data.uVersion = NOTIFYICON_VERSION_4;
    _data.uCallbackMessage = WM_TRAYCMD;
    _data.uID = ++s_idCounter;
}

TrayIcon::~TrayIcon()
{
    if (IsAttached()) Notify(NIM_DELETE);
}

void TrayIcon::SetTip(const wchar_t* tip)
{
    wcsncpy_s(_data.szTip, tip, _TRUNCATE);
    _data.uFlags |= NIF_TIP | NIF_SHOWTIP;
    if (IsAttached()) Notify(NIM_MODIFY);
}

void TrayIcon::SetOnClick(std::function<void()>&& func)
{
    _clickFunc = std::move(func);
}

void TrayIcon::SetMenu(MenuCallback_t&& func)
{
    _menuFunc = std::move(func);
}

void TrayIcon::ShowMenu(int x, int y)
{
    if (!_menuFunc) return;
    TrayIconMenu menu(_data.hWnd);
    if (!menu._menu) return;
    _menuFunc(menu);
    menu.Track(x, y);
}

void TrayIcon::UpdateIcon(HICON icon)
{
    if (!IsAttached()) return;
    _data.hIcon = icon;
    if (icon)
        _data.uFlags |= NIF_ICON;
    else
        _data.uFlags &= ~NIF_ICON;
    Notify(NIM_MODIFY);
}

UINT TrayIcon::TaskbarCreatedMsg()
{
    static const UINT s_msg = RegisterWindowMessageW(L"TaskbarCreated");
    return s_msg;
}

bool TrayIcon::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    const bool taskbarCreated = msg != WM_NULL && msg == TaskbarCreatedMsg();
    if (taskbarCreated) {
        if (IsAttached()) TryAddToTray();
        return true;
    }
    if (msg != WM_TRAYCMD) return false;
    const UINT iconId = HIWORD(lParam);
    const UINT event = LOWORD(lParam);
    if (iconId != _data.uID) return false;
    switch (event) {
    case NIN_SELECT:
        if (_clickFunc) _clickFunc();
        return true;
    case WM_CONTEXTMENU: {
        const POINT cursor = {GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)};
        ShowMenu(cursor.x, cursor.y);
        return true;
    }
    }
    return false;
}

void TrayIcon::Attach(HWND hwnd, HICON icon)
{
    if (IsAttached()) return;
    _data.hWnd = hwnd;
    _data.hIcon = icon;
    _data.uFlags |= NIF_ICON;
    // UIPI would drop the unelevated shell's broadcast to an elevated instance
    ChangeWindowMessageFilterEx(hwnd, TaskbarCreatedMsg(), MSGFLT_ALLOW, nullptr);
    TryAddToTray();
}

void TrayIcon::TryAddToTray()
{
    if (!Notify(NIM_ADD)) return;
    const bool usesVersion4Callbacks = Notify(NIM_SETVERSION);
    if (!usesVersion4Callbacks) Notify(NIM_DELETE);
}

bool TrayIcon::Notify(DWORD message)
{
    return Shell_NotifyIconW(message, &_data);
}
