#include "Win32/Keyboard.h"

Keyboard::Keyboard() : _hhook(NULL), _threadId(0)
{
    memset(&_state, NULL, sizeof(_state));
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

bool Keyboard::Attach()
{
    if (_thread.joinable()) return _hhook != NULL;
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
    _state.fill(0);
}

void Keyboard::SyncState()
{
    DWORD now = GetTickCount();
    for (unsigned short vkCode = 1; vkCode < _state.size(); ++vkCode)
        _state[vkCode] = (GetAsyncKeyState(vkCode) & 0x8000) ? now : 0;
}

void Keyboard::ThreadProc(std::stop_token stop, std::promise<bool>& ready)
{
    _threadId = GetCurrentThreadId();
    // force message-queue creation so Detach's WM_QUIT can never race ahead of it
    MSG msg;
    PeekMessageW(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    _hhook = SetWindowsHookExW(WH_KEYBOARD_LL, &LowLevelKeyboardProc, NULL, 0);
    if (_hhook) SyncState();
    ready.set_value(_hhook != NULL);
    if (!_hhook) return;

    // wakes the blocking GetMessage the instant Detach() requests a stop
    std::stop_callback onStop(stop, [id = _threadId]() { PostThreadMessageW(id, WM_QUIT, 0, 0); });

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
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
    if (_state[VK_LSHIFT] || _state[VK_RSHIFT] || _state[VK_SHIFT]) result |= KeyMod_Shift;
    if (_state[VK_LMENU] || _state[VK_RMENU] || _state[VK_MENU]) result |= KeyMod_Alt;
    if (_state[VK_LCONTROL] || _state[VK_RCONTROL] || _state[VK_CONTROL]) result |= KeyMod_Ctrl;
    return result;
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

void Keyboard::Press(HWND hwnd, unsigned short vkCode)
{
    UINT scCode = MapVirtualKeyA(vkCode, MAPVK_VK_TO_VSC_EX);
    keybd_event(vkCode, scCode, 0, INPUT_EXTRA_FLAGS_EMULATED);
    keybd_event(vkCode, scCode, KEYEVENTF_KEYUP, INPUT_EXTRA_FLAGS_EMULATED);
}

void Keyboard::SetState(unsigned short vkCode, bool down)
{
    UINT scCode = MapVirtualKeyA(vkCode, MAPVK_VK_TO_VSC_EX);
    keybd_event(vkCode, scCode, down ? 0 : KEYEVENTF_KEYUP, INPUT_EXTRA_FLAGS_EMULATED);
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
    static char buf[64] = {0};
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
