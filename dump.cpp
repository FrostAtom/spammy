#include <Windows.h>
#include <cstdio>

HHOOK hk;
KBDLLHOOKSTRUCT kbdStruct;

LRESULT __stdcall HookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0){
        switch(wParam){
        case WM_KEYUP:
        case WM_SYSKEYUP:
    		kbdStruct = *((KBDLLHOOKSTRUCT*)lParam);
    		printf("vkCode = 0x%02X\n", kbdStruct.vkCode);
            break;
        }
    }

    return CallNextHookEx(hk, nCode, wParam, lParam);
}

int main()
{
    if (!(hk = SetWindowsHookEx(WH_KEYBOARD_LL, HookCallback, NULL, 0))) {
        MessageBoxA(NULL, "Error installing keyboard hook", "Spammy", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    MSG msg;
    while(GetMessageA(&msg, NULL, 0, 0) != 0);

	return EXIT_SUCCESS;
}