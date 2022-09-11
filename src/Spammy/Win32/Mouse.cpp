#include "Mouse.h"

Mouse::Mouse()
    : _hhook(NULL)
{
    memset(&_state, NULL, sizeof(_state));
}

Mouse::~Mouse()
{
    detach();
}

Mouse& Mouse::instance()
{
    static std::unique_ptr<Mouse>keyboard(new Mouse());
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
    if (_hhook) { UnhookWindowsHookEx(_hhook); _hhook = NULL; }
}

DWORD Mouse::isPressed(unsigned short vkCode)
{
    return _state[vkCode];
}

void Mouse::wheel(float delta)
{

}

void Mouse::press(unsigned short vkCode)
{
    
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

static unsigned short MouseEvent2VKCode(WPARAM wParam)
{
    switch (wParam) {
    case WM_LBUTTONDOWN: case WM_LBUTTONUP:
        return VK_LBUTTON;
    case WM_RBUTTONDOWN: case WM_RBUTTONUP:
        return VK_RBUTTON;
    case WM_MBUTTONDOWN: case WM_MBUTTONUP:
        return VK_MBUTTON;
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
            case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: {
                if (self.handleDown(MouseEvent2VKCode(wParam)))
                    return 1;
            }
            case WM_LBUTTONUP: case WM_RBUTTONUP: {
                if (self.handleUp(MouseEvent2VKCode(wParam)))
                    return 1;
            }
            case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL: {
                if (self.handleWheel(HIWORD(data->mouseData) / WHEEL_DELTA))
                    return 1;
            }
            case WM_MOUSEMOVE:
                break;
            }
        }
    }
    return CallNextHookEx(self._hhook, nCode, wParam, lParam);
}

bool Mouse::handleWheel(float delta)
{

    return false;
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
