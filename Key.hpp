#include <windows.h>
#define ITSFAKE 0x80000000

class Key {
public:
    Key(DWORD vkCode) : isPressed(false)
    {
        memset(&input,0,sizeof(input));
        input.type = INPUT_KEYBOARD;
        input.ki.dwExtraInfo = ITSFAKE;
        input.ki.wVk = vkCode;
        input.ki.wScan = MapVirtualKeyA(vkCode, MAPVK_VK_TO_VSC);
    }

    void Press()
    {
        input.ki.dwFlags = 0;
        SendInput(1, &input, sizeof(input));
    }

    void Release()
    {
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(input));
    }

    bool isPressed;
private:
    INPUT input;
};