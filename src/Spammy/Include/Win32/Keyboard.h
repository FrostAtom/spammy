#pragma once
#include "Headers.h"
#include <future>
#define sKeyboard Keyboard::Instance()

enum KeyMod : std::uint8_t {
    KeyMod_None = 0,
    KeyMod_Shift = (1 << 0),
    KeyMod_Alt = (1 << 1),
    KeyMod_Ctrl = (1 << 2),
};

inline constexpr size_t kKeyboardKeysCount = 255;
inline constexpr size_t kKeyboardKeyModCount = (KeyMod_Shift | KeyMod_Alt | KeyMod_Ctrl) + 1;

class Keyboard {
public:
    using KeyCallback_t = std::function<bool(unsigned short vkCode, bool down, bool repeat)>;
    using StateCallback_t = std::function<void()>;

private:
    static constexpr unsigned kLeftAltBit = 1u << 0;
    static constexpr unsigned kRightAltBit = 1u << 1;

    HHOOK _keyboardHook = nullptr;
    HHOOK _mouseHook = nullptr;
    bool _withMouse = false;
    std::jthread _thread;
    std::array<std::atomic<bool>, kKeyboardKeysCount> _pressed; // written by the hook thread
    std::atomic<unsigned> _altsToRelease = 0;
    KeyCallback_t _swallowsNonModifierKey = [](unsigned short, bool, bool) { return false; };
    StateCallback_t _onAnyKeyPressOrRelease = [] {};
    StateCallback_t _onHooksRemoved = [] {};
    // hook thread only
    std::array<bool, kKeyboardKeysCount> _swallowedDown = {};
    bool _altMasked = false;

    Keyboard() = default;

public:
    ~Keyboard();
    static Keyboard& Instance();
    // the mouse hook sees every mouse move: only ask for it when buttons matter
    [[nodiscard]] bool Attach(bool withMouse);
    void Detach();

    unsigned TestModifiers() const noexcept;
    static constexpr bool IsModifier(unsigned short vkCode) noexcept;
    static constexpr bool IsMouseButton(unsigned short vkCode) noexcept;
    bool IsPressed(unsigned short vkCode) const noexcept { return _pressed[vkCode]; }
    void Press(unsigned short vkCode);
    // hook thread: hides Alt from the system until both Alts are released; the next KeyDownWithoutAlt lifts it
    void MaskAlt();
    void KeyDownWithoutAlt(unsigned short vkCode);
    void KeyUp(unsigned short vkCode);
    // the first two run on the hook thread, which Windows drops if it blocks; the last on the detaching thread
    void Listen(KeyCallback_t swallowsNonModifierKey, StateCallback_t onAnyKeyPressOrRelease,
                StateCallback_t onHooksRemoved)
    {
        _swallowsNonModifierKey = std::move(swallowsNonModifierKey);
        _onAnyKeyPressOrRelease = std::move(onAnyKeyPressOrRelease);
        _onHooksRemoved = std::move(onHooksRemoved);
    }
    static constexpr size_t kMaxKeyNameChars = 64;
    static constexpr size_t kMaxUtf8BytesPerUtf16Unit = 3;
    using KeyNameBuf = char[kMaxKeyNameChars * kMaxUtf8BytesPerUtf16Unit];
    static const char* GetKeyName(unsigned short vkCode, KeyNameBuf& buf);

private:
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    void ThreadProc(const std::stop_token& stop, std::promise<bool> ready);
    void SyncState();
    void ResetState();
    bool HandleKey(unsigned short vkCode, bool down);
    bool FilterModifier(unsigned short vkCode);
    bool FilterKey(unsigned short vkCode, bool down, bool repeat);
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
