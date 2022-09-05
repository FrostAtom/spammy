#pragma once
#include "Keyboard.h"
#define MOUSE_KEYS_COUNT 5
#define sMouse Mouse::instance()

class Mouse {
public:
    using Callback_t = std::function<bool(UINT vkCode, DWORD delta)>;

private:
    HHOOK _hhook;
    std::array<DWORD, MOUSE_KEYS_COUNT> _state;
    Callback_t _onPress, _onRelease, _onWheel;

    Mouse();
public:
    ~Mouse();
    static Mouse& instance();
    bool isAttached();
    bool attach();
    void detach();

    DWORD isPressed(unsigned short vkCode);
    void press(unsigned short vkCode);
    void wheel(float delta);
    void onWheel(Callback_t&& func);
    void onPress(Callback_t&& func);
    void onRelease(Callback_t&& func);

    static constexpr size_t count() { return MOUSE_KEYS_COUNT; }
    static const char* getKeyName(unsigned short vkCode);

private:
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    bool handleWheel(float delta);
    bool handleDown(unsigned short vkCode);
    bool handleUp(unsigned short vkCode);
};