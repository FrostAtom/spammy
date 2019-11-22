#include <Windows.h>
#include <map>
#include <algorithm>
#include <fstream>
#include <string>
#include <regex>
#include "key.hpp"
#define INTERVAL_MS 20
#define PROGRAMNAME "spammy"
#define CFGPATH "config/"
#define CFGEXTENSION ".txt"


HHOOK hhook;
HANDLE hEvent;
bool eventLocked = true;
CRITICAL_SECTION criticalSection;
bool validWindow = false, pause = false;
std::map<DWORD, Key*> keysData;


bool AnyoneKeyIsPressed()
{
    for (const auto& pair : keysData){
        if (pair.second->pressed)
            return true;
    }

    return false;
}

void ResetKeysMap(bool clean = false)
{
    if (keysData.empty()) return;

    ResetEvent(hEvent);
    eventLocked = true;

    EnterCriticalSection(&criticalSection);
    if (clean){
        for (auto& pair : keysData)
            delete pair.second;
        keysData.clear();
    }
    else{
        for (auto& pair : keysData)
            pair.second->pressed = false;
    }
    LeaveCriticalSection(&criticalSection);
}

bool FillKeysMapFromConfigFile(const char* name)
{
    std::string path;
    path.reserve(MAX_PATH);
    path.append(CFGPATH).append(name).append(CFGEXTENSION);

    std::fstream file(path.c_str(), std::ios::in);
    if (file.is_open()){
        DWORD vkCode = NULL;
        const std::regex pattern("(0[xX][0-9a-fA-F]+)");
        std::smatch match_result;

        EnterCriticalSection(&criticalSection);
        for(std::string line; getline(file, line);) {
            if (!line.empty() && std::regex_search(line, match_result, pattern) && !match_result.empty()){
                vkCode = std::strtoul(match_result.str(1).c_str(), NULL, 16);
                if (vkCode)
                    keysData[vkCode] = new Key(vkCode);
            }
        }
        LeaveCriticalSection(&criticalSection);
        file.close();
        
        return !keysData.empty();
    }

    return false;
}

void CheckForegroundWindow()
{
    static HWND hWndCurrent = NULL;

    HWND hWndForeground = GetForegroundWindow();
    if (hWndForeground != hWndCurrent){
        hWndCurrent = hWndForeground;

        char* buffer = new char[MAX_PATH];
        if (GetWindowTextA(hWndForeground, buffer, MAX_PATH) > 0){
            ResetKeysMap(true);
            validWindow = FillKeysMapFromConfigFile(buffer);
        }
        else
            validWindow = false;

        delete[] buffer;
    }
}

VOID CALLBACK WinEventProcCallback(HWINEVENTHOOK, DWORD dwEvent, HWND hwnd, LONG, LONG, DWORD, DWORD)
{
    if (dwEvent == EVENT_SYSTEM_FOREGROUND)
        CheckForegroundWindow();
}

void HandlePress(LPKBDLLHOOKSTRUCT kbdStruct, bool pressed)
{
    if (auto pos = keysData.find(kbdStruct->vkCode); pos != keysData.end()) {
        if (pos->second->pressed != pressed){
            //EnterCriticalSection(&criticalSection);
            pos->second->pressed = pressed;
            //LeaveCriticalSection(&criticalSection);

            if (pressed){
                if (eventLocked){
                    eventLocked = false;
                    SetEvent(hEvent);
                }
            }
            else{
                if (!AnyoneKeyIsPressed() && !eventLocked){
                    eventLocked = true;
                    ResetEvent(hEvent);
                }
            }
        }
    }
}

LRESULT __stdcall HookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
    static LPKBDLLHOOKSTRUCT lpkbdStruct;
    if (nCode == HC_ACTION && validWindow){
        lpkbdStruct = (LPKBDLLHOOKSTRUCT)lParam;
        if (!(lpkbdStruct->dwExtraInfo & ITSFAKE)){
            switch(wParam){
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                if (!pause && lpkbdStruct->vkCode != VK_PAUSE)
                    HandlePress(lpkbdStruct, true);
                break;
            case WM_KEYUP:
            case WM_SYSKEYUP:
                if (lpkbdStruct->vkCode == VK_PAUSE){
                    pause = !pause;

                    ResetKeysMap();
                }
                else if (!pause)
                    HandlePress(lpkbdStruct, false);

                break;
            }
        }
    }

    return CallNextHookEx(hhook, nCode, wParam, lParam);
}

void __stdcall SecondThread()
{
    for (;;) {
        if (WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0) {

            EnterCriticalSection(&criticalSection);
            for (const auto& key : keysData){
                if (key.second->pressed)
                    key.second->Up();
            }

            for (const auto& key : keysData){
                if (key.second->pressed)
                    key.second->Down();
            }
            LeaveCriticalSection(&criticalSection);

            Sleep(INTERVAL_MS);
        }
        else
            throw "WaitForSingleObject fail";
    }
}

int main()
{
    try {
        hEvent = CreateEventA(NULL, TRUE, FALSE, TEXT("Local\\" PROGRAMNAME "Event"));
        if (GetLastError() == ERROR_ALREADY_EXISTS)
            throw "Alredy running";

        if (hhook = SetWindowsHookEx(WH_KEYBOARD_LL, HookCallback, NULL, 0); !hhook)
            throw "SetWindowsHookEx fail";

        if (HWINEVENTHOOK hWinEventHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, WinEventProcCallback, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS); !hWinEventHook)
            throw "SetWinEventHook fail";

        InitializeCriticalSection(&criticalSection);

        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)& SecondThread, NULL, 0, NULL);

        CheckForegroundWindow();
        for (MSG msg; GetMessage(&msg, NULL, 0, 0) > 0;);
    }
    catch (const char* e){
        MessageBox(NULL, e, PROGRAMNAME, MB_ICONERROR | MB_OK);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}