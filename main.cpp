#include "Key.hpp"
#include "Event.hpp"
#include <Windows.h>
#include <string>
#include <fstream>
#include <regex>
#include <map>
#include <memory>
#define PROGRAM_NAME "Spammy"
#define PROGRAM_CONFIG_PATH "config\\"
#define PROGRAM_CONFIG_EXTENSION ".txt"
#define SPAM_INTERVAL_MS 20


HHOOK hhook;
HANDLE hThread_LoopThread;
HWINEVENTHOOK hWinEventHook;
bool currentStatement = false, pause = false;
std::map<DWORD,std::unique_ptr<Key>> keys;
Event *event;


void ToggleHooks(bool);

void Error(const char* err)
{
    MessageBox(NULL, err, PROGRAM_NAME, MB_ICONERROR | MB_OK);
    std::exit(EXIT_FAILURE);
}

bool IsAnyoneKeyPressed()
{
    for (const auto& key : keys) {
        if (key.second->isPressed)
            return true;
    }

    return false;
}

void HandleKeyEvent(LPKBDLLHOOKSTRUCT lpkbdStruct, bool isPress)
{
    if (auto iter = keys.find(lpkbdStruct->vkCode); iter != keys.end()) {
        if (iter->second->isPressed != isPress) {
            iter->second->isPressed = isPress;

            if (!pause) {
                if (isPress)
                    event->Unlock();
                else if (!IsAnyoneKeyPressed())
                    event->Lock();
            }
        }
    }
}


LRESULT __stdcall KeyboardHookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION){
        auto lpkbdStruct = (LPKBDLLHOOKSTRUCT)lParam;
        if (!(lpkbdStruct->dwExtraInfo & ITSFAKE)){
            switch(wParam){
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                if (lpkbdStruct->vkCode != VK_PAUSE)
                    HandleKeyEvent(lpkbdStruct,true);
                break;
            case WM_KEYUP:
            case WM_SYSKEYUP:
                if (lpkbdStruct->vkCode == VK_PAUSE) {
                    pause = !pause;
                    if (pause)
                        event->Lock();
                    else if (IsAnyoneKeyPressed())
                        event->Unlock();
                }
                else
                    HandleKeyEvent(lpkbdStruct,false);

                break;
            }
        }
    }

    return CallNextHookEx(hhook, nCode, wParam, lParam);
}

void __stdcall LoopThreadControl()
{
    while (event->Wait()) {
        for (const auto& key : keys) {
            if (key.second->isPressed) 
                key.second->Click();
        }

        Sleep(SPAM_INTERVAL_MS);
    }

    Error("WaitForSingleObject(): fail");
}

void ToggleHooks(bool newStatement)
{
    if (currentStatement == newStatement) return;

    if (newStatement) {
        if (!(hhook = SetWindowsHookExA(WH_KEYBOARD_LL, KeyboardHookCallback, NULL, 0)))
            Error("SetWindowsHookEx(): fail");

        if (!(hThread_LoopThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)& LoopThreadControl, NULL, 0, NULL)))
            Error("CreateThread(): fail");
    }
    else {
        if (!UnhookWindowsHookEx(hhook))
            Error("UnhookWindowsHookEx(): fail");

        if (!TerminateThread(hThread_LoopThread, 0))
            Error("TerminateThread(): fail");
    }

    currentStatement = newStatement;
}

void ReadKeysFromFile(std::ifstream& file)
{    
    const static std::regex pattern("[0[Xx]]?([0-9a-fA-F]{2,4})");
    std::smatch matchResult;

    for (std::string line; std::getline(file,line);) {
        if (line.empty()) continue;

        if (std::regex_search(line, matchResult, pattern) && !matchResult.empty()) {
            auto matchResult1 = matchResult.str(1);
            if (DWORD vkCode = std::strtoul(matchResult1.c_str(),NULL,16)) {
                if (auto iter = keys.find(vkCode); iter == keys.end())
                    keys[vkCode] = std::unique_ptr<Key>(new Key(vkCode));
            }
        }
    }
}

void ForegroundWindowUpdate()
{
    static HWND hwndLast;
    HWND hwndCurrent = GetForegroundWindow();
    if (hwndCurrent == hwndLast) return;
    hwndLast = hwndCurrent;


    event->Lock();
    keys.clear();

    char* windowName = new char[MAX_PATH];
    if (GetWindowText(hwndCurrent,windowName,MAX_PATH) > 0) {
        char* cfgFileName = new char[MAX_PATH];
        sprintf(cfgFileName,"%s%s%s",PROGRAM_CONFIG_PATH,windowName,PROGRAM_CONFIG_EXTENSION);

        std::ifstream cfgFile(cfgFileName);
        if (cfgFile.is_open())
            ReadKeysFromFile(cfgFile);

        delete[] cfgFileName;
    }

    ToggleHooks(!keys.empty());

    delete[] windowName;
}

VOID CALLBACK WinEventProcCallback(HWINEVENTHOOK, DWORD dwEvent, HWND hwnd, LONG, LONG, DWORD, DWORD)
{
    if (dwEvent == EVENT_SYSTEM_FOREGROUND)
        ForegroundWindowUpdate();
}

int main(int argc, char* argv[])
{
    try {
        event = new Event;
        auto errCode = event->Initialize("Local\\" PROGRAM_NAME "_event");
        switch(errCode) {
        case 0:
            break;
        case ERROR_ALREADY_EXISTS:
            throw "Alredy launched";
            break;
        default:
            throw "CreateEventA(): fail";
            break;
        }

        if (!(hWinEventHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,NULL,WinEventProcCallback,0,0,WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS)))
            throw "SetWinEventHook(): fail";
        

        ForegroundWindowUpdate();

        for (MSG msg; GetMessage(&msg, NULL, 0, 0) > 0; );
    }
    catch (const char* err){
        Error(err);
    }


    return EXIT_SUCCESS;
}