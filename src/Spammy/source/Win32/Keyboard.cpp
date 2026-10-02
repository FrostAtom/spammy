#include "Win32/Keyboard.h"

namespace {

// tags injected input so the hooks can tell our own SendInput from physical keys
constexpr ULONG_PTR kEmulatedExtraInfo = 0x80000000;
// an unassigned VK tapped between Alt's down and up, so the up doesn't open the window menu
constexpr unsigned short kMenuMaskVk = 0xE8;

bool IsEmulated(ULONG_PTR extraInfo) noexcept
{
    return (extraInfo & kEmulatedExtraInfo) != 0;
}

INPUT MakeInput(unsigned short vkCode, bool down)
{
    INPUT in{};
    if (Keyboard::IsMouseButton(vkCode)) {
        in.type = INPUT_MOUSE;
        switch (vkCode) {
        case VK_LBUTTON: in.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP; break;
        case VK_RBUTTON: in.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP; break;
        case VK_MBUTTON: in.mi.dwFlags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP; break;
        case VK_XBUTTON1:
        case VK_XBUTTON2:
            in.mi.dwFlags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
            in.mi.mouseData = vkCode == VK_XBUTTON1 ? XBUTTON1 : XBUTTON2;
            break;
        }
        in.mi.dwExtraInfo = kEmulatedExtraInfo;
        return in;
    }
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vkCode;
    in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    // games read scancodes; arrows/Ins/Del/RCtrl/... are extended (0xE0xx) and become numpad keys without the flag
    if (const UINT scCode = MapVirtualKeyA(vkCode, MAPVK_VK_TO_VSC_EX)) {
        in.ki.wScan = (WORD)(scCode & 0xFF);
        in.ki.dwFlags |= KEYEVENTF_SCANCODE;
        if (scCode & 0xE000) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    in.ki.dwExtraInfo = kEmulatedExtraInfo;
    return in;
}

} // namespace

Keyboard::~Keyboard()
{
    Detach();
}

Keyboard& Keyboard::Instance()
{
    static Keyboard keyboard;
    return keyboard;
}

bool Keyboard::Attach(bool withMouse)
{
    if (_thread.joinable()) {
        if (_withMouse == withMouse) return _keyboardHook != nullptr;
        Detach();
    }
    _withMouse = withMouse;
    std::promise<bool> ready;
    std::future<bool> installed = ready.get_future();
    _thread = std::jthread(
        [this, ready = std::move(ready)](std::stop_token stop) mutable { ThreadProc(stop, std::move(ready)); });
    return installed.get();
}

void Keyboard::Detach()
{
    if (!_thread.joinable()) return;
    _thread.request_stop();
    _thread.join();
    ResetState();
}

void Keyboard::ResetState()
{
    for (auto& pressed : _pressed)
        pressed = false;
    _swallowed.fill(false);
    // _altsToRelease stays: a pending lift must still reach the system, an extra Alt up is harmless
    _altMasked = false;
}

void Keyboard::SyncState()
{
    for (unsigned short vkCode = 1; vkCode < _pressed.size(); ++vkCode) {
        // LL hooks only ever deliver left/right modifier codes, so a generic VK_SHIFT/VK_CONTROL/VK_MENU
        // captured here (e.g. Alt still held during Alt+Tab) would never be released and stick forever
        const bool generic = vkCode == VK_SHIFT || vkCode == VK_CONTROL || vkCode == VK_MENU;
        _pressed[vkCode] = !generic && (GetAsyncKeyState(vkCode) & 0x8000);
    }
}

void Keyboard::ThreadProc(std::stop_token stop, std::promise<bool> ready)
{
    const DWORD threadId = GetCurrentThreadId();
    // the RIT blocks on every hook call; make sure we're never starved by rendering or the input worker
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    // force message-queue creation so Detach's WM_QUIT can never race ahead of it
    MSG msg;
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    _keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, &LowLevelKeyboardProc, nullptr, 0);
    if (!_keyboardHook) {
        ready.set_value(false);
        return;
    }
    if (_withMouse) _mouseHook = SetWindowsHookExW(WH_MOUSE_LL, &LowLevelMouseProc, nullptr, 0);
    SyncState();
    ready.set_value(true);

    // wakes the blocking GetMessage the instant Detach() requests a stop
    std::stop_callback onStop(stop, [threadId]() { PostThreadMessageW(threadId, WM_QUIT, 0, 0); });
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (_mouseHook) {
        UnhookWindowsHookEx(_mouseHook);
        _mouseHook = nullptr;
    }
    UnhookWindowsHookEx(_keyboardHook);
    _keyboardHook = nullptr;
}

unsigned Keyboard::TestModifiers() const noexcept
{
    unsigned result = KeyMod_None;
    if (_pressed[VK_LSHIFT] || _pressed[VK_RSHIFT]) result |= KeyMod_Shift;
    if (_pressed[VK_LMENU] || _pressed[VK_RMENU]) result |= KeyMod_Alt;
    if (_pressed[VK_LCONTROL] || _pressed[VK_RCONTROL]) result |= KeyMod_Ctrl;
    return result;
}

void Keyboard::Press(unsigned short vkCode)
{
    // one SendInput call: down+up land in the input stream back-to-back, nothing can interleave
    INPUT in[2] = {MakeInput(vkCode, true), MakeInput(vkCode, false)};
    SendInput(2, in, sizeof(INPUT));
}

void Keyboard::MaskAlt()
{
    if (_altMasked) return; // already hidden: the system's Alt is (or is about to be) up
    _altMasked = true;
    _altsToRelease |= (_pressed[VK_LMENU] ? 1u : 0u) | (_pressed[VK_RMENU] ? 2u : 0u);
}

void Keyboard::KeyDownWithoutAlt(unsigned short vkCode)
{
    INPUT in[5];
    UINT count = 0;
    if (const unsigned alts = _altsToRelease.exchange(0)) {
        in[count++] = MakeInput(kMenuMaskVk, true);
        in[count++] = MakeInput(kMenuMaskVk, false);
        if (alts & 1) in[count++] = MakeInput(VK_LMENU, false);
        if (alts & 2) in[count++] = MakeInput(VK_RMENU, false);
    }
    in[count++] = MakeInput(vkCode, true);
    SendInput(count, in, sizeof(INPUT));
}

void Keyboard::KeyUp(unsigned short vkCode)
{
    INPUT in = MakeInput(vkCode, false);
    SendInput(1, &in, sizeof(INPUT));
}

const char* Keyboard::GetKeyName(unsigned short vkCode)
{
    switch (vkCode) {
    case VK_LBUTTON: return "Mouse Left";
    case VK_RBUTTON: return "Mouse Right";
    case VK_MBUTTON: return "Mouse Middle";
    case VK_XBUTTON1: return "Mouse 4";
    case VK_XBUTTON2: return "Mouse 5";
    // keyboard layouts carry no names for these
    case VK_SLEEP: return "Sleep";
    case VK_VOLUME_MUTE: return "Mute";
    case VK_VOLUME_DOWN: return "Volume -";
    case VK_VOLUME_UP: return "Volume +";
    case VK_MEDIA_PLAY_PAUSE: return "Play";
    case VK_MEDIA_PREV_TRACK: return "Prev Track";
    case VK_MEDIA_NEXT_TRACK: return "Next Track";
    case VK_MEDIA_STOP: return "Media Stop";
    case VK_LAUNCH_MEDIA_SELECT: return "Media";
    case VK_LAUNCH_MAIL: return "Mail";
    case VK_LAUNCH_APP1: return "App 1";
    case VK_LAUNCH_APP2: return "App 2";
    case VK_BROWSER_BACK: return "Web Back";
    case VK_BROWSER_FORWARD: return "Web Fwd";
    case VK_BROWSER_HOME: return "Web Home";
    case VK_BROWSER_REFRESH: return "Web Reload";
    case VK_BROWSER_STOP: return "Web Stop";
    case VK_BROWSER_SEARCH: return "Web Search";
    case VK_BROWSER_FAVORITES: return "Web Fav";
    }
    static char buf[64];
    buf[0] = '\0';
    // extended keys (arrows, nav, RCtrl, ...) need bit 24 set or they're named as their numpad twins
    const UINT scCode = MapVirtualKeyA(vkCode, MAPVK_VK_TO_VSC_EX);
    LONG lParam = (LONG)((scCode & 0xFF) << 16);
    if (scCode & 0xE000) lParam |= 1 << 24;
    GetKeyNameTextA(lParam, buf, std::size(buf));
    return buf;
}

LRESULT CALLBACK Keyboard::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    Keyboard& self = Instance();
    if (nCode == HC_ACTION) {
        const KBDLLHOOKSTRUCT* data = (const KBDLLHOOKSTRUCT*)lParam;
        const bool down = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
        const bool up = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
        // some OEM/Fn keys report the reserved VK 0xFF, which has no slot in the per-key tables
        if ((down || up) && data->vkCode < kKeyboardKeysCount && !IsEmulated(data->dwExtraInfo) &&
            self.HandleKey((unsigned short)data->vkCode, down))
            return 1;
    }
    return CallNextHookEx(self._keyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK Keyboard::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    Keyboard& self = Instance();
    if (nCode == HC_ACTION) {
        const MSLLHOOKSTRUCT* data = (const MSLLHOOKSTRUCT*)lParam;
        unsigned short vkCode = 0;
        bool down = false;
        switch (wParam) {
        case WM_LBUTTONDOWN: down = true; [[fallthrough]];
        case WM_LBUTTONUP: vkCode = VK_LBUTTON; break;
        case WM_RBUTTONDOWN: down = true; [[fallthrough]];
        case WM_RBUTTONUP: vkCode = VK_RBUTTON; break;
        case WM_MBUTTONDOWN: down = true; [[fallthrough]];
        case WM_MBUTTONUP: vkCode = VK_MBUTTON; break;
        case WM_XBUTTONDOWN: down = true; [[fallthrough]];
        case WM_XBUTTONUP: vkCode = HIWORD(data->mouseData) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2; break;
        }
        if (vkCode && !IsEmulated(data->dwExtraInfo) && self.HandleKey(vkCode, down)) return 1;
    }
    return CallNextHookEx(self._mouseHook, nCode, wParam, lParam);
}

bool Keyboard::HandleKey(unsigned short vkCode, bool down)
{
    // auto-repeat: a down while already down, or an up for a key we never saw go down
    const bool repeat = _pressed[vkCode].exchange(down) == down;
    if (IsModifier(vkCode)) {
        if (!_altMasked || (vkCode != VK_LMENU && vkCode != VK_RMENU)) return false;
        // the system already got (or is getting) Alt's up from KeyDownWithoutAlt, this one included
        if (!_pressed[VK_LMENU] && !_pressed[VK_RMENU]) _altMasked = false;
        return true;
    }
    const Callback_t& callback = down ? _onPress : _onRelease;
    const bool swallow = callback && callback(vkCode, repeat);
    // only swallow what pairs with a down we swallowed: if win32k saw the down (key held before the hook
    // went up, SyncState seeded it), blocking its up leaves the button stuck system-wide — every click
    // anywhere is then routed to the "mouse owner" window until that very button reaches win32k again
    if (down && !repeat) return _swallowed[vkCode] = swallow;
    return swallow && _swallowed[vkCode];
}
