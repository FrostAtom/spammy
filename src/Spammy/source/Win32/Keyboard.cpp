#include "Win32/Keyboard.h"

namespace {

// tags injected input so the hooks can tell our own SendInput from physical keys
constexpr ULONG_PTR kEmulatedExtraInfo = 0x80000000;

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
    if (UINT scCode = MapVirtualKeyA(vkCode, MAPVK_VK_TO_VSC_EX)) {
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
        if (_withMouse == withMouse) return _hhook != NULL;
        Detach();
    }
    _withMouse = withMouse;
    std::promise<bool> ready;
    std::future<bool> result = ready.get_future();
    _thread = std::jthread([this, &ready](std::stop_token stop) { ThreadProc(stop, ready); });
    return result.get();
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
    for (auto& s : _state)
        s = 0;
}

void Keyboard::SyncState()
{
    const DWORD now = GetTickCount();
    for (unsigned short vkCode = 1; vkCode < _state.size(); ++vkCode) {
        // LL hooks only ever deliver left/right modifier codes, so a generic VK_SHIFT/VK_CONTROL/VK_MENU
        // captured here (e.g. Alt still held during Alt+Tab) would never be released and stick forever
        const bool generic = vkCode == VK_SHIFT || vkCode == VK_CONTROL || vkCode == VK_MENU;
        _state[vkCode] = !generic && (GetAsyncKeyState(vkCode) & 0x8000) ? now : 0;
    }
}

void Keyboard::ThreadProc(std::stop_token stop, std::promise<bool>& ready)
{
    const DWORD threadId = GetCurrentThreadId();
    // the RIT blocks on every hook call; make sure we're never starved by rendering or the input worker
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    // force message-queue creation so Detach's WM_QUIT can never race ahead of it
    MSG msg;
    PeekMessageW(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    _hhook = SetWindowsHookExW(WH_KEYBOARD_LL, &LowLevelKeyboardProc, NULL, 0);
    if (_hhook) {
        if (_withMouse) _mouseHook = SetWindowsHookExW(WH_MOUSE_LL, &LowLevelMouseProc, NULL, 0);
        SyncState();
    }
    ready.set_value(_hhook != NULL);
    if (!_hhook) return;

    // wakes the blocking GetMessage the instant Detach() requests a stop
    std::stop_callback onStop(stop, [threadId]() { PostThreadMessageW(threadId, WM_QUIT, 0, 0); });

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (_mouseHook) {
        UnhookWindowsHookEx(_mouseHook);
        _mouseHook = NULL;
    }
    UnhookWindowsHookEx(_hhook);
    _hhook = NULL;
}

unsigned Keyboard::TestModifiers() const noexcept
{
    unsigned result = KeyMod_None;
    if (_state[VK_LSHIFT] || _state[VK_RSHIFT]) result |= KeyMod_Shift;
    if (_state[VK_LMENU] || _state[VK_RMENU]) result |= KeyMod_Alt;
    if (_state[VK_LCONTROL] || _state[VK_RCONTROL]) result |= KeyMod_Ctrl;
    return result;
}

void Keyboard::Press(unsigned short vkCode)
{
    // one SendInput call: down+up land in the input stream back-to-back, nothing can interleave
    INPUT in[2] = {MakeInput(vkCode, true), MakeInput(vkCode, false)};
    SendInput(2, in, sizeof(INPUT));
}

const char* Keyboard::GetKeyName(unsigned short vkCode)
{
    switch (vkCode) {
    case VK_LBUTTON: return "Mouse Left";
    case VK_RBUTTON: return "Mouse Right";
    case VK_MBUTTON: return "Mouse Middle";
    case VK_XBUTTON1: return "Mouse 4";
    case VK_XBUTTON2: return "Mouse 5";
    }
    static char buf[64];
    buf[0] = '\0';
    GetKeyNameTextA(MapVirtualKeyA(vkCode, MAPVK_VK_TO_VSC) << 16, buf, std::size(buf));
    return buf;
}

LRESULT CALLBACK Keyboard::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    Keyboard& self = Instance();
    if (nCode == HC_ACTION) {
        const KBDLLHOOKSTRUCT* data = (const KBDLLHOOKSTRUCT*)lParam;
        const bool down = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
        const bool up = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
        if ((down || up) && !IsEmulated(data->dwExtraInfo) && self.HandleKey((unsigned short)data->vkCode, down))
            return 1;
    }
    return CallNextHookEx(self._hhook, nCode, wParam, lParam);
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
    const DWORD newState = down ? GetTickCount() : 0;
    if (IsModifier(vkCode)) {
        _state[vkCode] = newState;
        return false;
    }
    // auto-repeat: a down while already down, or (after SyncState) an up we never saw the down for
    const bool repeat = (_state[vkCode] != 0) == down;
    if (!repeat) _state[vkCode] = newState;
    const Callback_t& callback = down ? _onPress : _onRelease;
    return callback && callback(vkCode, repeat);
}
