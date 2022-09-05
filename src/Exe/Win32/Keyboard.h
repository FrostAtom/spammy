#pragma once
#include <Windows.h>
#include <array>
#include <functional>
#define sKeyboard Keyboard::instance()
#define INPUT_EXTRA_FLAGS_EMULATED 0x80000000
#define KEYBOARD_KEYS_COUNT (255)
#define KEYBOARD_KEYMOD_COUNT ((KeyMod_Shift | KeyMod_Alt | KeyMod_Ctrl) + 1)

enum KeyMod {
    KeyMod_None = 0,
    KeyMod_Shift = (1 << 0),
    KeyMod_Alt = (1 << 1),
    KeyMod_Ctrl = (1 << 2),
};

class Keyboard {
public:
    using Callback_t = std::function<bool(UINT vkCode, bool repeat)>;

private:
    HHOOK _hhook;
    std::array<DWORD, KEYBOARD_KEYS_COUNT> _state;
    Callback_t _onPress, _onRelease;

    Keyboard();
public:
    ~Keyboard();
    static Keyboard& instance();
    bool isAttached();
    bool atttach();
    void detach();

    // key total number
    static constexpr size_t count() { return KEYBOARD_KEYS_COUNT; }
    // check for all mods is pressed
    bool testModifiers(unsigned mods);
    // get pressed mods mask
    unsigned testModifiers();
    // key is modifier
    unsigned isModifier(unsigned short vkCode);
    // key is pressed
    DWORD isPressed(unsigned short vkCode);
    // send press notification to target HWND
    void press(HWND hwnd, unsigned short vkCode);
    // setups callback to key-down event
    void onPress(Callback_t&& func);
    // setups callback to key-up event
    void onRelease(Callback_t&& func);
    // returns key name
    static const char* geyKeyName(unsigned short vkCode);

private:
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    bool handleDown(unsigned short vkCode);
    bool handleUp(unsigned short vkCode);
};