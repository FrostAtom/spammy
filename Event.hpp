#pragma once
#include <Windows.h>
#include <cstdio>


class Event {
public:
	DWORD Initialize(const char* name)
	{
		SetLastError(0);

		hEvent = CreateEventA(NULL, TRUE, FALSE, name);
		auto errCode = GetLastError();

		if (errCode == ERROR_ALREADY_EXISTS || !hEvent)
			return errCode;

		isLocked = true;

		return 0;
	}

	void Lock()
	{
		if (isLocked) return;
		ResetEvent(hEvent);
		isLocked = true;
	}

	void Unlock()
	{
		if (!isLocked) return;
		SetEvent(hEvent);
		isLocked = false;
	}

	bool Wait()
	{
		return WaitForSingleObject(hEvent,INFINITE) == WAIT_OBJECT_0;
	}
private:
	bool isLocked;
	HANDLE hEvent;
};