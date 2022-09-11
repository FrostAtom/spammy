#include "WinEvent.h"

WinEvent::~WinEvent() { reset(); }

WinEvent& WinEvent::instance()
{
    static std::unique_ptr<WinEvent> winEvent(new WinEvent());
    return *winEvent;
}

bool WinEvent::on(DWORD event, Callback_t&& func)
{
    if (_events.contains(event)) return false;
    HWINEVENTHOOK hEvent = SetWinEventHook(event, EVENT_SYSTEM_FOREGROUND, NULL, &WinEvent::HandleWinEvent, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    if (!hEvent) return false;
    _events.emplace(event, std::tuple<HWINEVENTHOOK, Callback_t>(hEvent, std::forward<Callback_t>(func)));
    return true;
}

bool WinEvent::on(DWORD event, PureCallback_t&& func)
{
    return on(event, [func = std::forward<PureCallback_t>(func)](DWORD, HWND, LONG, LONG, DWORD, DWORD) { func(); });
}

void WinEvent::off(DWORD event)
{
    auto it = _events.find(event);
    if (it == _events.end()) return;
    UnhookWinEvent(std::get<0>(it->second));
    _events.erase(it);
}

bool WinEvent::state(DWORD event)
{
    return _events.contains(event);
}

void WinEvent::reset()
{
    for (auto& [event, tuple] : _events)
        UnhookWinEvent(std::get<0>(tuple));
    _events.clear();
}

VOID CALLBACK WinEvent::HandleWinEvent(HWINEVENTHOOK hEvent, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime)
{
    auto& self = sWinEvent;
    auto it = self._events.find(event);
    if (it == self._events.end()) return;
    (std::get<1>(it->second))(event, hwnd, idObject, idChild, idEventThread, dwmsEventTime);
}