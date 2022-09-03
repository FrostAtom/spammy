#pragma once
#include <Windows.h>
#include <functional>
#include <map>
#define sWinEvent WinEvent::instance()

class WinEvent {
    using Callback_t = std::function<void(DWORD, HWND, LONG, LONG, DWORD, DWORD)>;
    using PureCallback_t = std::function<void()>;
    std::map<DWORD, std::tuple<HWINEVENTHOOK, Callback_t>> _events;

    WinEvent() {}
    WinEvent(const WinEvent&) = delete;
    WinEvent& operator=(const WinEvent&) = delete;

public:
    ~WinEvent();
    static WinEvent& instance();
    bool on(DWORD event, Callback_t&& func);
    bool on(DWORD event, PureCallback_t&& func);
    void off(DWORD event);
    bool state(DWORD event);
    void reset();

private:
    static VOID CALLBACK HandleWinEvent(HWINEVENTHOOK hEvent, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime);
};