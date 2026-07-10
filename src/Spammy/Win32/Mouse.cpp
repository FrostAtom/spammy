#include "Mouse.h"

Mouse::Mouse() : _hhook(NULL)
{
    memset(&_state, NULL, sizeof(_state));
}

Mouse::~Mouse()
{
    detach();
}

Mouse& Mouse::instance()
{
    static std::unique_ptr<Mouse> keyboard(new Mouse());
    return *keyboard;
}

bool Mouse::isAttached()
{
    return _hhook;
}

bool Mouse::attach()
{
    _hhook = SetWindowsHookExW(WH_MOUSE_LL, &LowLevelMouseProc, NULL, 0);
    return _hhook;
}

void Mouse::detach()
{
    if (_hhook) {
        UnhookWindowsHookEx(_hhook);
        _hhook = NULL;
    }
}

DWORD Mouse::isPressed(unsigned short vkCode)
{
    return _state[vkCode];
}

void Mouse::wheel(float delta)
{
    mouse_event(MOUSEEVENTF_WHEEL, 0, 0, (DWORD)(delta * WHEEL_DELTA), INPUT_EXTRA_FLAGS_EMULATED);
}

void Mouse::press(unsigned short vkCode)
{
    DWORD down = 0, up = 0;
    switch (vkCode) {
    case VK_LBUTTON:
        down = MOUSEEVENTF_LEFTDOWN;
        up = MOUSEEVENTF_LEFTUP;
        break;
    case VK_RBUTTON:
        down = MOUSEEVENTF_RIGHTDOWN;
        up = MOUSEEVENTF_RIGHTUP;
        break;
    case VK_MBUTTON:
        down = MOUSEEVENTF_MIDDLEDOWN;
        up = MOUSEEVENTF_MIDDLEUP;
        break;
    default: return;
    }
    mouse_event(down, 0, 0, 0, INPUT_EXTRA_FLAGS_EMULATED);
    mouse_event(up, 0, 0, 0, INPUT_EXTRA_FLAGS_EMULATED);
}

void Mouse::onWheel(Callback_t&& func)
{
    _onWheel = std::forward<Callback_t>(func);
}

void Mouse::onPress(Callback_t&& func)
{
    _onPress = std::forward<Callback_t>(func);
}

void Mouse::onRelease(Callback_t&& func)
{
    _onRelease = std::forward<Callback_t>(func);
}

const char* Mouse::getKeyName(unsigned short vkCode)
{
    switch (vkCode) {
    case VK_LBUTTON: return "Mouse Left";
    case VK_RBUTTON: return "Mouse Right";
    case VK_MBUTTON: return "Mouse Middle";
    case VK_XBUTTON1: return "Mouse X1";
    case VK_XBUTTON2: return "Mouse X2";
    }
    return "";
}

static unsigned short MouseEvent2VKCode(WPARAM wParam)
{
    switch (wParam) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP: return VK_LBUTTON;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP: return VK_RBUTTON;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP: return VK_MBUTTON;
    }
    return 0;
}

LRESULT CALLBACK Mouse::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    Mouse& self = Mouse::instance();
    if (nCode == HC_ACTION) {
        MSLLHOOKSTRUCT* data = (MSLLHOOKSTRUCT*)lParam;
        bool isEmulated = (data->dwExtraInfo & INPUT_EXTRA_FLAGS_EMULATED) != 0;
        if (!isEmulated) {
            switch (wParam) {
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
                if (self.handleDown(MouseEvent2VKCode(wParam))) return 1;
                break;
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
                if (self.handleUp(MouseEvent2VKCode(wParam))) return 1;
                break;
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
                if (self.handleWheel((short)HIWORD(data->mouseData) / (float)WHEEL_DELTA)) return 1;
                break;
            }
        }
    }
    return CallNextHookEx(self._hhook, nCode, wParam, lParam);
}

bool Mouse::handleWheel(float delta)
{
    return _onWheel && _onWheel(0, (DWORD)(LONG)delta);
}

bool Mouse::handleDown(unsigned short vkCode)
{
    _state[vkCode] = GetTickCount();
    return _onPress && _onPress(vkCode, NULL);
}

bool Mouse::handleUp(unsigned short vkCode)
{
    _state[vkCode] = NULL;
    return _onRelease && _onRelease(vkCode, NULL);
}
