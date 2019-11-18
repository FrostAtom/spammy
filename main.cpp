#include <Windows.h>
#include <errhandlingapi.h>
#include <map>
#include <algorithm>
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <synchapi.h>
#include <winspool.h>
#include "key.hpp"
#include <future>
#define SPAMINTERVAL 20
#define CFGPATH "spammy_cfg/"
#define CFGEXTENSION ".txt"
#define PROGRAMNAME "Spammy"


bool validWindow = false, pause = false;
std::map<DWORD, Key*> keys;
int pressedNow;
HANDLE hEvent;
CRITICAL_SECTION criticalSection;


bool LoadConfig(const char* name)
{
    std::string path;
    path.reserve(strlen(CFGPATH) + strlen(name) + strlen(CFGEXTENSION) + 1);
    path.append(CFGPATH);
    path.append(name);
    path.append(CFGEXTENSION);

    std::fstream file(path.c_str(), std::ios::in);
    if (file.is_open()){
        for(std::string line; getline(file, line);) {
            if (!line.empty()){
                DWORD vkCode = std::strtoul(line.c_str(), NULL, 16);
                keys[vkCode] = new Key(vkCode);
            }
        }
        file.close();
        
        if (!keys.empty())
            return true;
    }

    return false;
}

void CheckWindowValid(HWND hWnd)
{
    if (!keys.empty()){
        ResetEvent(hEvent);
        EnterCriticalSection(&criticalSection);
        for (auto& pair : keys)
            delete pair.second;
        keys.clear();
        LeaveCriticalSection(&criticalSection);

        pressedNow = 0; 
    }

    
    if (char* buffer = new char[MAX_PATH]; GetWindowTextA(hWnd, buffer, MAX_PATH) > 0){
        if (LoadConfig(buffer)){
            validWindow = true;
        }
        else{
            validWindow = false;
        }
    }
    else{
        validWindow = false;
    }
}

HWINEVENTHOOK hWinEventHook;
VOID CALLBACK WinEventProcCallback (HWINEVENTHOOK hWinEventHook, DWORD dwEvent, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
    if (dwEvent == EVENT_SYSTEM_FOREGROUND){
        CheckWindowValid(hwnd);
    }
}

void HandleLParam(LPKBDLLHOOKSTRUCT kbdStruct, bool pressed)
{
    if (auto pos = keys.find(kbdStruct->vkCode); pos != keys.end()) {
        if (pos->second->pressed != pressed){
            pos->second->pressed = pressed;

            if (pressed){
                pressedNow++;
                if (pressedNow == 1)
                    SetEvent(hEvent);
            }
            else{
                pressedNow--;
                if (pressedNow == 0)
                    ResetEvent(hEvent);
            }
        }
    }
}

HHOOK hhook;
LPKBDLLHOOKSTRUCT kbdStruct;
LRESULT __stdcall HookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && validWindow){
        kbdStruct = (KBDLLHOOKSTRUCT*)lParam;
        if (!(kbdStruct->dwExtraInfo & ITSFAKE)){
            switch(wParam){
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                if (!pause && kbdStruct->vkCode != VK_PAUSE)
                    HandleLParam(kbdStruct, true);
                break;
            case WM_KEYUP:
            case WM_SYSKEYUP:
                if (kbdStruct->vkCode == VK_PAUSE){
                    pause = !pause;

                    ResetEvent(hEvent);
                    EnterCriticalSection(&criticalSection);
                    for (auto& pair : keys)
                        pair.second->pressed = false;
                    LeaveCriticalSection(&criticalSection);
                    
                    pressedNow = 0;
                }
                else if (!pause) {
                    HandleLParam(kbdStruct, false);
                }
                break;
            }
        }
    }

    return CallNextHookEx(hhook, nCode, wParam, lParam);
}

void error(const char* format, ...)
{
    char* buffer = new char[MAX_PATH];
    va_list argptr;
    va_start(argptr, format);
    vsprintf(buffer, format, argptr);
    va_end(argptr);

    MessageBoxA(NULL, buffer, PROGRAMNAME, MB_ICONERROR | MB_OK);

    exit(EXIT_FAILURE);
}

void __stdcall SecondThread()
{
    for (;;) {
        if (WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0) {

            EnterCriticalSection(&criticalSection);
            for (const auto& key : keys){
                if (key.second->pressed)
                    key.second->Up();
            }

            for (const auto& key : keys){
                if (key.second->pressed)
                    key.second->Down();
            }
            LeaveCriticalSection(&criticalSection);

            Sleep(SPAMINTERVAL);
        }
        else
            error("Event error, code: %lu\n",GetLastError());
    }
}

int main()
{
    hEvent = CreateEventA(NULL, TRUE, FALSE, TEXT("Local\\SpammyEvent"));
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        error("Already running");

    if (!(hhook = SetWindowsHookEx(WH_KEYBOARD_LL, HookCallback, NULL, 0)))
        error("Error installing keyboard hook");

    if (!(hWinEventHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND , EVENT_SYSTEM_FOREGROUND ,NULL, WinEventProcCallback, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS)))
        error("Error installing window change hook");

    InitializeCriticalSection(&criticalSection);
    HWND hWnd = GetForegroundWindow();
    CheckWindowValid(hWnd);
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)& SecondThread, NULL, 0, NULL);

    for (MSG msg; GetMessage(&msg, NULL, 0, 0) > 0;) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return EXIT_SUCCESS;
}