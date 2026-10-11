#include "Win32/Keyboard.h"

namespace {

constexpr ULONG_PTR kEmulatedInputTag = 0x80000000;
// unassigned VK: tapped while Alt is down, it keeps Alt's up from opening the window menu
constexpr unsigned short kMenuMaskVk = 0xE8;

bool IsEmulated(ULONG_PTR extraInfo) noexcept
{
    return (extraInfo & kEmulatedInputTag) != 0;
}

// MAPVK_VK_TO_VSC_EX prefixes extended keys (arrows, Ins/Del, RCtrl, ...) with 0xE0; sent or named without the
// extended flag, they become their numpad twins
bool IsExtendedScanCode(UINT scanCodeEx) noexcept
{
    return (scanCodeEx & 0xE000) != 0;
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
        in.mi.dwExtraInfo = kEmulatedInputTag;
        return in;
    }
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vkCode;
    in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    if (const UINT scanCode = MapVirtualKeyA(vkCode, MAPVK_VK_TO_VSC_EX)) {
        in.ki.wScan = (WORD)(scanCode & 0xFF);
        in.ki.dwFlags |= KEYEVENTF_SCANCODE; // games read the scancode, not the VK
        if (IsExtendedScanCode(scanCode)) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    in.ki.dwExtraInfo = kEmulatedInputTag;
    return in;
}

void SendUninterrupted(std::span<INPUT> inputs)
{
    SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
}

void CreateMessageQueue()
{
    MSG msg;
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
}

const char* NameMissingFromKeyboardLayouts(unsigned short vkCode)
{
    switch (vkCode) {
    case VK_LBUTTON: return "Mouse Left";
    case VK_RBUTTON: return "Mouse Right";
    case VK_MBUTTON: return "Mouse Middle";
    case VK_XBUTTON1: return "Mouse 4";
    case VK_XBUTTON2: return "Mouse 5";
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
    return nullptr;
}

const char* KeyboardLayoutKeyName(unsigned short vkCode)
{
    constexpr int kScanCodeShift = 16;
    constexpr LONG kExtendedKeyFlag = 1 << 24;
    static char buf[64];
    buf[0] = '\0';
    const UINT scanCode = MapVirtualKeyA(vkCode, MAPVK_VK_TO_VSC_EX);
    LONG lParam = (LONG)((scanCode & 0xFF) << kScanCodeShift);
    if (IsExtendedScanCode(scanCode)) lParam |= kExtendedKeyFlag;
    GetKeyNameTextA(lParam, buf, std::size(buf));
    return buf;
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
    const bool attachedAsAsked = _keyboardHook && _withMouse == withMouse && (!withMouse || _mouseHook);
    if (attachedAsAsked) return true;
    Detach();
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
    _swallowedDown.fill(false);
    _altMasked = false;
    // not _altsToRelease: a pending Alt lift must still reach the system
}

void Keyboard::SyncState()
{
    for (unsigned short vkCode = 1; vkCode < _pressed.size(); ++vkCode) {
        // LL hooks only deliver left/right modifier VKs, so a generic one marked here would never be released
        const bool isGenericModifier = vkCode == VK_SHIFT || vkCode == VK_CONTROL || vkCode == VK_MENU;
        _pressed[vkCode] = !isGenericModifier && (GetAsyncKeyState(vkCode) & 0x8000);
    }
}

void Keyboard::ThreadProc(std::stop_token stop, std::promise<bool> ready)
{
    const DWORD threadId = GetCurrentThreadId();
    // the raw input thread blocks on every hook call
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    CreateMessageQueue(); // the stop callback's PostThreadMessage fails while there is none

    _keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, &LowLevelKeyboardProc, nullptr, 0);
    if (!_keyboardHook) {
        ready.set_value(false);
        return;
    }
    if (_withMouse) _mouseHook = SetWindowsHookExW(WH_MOUSE_LL, &LowLevelMouseProc, nullptr, 0);
    SyncState();
    ready.set_value(true);

    std::stop_callback quitOnStop(stop, [threadId]() { PostThreadMessageW(threadId, WM_QUIT, 0, 0); });
    MSG msg;
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
    INPUT in[2] = {MakeInput(vkCode, true), MakeInput(vkCode, false)};
    SendUninterrupted(in);
}

void Keyboard::MaskAlt()
{
    if (_altMasked) return;
    _altMasked = true;
    _altsToRelease |= (_pressed[VK_LMENU] ? kLeftAltBit : 0u) | (_pressed[VK_RMENU] ? kRightAltBit : 0u);
}

void Keyboard::KeyDownWithoutAlt(unsigned short vkCode)
{
    INPUT in[5];
    UINT count = 0;
    if (const unsigned alts = _altsToRelease.exchange(0)) {
        in[count++] = MakeInput(kMenuMaskVk, true);
        in[count++] = MakeInput(kMenuMaskVk, false);
        if (alts & kLeftAltBit) in[count++] = MakeInput(VK_LMENU, false);
        if (alts & kRightAltBit) in[count++] = MakeInput(VK_RMENU, false);
    }
    in[count++] = MakeInput(vkCode, true);
    SendUninterrupted(std::span(in, count));
}

void Keyboard::KeyUp(unsigned short vkCode)
{
    INPUT in = MakeInput(vkCode, false);
    SendInput(1, &in, sizeof(INPUT));
}

const char* Keyboard::GetKeyName(unsigned short vkCode)
{
    if (const char* name = NameMissingFromKeyboardLayouts(vkCode)) return name;
    return KeyboardLayoutKeyName(vkCode);
}

LRESULT CALLBACK Keyboard::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    Keyboard& self = Instance();
    if (nCode == HC_ACTION) {
        const KBDLLHOOKSTRUCT* data = (const KBDLLHOOKSTRUCT*)lParam;
        const bool down = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
        const bool up = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
        const bool fitsKeyTables = data->vkCode < kKeyboardKeysCount;
        if ((down || up) && fitsKeyTables && !IsEmulated(data->dwExtraInfo) &&
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
    const bool wasDown = _pressed[vkCode].exchange(down);
    const bool repeat = wasDown == down;
    if (IsModifier(vkCode)) {
        const bool isAlt = vkCode == VK_LMENU || vkCode == VK_RMENU;
        if (!isAlt || !_altMasked) return false;
        const bool lastAltReleased = !_pressed[VK_LMENU] && !_pressed[VK_RMENU];
        if (lastAltReleased) _altMasked = false;
        return true;
    }
    const Callback_t& callback = down ? _onPress : _onRelease;
    const bool swallow = callback && callback(vkCode, repeat);
    // swallow an up only if we swallowed its down: if win32k saw the down (key held before the hook went up),
    // blocking the up leaves the key stuck system-wide
    if (down && !repeat) return _swallowedDown[vkCode] = swallow;
    return swallow && _swallowedDown[vkCode];
}
