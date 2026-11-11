#include "Win32/Keyboard.h"

namespace {

constexpr ULONG_PTR kEmulatedInputTag = 0x80000000;
// unassigned VK: tapped while Alt is down, it keeps Alt's up from opening the window menu
constexpr unsigned short kMenuMaskVk = 0xE8;

bool IsEmulated(ULONG_PTR extraInfo) noexcept
{
    return (extraInfo & kEmulatedInputTag) != 0;
}

// MAPVK_VK_TO_VSC_EX marks extended keys (arrows, Ins/Del, RCtrl) with 0xE0; without the flag they act as numpad keys
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

struct VkName {
    unsigned short vkCode;
    const char* name;
};
constexpr VkName kNamesMissingFromKeyboardLayouts[] = {
    {VK_LBUTTON, "Mouse Left"},
    {VK_RBUTTON, "Mouse Right"},
    {VK_MBUTTON, "Mouse Middle"},
    {VK_XBUTTON1, "Mouse 4"},
    {VK_XBUTTON2, "Mouse 5"},
    {VK_SLEEP, "Sleep"},
    {VK_VOLUME_MUTE, "Mute"},
    {VK_VOLUME_DOWN, "Volume -"},
    {VK_VOLUME_UP, "Volume +"},
    {VK_MEDIA_PLAY_PAUSE, "Play"},
    {VK_MEDIA_PREV_TRACK, "Prev Track"},
    {VK_MEDIA_NEXT_TRACK, "Next Track"},
    {VK_MEDIA_STOP, "Media Stop"},
    {VK_LAUNCH_MEDIA_SELECT, "Media"},
    {VK_LAUNCH_MAIL, "Mail"},
    {VK_LAUNCH_APP1, "App 1"},
    {VK_LAUNCH_APP2, "App 2"},
    {VK_BROWSER_BACK, "Web Back"},
    {VK_BROWSER_FORWARD, "Web Fwd"},
    {VK_BROWSER_HOME, "Web Home"},
    {VK_BROWSER_REFRESH, "Web Reload"},
    {VK_BROWSER_STOP, "Web Stop"},
    {VK_BROWSER_SEARCH, "Web Search"},
    {VK_BROWSER_FAVORITES, "Web Fav"},
};

const char* NameMissingFromKeyboardLayouts(unsigned short vkCode)
{
    const auto* found = std::ranges::find(kNamesMissingFromKeyboardLayouts, vkCode, &VkName::vkCode);
    return found != std::end(kNamesMissingFromKeyboardLayouts) ? found->name : nullptr;
}

const char* KeyboardLayoutKeyName(unsigned short vkCode, Keyboard::KeyNameBuf& buf)
{
    constexpr int kScanCodeShift = 16;
    constexpr LONG kExtendedKeyFlag = 1 << 24;
    const UINT scanCode = MapVirtualKeyW(vkCode, MAPVK_VK_TO_VSC_EX);
    LONG lParam = (LONG)(scanCode & 0xFF) << kScanCodeShift;
    if (IsExtendedScanCode(scanCode)) lParam |= kExtendedKeyFlag;

    wchar_t wide[Keyboard::kMaxKeyNameChars];
    const int wideLength = GetKeyNameTextW(lParam, wide, (int)std::size(wide));
    std::byte stack[2 * sizeof(Keyboard::KeyNameBuf)];
    std::pmr::monotonic_buffer_resource stackThenHeap(stack, sizeof(stack));
    const std::pmr::string utf8 = una::utf16to8<wchar_t, char, std::pmr::polymorphic_allocator<char>>(
        std::wstring_view(wide, (size_t)wideLength), &stackThenHeap);
    *std::ranges::copy(utf8, buf).out = '\0';
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
    _thread = std::jthread([this, ready = std::move(ready)](const std::stop_token& stop) mutable {
        ThreadProc(stop, std::move(ready));
    });
    return installed.get();
}

void Keyboard::Detach()
{
    if (!_thread.joinable()) return;
    _thread.request_stop();
    _thread.join();
    ResetState();
    _onHooksRemoved();
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
    for (size_t vkCode = 1; vkCode < _pressed.size(); ++vkCode) {
        // LL hooks only deliver left/right modifier VKs, so a generic one marked here would never be released
        const bool isGenericModifier = vkCode == VK_SHIFT || vkCode == VK_CONTROL || vkCode == VK_MENU;
        _pressed[vkCode] = !isGenericModifier && (GetAsyncKeyState((int)vkCode) & 0x8000);
    }
}

void Keyboard::ThreadProc(const std::stop_token& stop, std::promise<bool> ready)
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

const char* Keyboard::GetKeyName(unsigned short vkCode, KeyNameBuf& buf)
{
    if (const char* name = NameMissingFromKeyboardLayouts(vkCode)) return name;
    return KeyboardLayoutKeyName(vkCode, buf);
}

LRESULT CALLBACK Keyboard::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    Keyboard& self = Instance();
    if (nCode == HC_ACTION) {
        const auto* data = (const KBDLLHOOKSTRUCT*)lParam;
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
        const auto* data = (const MSLLHOOKSTRUCT*)lParam;
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
    const bool swallow = IsModifier(vkCode) ? FilterModifier(vkCode) : FilterKey(vkCode, down, repeat);
    if (!repeat) _onAnyKeyPressOrRelease();
    return swallow;
}

bool Keyboard::FilterModifier(unsigned short vkCode)
{
    const bool isAlt = vkCode == VK_LMENU || vkCode == VK_RMENU;
    if (!isAlt || !_altMasked) return false;
    const bool lastAltReleased = !_pressed[VK_LMENU] && !_pressed[VK_RMENU];
    if (lastAltReleased) _altMasked = false;
    return true;
}

bool Keyboard::FilterKey(unsigned short vkCode, bool down, bool repeat)
{
    const bool swallow = _swallowsNonModifierKey(vkCode, down, repeat);
    // an up whose down win32k saw (key held before the hook went up) must pass, or the key stays stuck system-wide
    if (down && !repeat) return _swallowedDown[vkCode] = swallow;
    return swallow && _swallowedDown[vkCode];
}
