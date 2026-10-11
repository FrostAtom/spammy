#include "Window/TrayIcon.h"

static constexpr UINT WM_TRAYCMD = WM_APP + 0x10;

TrayIconMenu::TrayIconMenu(HWND hwnd) : _hwnd(hwnd), _menu(CreatePopupMenu())
{
}

TrayIconMenu::~TrayIconMenu()
{
    if (_menu) DestroyMenu(_menu);
}

void TrayIconMenu::Track(int x, int y)
{
    // without a foreground window the popup won't close when the user clicks elsewhere
    SetForegroundWindow(_hwnd);
    // TPM_RETURNCMD hands the pick back right here; a posted WM_COMMAND would only arrive after the menu and its
    // callbacks are gone
    const UINT flags = (GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN) | TPM_BOTTOMALIGN |
                       TPM_RETURNCMD | TPM_NONOTIFY;
    const UINT id = (UINT)TrackPopupMenuEx(_menu, flags, x, y, _hwnd, nullptr);
    // forces the task switch the menu needs to dismiss properly next time (KB135788)
    PostMessageW(_hwnd, WM_NULL, 0, 0);
    if (id && id <= _items.size() && _items[id - 1]) _items[id - 1]();
}

void TrayIconMenu::Append(UINT flags, const wchar_t* text, Callback_t&& cb)
{
    if (_items.size() >= MaxItems) return;
    // 0 means "nothing picked" to TPM_RETURNCMD, so ids are index + 1
    AppendMenuW(_menu, flags, _items.size() + 1, text);
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
    if (!_menuFunc) return;
    TrayIconMenu menu(_data.hWnd);
    if (!menu._menu) return;
    _menuFunc(menu);
    menu.Track(x, y);
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

UINT TrayIcon::TaskbarCreatedMsg()
{
    static const UINT s_msg = RegisterWindowMessageW(L"TaskbarCreated");
    return s_msg;
}

bool TrayIcon::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    // a restarted explorer comes up with an empty tray and broadcasts this to every top-level window
    // (also sent on a taskbar DPI change with the icon still there; NIM_ADD then just fails, nothing lost)
    if (msg && msg == TaskbarCreatedMsg()) {
        if (IsCreated()) Add();
        return true;
    }
    // NOTIFYICON_VERSION_4: HIWORD(lParam) = icon id, LOWORD(lParam) = event, wParam = cursor position
    if (msg != WM_TRAYCMD || HIWORD(lParam) != _data.uID) return false;
    switch (LOWORD(lParam)) {
    case NIN_SELECT:
        if (_clickFunc) _clickFunc();
        return true;
    case WM_CONTEXTMENU: ShowMenu(GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)); return true;
    }
    return false;
}

bool TrayIcon::Create(HWND hwnd, HICON icon)
{
    if (IsCreated()) return false;
    _data.hWnd = hwnd;
    _data.hIcon = icon;
    _data.uFlags |= NIF_ICON;
    // the shell runs unelevated: UIPI would drop its broadcast to an elevated instance
    ChangeWindowMessageFilterEx(hwnd, TaskbarCreatedMsg(), MSGFLT_ALLOW, nullptr);
    // hWnd stays set even if this fails (autolaunch can beat the taskbar), TaskbarCreated adds it once the tray is up
    return Add();
}

bool TrayIcon::Add()
{
    if (!Notify(NIM_ADD)) return false;
    if (Notify(NIM_SETVERSION)) return true;
    Notify(NIM_DELETE);
    return false;
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
