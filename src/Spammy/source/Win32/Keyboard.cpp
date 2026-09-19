#include "Win32/Keyboard.h"

Keyboard::Keyboard() : _hhook(NULL), _mouseHook(NULL), _withMouse(false), _threadId(0)
{
    for (auto& s : _state) s = 0;
}

Keyboard::~Keyboard()
{
    Detach();
}

Keyboard& Keyboard::Instance()
{
    static std::unique_ptr<Keyboard> keyboard(new Keyboard());
    return *keyboard;
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
    _threadId = 0;
    for (auto& s : _state) s = 0;
}

void Keyboard::SyncState()
{
    DWORD now = GetTickCount();
    for (unsigned short vkCode = 1; vkCode < _state.size(); ++vkCode) {
        // LL hooks only ever deliver left/right modifier codes, so a generic VK_SHIFT/VK_CONTROL/VK_MENU
        // captured here (e.g. Alt still held during Alt+Tab) would never be released and stick forever
        if (vkCode == VK_SHIFT || vkCode == VK_CONTROL || vkCode == VK_MENU) {
            _state[vkCode] = 0;
            continue;
        }
        _state[vkCode] = (GetAsyncKeyState(vkCode) & 0x8000) ? now : 0;
    }
}

void Keyboard::ThreadProc(std::stop_token stop, std::promise<bool>& ready)
{
    _threadId = GetCurrentThreadId();
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
    std::stop_callback onStop(stop, [id = _threadId]() { PostThreadMessageW(id, WM_QUIT, 0, 0); });

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

bool Keyboard::TestModifiers(unsigned mods)
{
    DWORD _mods = TestModifiers();
    return (_mods & mods) == mods;
}

unsigned Keyboard::TestModifiers()
{
    DWORD result = KeyMod_None;
    if (_state[VK_LSHIFT] || _state[VK_RSHIFT]) result |= KeyMod_Shift;
    if (_state[VK_LMENU] || _state[VK_RMENU]) result |= KeyMod_Alt;
    if (_state[VK_LCONTROL] || _state[VK_RCONTROL]) result |= KeyMod_Ctrl;
    return result;
}

bool Keyboard::IsMouseButton(unsigned short vkCode)
{
    switch (vkCode) {
    case VK_LBUTTON:
    case VK_RBUTTON:
    case VK_MBUTTON:
    case VK_XBUTTON1:
    case VK_XBUTTON2: return true;
    }
    return false;
}

unsigned Keyboard::IsModifier(unsigned short vkCode)
{
    switch (vkCode) {
    case VK_LMENU:
    case VK_RMENU:
    case VK_MENU: return KeyMod_Alt;
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_CONTROL: return KeyMod_Ctrl;
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_SHIFT: return KeyMod_Shift;
    }
    return KeyMod_None;
}

DWORD Keyboard::IsPressed(unsigned short vkCode)
{
    return _state[vkCode];
}

// fills a single INPUT for a key/button transition
static void MakeInput(INPUT& in, unsigned short vkCode, bool down)
{
    memset(&in, 0, sizeof(in));
    if (Keyboard::IsMouseButton(vkCode)) {
        in.type = INPUT_MOUSE;
        switch (vkCode) {
        case VK_LBUTTON: in.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP; break;
        case VK_RBUTTON: in.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP; break;
        case VK_MBUTTON: in.mi.dwFlags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP; break;
        case VK_XBUTTON1:
            in.mi.dwFlags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
            in.mi.mouseData = XBUTTON1;
            break;
        case VK_XBUTTON2:
            in.mi.dwFlags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
            in.mi.mouseData = XBUTTON2;
            break;
        }
        in.mi.dwExtraInfo = INPUT_EXTRA_FLAGS_EMULATED;
        return;
    }
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vkCode;
    in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    // games read scancodes; arrows/Ins/Del/RCtrl/... are extended (0xE0xx) and become numpad keys without the flag
    UINT scCode = MapVirtualKeyA(vkCode, MAPVK_VK_TO_VSC_EX);
    if (scCode) {
        in.ki.wScan = (WORD)(scCode & 0xFF);
        in.ki.dwFlags |= KEYEVENTF_SCANCODE;
        if (scCode & 0xE000) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    in.ki.dwExtraInfo = INPUT_EXTRA_FLAGS_EMULATED;
}

void Keyboard::Press(HWND hwnd, unsigned short vkCode)
{
    // one SendInput call: down+up land in the input stream back-to-back, nothing can interleave
    INPUT in[2];
    MakeInput(in[0], vkCode, true);
    MakeInput(in[1], vkCode, false);
    SendInput(2, in, sizeof(INPUT));
}

void Keyboard::SetState(unsigned short vkCode, bool down)
{
    INPUT in;
    MakeInput(in, vkCode, down);
    SendInput(1, &in, sizeof(INPUT));
}

void Keyboard::OnPress(Callback_t&& func)
{
    _onPress = std::forward<Callback_t>(func);
}

void Keyboard::OnRelease(Callback_t&& func)
{
    _onRelease = std::forward<Callback_t>(func);
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
    Keyboard& self = Keyboard::Instance();
    if (nCode == HC_ACTION) {
        LPKBDLLHOOKSTRUCT data = (LPKBDLLHOOKSTRUCT)lParam;
        bool isEmulated = (data->dwExtraInfo & INPUT_EXTRA_FLAGS_EMULATED) != 0;
        if (!isEmulated) {
            switch (wParam) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN: {
                if (self.HandleDown(data->vkCode)) return 1;
                break;
            }
            case WM_KEYUP:
            case WM_SYSKEYUP: {
                if (self.HandleUp(data->vkCode)) return 1;
                break;
            }
            }
        }
    }
    return CallNextHookEx(self._hhook, nCode, wParam, lParam);
}

LRESULT CALLBACK Keyboard::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    Keyboard& self = Keyboard::Instance();
    if (nCode == HC_ACTION) {
        LPMSLLHOOKSTRUCT data = (LPMSLLHOOKSTRUCT)lParam;
        bool isEmulated = (data->dwExtraInfo & INPUT_EXTRA_FLAGS_EMULATED) != 0;
        if (!isEmulated) {
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
            if (vkCode && (down ? self.HandleDown(vkCode) : self.HandleUp(vkCode))) return 1;
        }
    }
    return CallNextHookEx(self._mouseHook, nCode, wParam, lParam);
}

bool Keyboard::HandleDown(unsigned short vkCode)
{
    if (IsModifier(vkCode)) {
        _state[vkCode] = GetTickCount();
        return false;
    }
    bool repeat = _state[vkCode];
    if (!repeat) _state[vkCode] = GetTickCount();
    return _onPress && _onPress(vkCode, repeat);
}

bool Keyboard::HandleUp(unsigned short vkCode)
{
    if (IsModifier(vkCode)) {
        _state[vkCode] = 0;
        return false;
    }
    bool repeat = !_state[vkCode];
    if (!repeat) _state[vkCode] = 0;
    return _onRelease && _onRelease(vkCode, repeat);
}
