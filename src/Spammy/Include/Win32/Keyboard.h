#pragma once
#include "Headers.h"
#define sKeyboard Keyboard::Instance()

enum KeyMod {
    KeyMod_None = 0,
    KeyMod_Shift = (1 << 0),
    KeyMod_Alt = (1 << 1),
    KeyMod_Ctrl = (1 << 2),
};

inline constexpr size_t kKeyboardKeysCount = 255;
inline constexpr size_t kKeyboardKeyModCount = (KeyMod_Shift | KeyMod_Alt | KeyMod_Ctrl) + 1;

class Keyboard {
public:
    // runs on the hook thread: must only decide whether to swallow the event and return immediately —
    // any blocking here (locks, input injection, I/O) stalls global input and gets the hook silently dropped
    using Callback_t = std::function<bool(UINT vkCode, bool repeat)>;

private:
    HHOOK _keyboardHook = nullptr;
    HHOOK _mouseHook = nullptr;
    bool _withMouse = false;
    std::jthread _thread;
    // written by the hook thread, read by the input worker and the UI
    std::array<std::atomic<bool>, kKeyboardKeysCount> _pressed;
    // hook thread only: true while the physical down of this key was swallowed by us, so win32k never saw it
    std::array<bool, kKeyboardKeysCount> _swallowed = {};
    Callback_t _onPress, _onRelease;

    Keyboard() = default;

public:
    ~Keyboard();
    static Keyboard& Instance();
    // withMouse: also install WH_MOUSE_LL — it sees every mouse move, so only ask for it when buttons matter.
    // Re-attaches (and resyncs state) if already attached with a different mouse setting.
    bool Attach(bool withMouse);
    void Detach();

    unsigned TestModifiers() const noexcept;
    static constexpr bool IsModifier(unsigned short vkCode) noexcept;
    static constexpr bool IsMouseButton(unsigned short vkCode) noexcept;
    bool IsPressed(unsigned short vkCode) const noexcept { return _pressed[vkCode]; }
    void Press(unsigned short vkCode);
    void OnPress(Callback_t&& func) { _onPress = std::move(func); }
    void OnRelease(Callback_t&& func) { _onRelease = std::move(func); }
    static const char* GetKeyName(unsigned short vkCode);

private:
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    void ThreadProc(std::stop_token stop, std::promise<bool> ready);
    void SyncState();
    void ResetState();
    bool HandleKey(unsigned short vkCode, bool down);
};

constexpr bool Keyboard::IsModifier(unsigned short vkCode) noexcept
{
    switch (vkCode) {
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU: return true;
    }
    return false;
}

constexpr bool Keyboard::IsMouseButton(unsigned short vkCode) noexcept
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
