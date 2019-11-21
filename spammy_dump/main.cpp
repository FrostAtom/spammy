#include <Windows.h>
#include <cstdio>
#define PROGRAMNAME "spammy_dump"

HHOOK hhook;

LRESULT __stdcall HookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
    static LPKBDLLHOOKSTRUCT lpkbdStruct;
    if (nCode == HC_ACTION){
        switch(wParam){
        case WM_KEYUP:
        case WM_SYSKEYUP:
    		lpkbdStruct = (LPKBDLLHOOKSTRUCT)lParam;
    		printf("vkCode = 0x%02X\tscanCode = 0x%02X\n", lpkbdStruct->vkCode, lpkbdStruct->scanCode);
            break;
        }
    }

    return CallNextHookEx(hhook, nCode, wParam, lParam);
}

int main()
{
    try {
        if (hhook = SetWindowsHookEx(WH_KEYBOARD_LL, HookCallback, NULL, 0); !hhook)
            throw "SetWindowsHookEx fail";

        for (MSG msg; GetMessageA(&msg, NULL, 0, 0) != 0; );
    }
    catch (const char* e){
        MessageBox(NULL, e, PROGRAMNAME, MB_ICONERROR | MB_OK);
        return EXIT_FAILURE;
    }

	return EXIT_SUCCESS;
}