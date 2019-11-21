#include <windows.h>
#define ITSFAKE 0xF0000000

class Key {
public:
    Key(DWORD vkCode)
    {
        ZeroMemory(&input,sizeof(input));
        input.type = INPUT_KEYBOARD;
        input.ki.dwExtraInfo = ITSFAKE;
        input.ki.wVk = vkCode;
        input.ki.wScan = MapVirtualKeyA(vkCode, MAPVK_VK_TO_VSC);
    }

    void Down()
    {
        input.ki.dwFlags = 0;
        SendInput(1, &input, sizeof(input));
    }

    void Up()
    {
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(input));
    }

    bool pressed = false;
private:
    INPUT input;
};