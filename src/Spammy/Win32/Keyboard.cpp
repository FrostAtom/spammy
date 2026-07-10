#include "Keyboard.h"

Keyboard::Keyboard() : _hhook(NULL), _threadId(0)
{
    memset(&_state, NULL, sizeof(_state));
}

Keyboard::~Keyboard()
{
    detach();
}

Keyboard& Keyboard::instance()
{
    static std::unique_ptr<Keyboard> keyboard(new Keyboard());
    return *keyboard;
}

bool Keyboard::isAttached()
{
    return _thread.joinable();
}

bool Keyboard::atttach()
{
    if (_thread.joinable()) return _hhook != NULL;
    std::promise<bool> ready;
    std::future<bool> result = ready.get_future();
    _thread = std::jthread([this, &ready](std::stop_token stop) { threadProc(stop, ready); });
    return result.get();
}

void Keyboard::detach()
{
    if (!_thread.joinable()) return;
    _thread.request_stop();
    _thread.join();
    _threadId = 0;
    _state.fill(0);
}

void Keyboard::syncState()
{
    DWORD now = GetTickCount();
    for (unsigned short vkCode = 1; vkCode < _state.size(); ++vkCode)
        _state[vkCode] = (GetAsyncKeyState(vkCode) & 0x8000) ? now : 0;
}

void Keyboard::threadProc(std::stop_token stop, std::promise<bool>& ready)
{
    _threadId = GetCurrentThreadId();
    // force message-queue creation so detach's WM_QUIT can never race ahead of it
    MSG msg;
    PeekMessageW(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    _hhook = SetWindowsHookExW(WH_KEYBOARD_LL, &LowLevelKeyboardProc, NULL, 0);
    if (_hhook) syncState();
    ready.set_value(_hhook != NULL);
    if (!_hhook) return;

    // wakes the blocking GetMessage the instant detach() requests a stop
    std::stop_callback onStop(stop, [id = _threadId]() { PostThreadMessageW(id, WM_QUIT, 0, 0); });

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnhookWindowsHookEx(_hhook);
    _hhook = NULL;
}

bool Keyboard::testModifiers(unsigned mods)
{
    DWORD _mods = testModifiers();
    return (_mods & mods) == mods;
}

unsigned Keyboard::testModifiers()
{
    DWORD result = KeyMod_None;
    if (_state[VK_LSHIFT] || _state[VK_RSHIFT] || _state[VK_SHIFT]) result |= KeyMod_Shift;
    if (_state[VK_LMENU] || _state[VK_RMENU] || _state[VK_MENU]) result |= KeyMod_Alt;
    if (_state[VK_LCONTROL] || _state[VK_RCONTROL] || _state[VK_CONTROL]) result |= KeyMod_Ctrl;
    return result;
}

unsigned Keyboard::isModifier(unsigned short vkCode)
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

DWORD Keyboard::isPressed(unsigned short vkCode)
{
    return _state[vkCode];
}

void Keyboard::press(HWND hwnd, unsigned short vkCode)
{
    UINT scCode = MapVirtualKeyA(vkCode, MAPVK_VK_TO_VSC_EX);
    //unsigned lParam = 0 | ((scCode & 0xFF) << 16);
    //PostMessageW(hwnd, WM_KEYDOWN, vkCode, lParam);
    //lParam = 1 | ((scCode & 0xFF) << 16) | (1 << 30) | (1 << 31);
    //PostMessageW(hwnd, WM_KEYUP, vkCode, lParam);
    keybd_event(vkCode, scCode, 0, INPUT_EXTRA_FLAGS_EMULATED);
    keybd_event(vkCode, scCode, KEYEVENTF_KEYUP, INPUT_EXTRA_FLAGS_EMULATED);
}

void Keyboard::onPress(Callback_t&& func)
{
    _onPress = std::forward<Callback_t>(func);
}

void Keyboard::onRelease(Callback_t&& func)
{
    _onRelease = std::forward<Callback_t>(func);
}

const char* Keyboard::geyKeyName(unsigned short vkCode)
{
    static char buf[64] = {0};
    GetKeyNameTextA(MapVirtualKeyA(vkCode, MAPVK_VK_TO_VSC) << 16, buf, std::size(buf));
    return buf;
}

LRESULT CALLBACK Keyboard::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    Keyboard& self = Keyboard::instance();
    if (nCode == HC_ACTION) {
        LPKBDLLHOOKSTRUCT data = (LPKBDLLHOOKSTRUCT)lParam;
        bool isEmulated = (data->dwExtraInfo & INPUT_EXTRA_FLAGS_EMULATED) != 0;
        if (!isEmulated) {
            switch (wParam) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN: {
                if (self.handleDown(data->vkCode)) return 1;
                break;
            }
            case WM_KEYUP:
            case WM_SYSKEYUP: {
                if (self.handleUp(data->vkCode)) return 1;
                break;
            }
            }
        }
    }
    return CallNextHookEx(self._hhook, nCode, wParam, lParam);
}

bool Keyboard::handleDown(unsigned short vkCode)
{
    if (isModifier(vkCode)) {
        _state[vkCode] = GetTickCount();
        return false;
    }
    bool repeat = _state[vkCode];
    if (!repeat) _state[vkCode] = GetTickCount();
    return _onPress && _onPress(vkCode, repeat);
}

bool Keyboard::handleUp(unsigned short vkCode)
{
    if (isModifier(vkCode)) {
        _state[vkCode] = 0;
        return false;
    }
    bool repeat = !_state[vkCode];
    if (!repeat) _state[vkCode] = 0;
    return _onRelease && _onRelease(vkCode, repeat);
}
