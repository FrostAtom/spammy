#pragma once
#include "Headers.h"
#define sKeyboard Keyboard::Instance()
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
    HHOOK _mouseHook;
    std::jthread _thread;
    DWORD _threadId;
    std::array<DWORD, KEYBOARD_KEYS_COUNT> _state;
    Callback_t _onPress, _onRelease;

    Keyboard();

public:
    ~Keyboard();
    static Keyboard& Instance();
    bool Attach();
    void Detach();

    // key total number
    static constexpr size_t Count() { return KEYBOARD_KEYS_COUNT; }
    // check for all mods is pressed
    bool TestModifiers(unsigned mods);
    // get pressed mods mask
    unsigned TestModifiers();
    // key is modifier
    unsigned IsModifier(unsigned short vkCode);
    // key is mouse button
    static bool IsMouseButton(unsigned short vkCode);
    // key is pressed
    DWORD IsPressed(unsigned short vkCode);
    // send press notification to target HWND
    void Press(HWND hwnd, unsigned short vkCode);
    // emulate a standalone key-down or key-up
    void SetState(unsigned short vkCode, bool down);
    // setups callback to key-down event
    void OnPress(Callback_t&& func);
    // setups callback to key-up event
    void OnRelease(Callback_t&& func);
    // returns key name
    static const char* GetKeyName(unsigned short vkCode);

private:
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    void ThreadProc(std::stop_token stop, std::promise<bool>& ready);
    void SyncState();
    bool HandleDown(unsigned short vkCode);
    bool HandleUp(unsigned short vkCode);
};
