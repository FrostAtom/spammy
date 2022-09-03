#include "Utils.h"

void FormatTimeElapsed(char* buf, size_t size, time_t value, time_t current)
{
	using namespace std::chrono;
	auto diff = system_clock::from_time_t(current) - system_clock::from_time_t(value);
	if (diff.count() < 0) { buf[0] = '\0'; return; }
	const char* measure = NULL;
	int _value = 0;
	if ((_value = duration_cast<months>(diff).count()))
		measure = "month";
	else if ((_value = duration_cast<weeks>(diff).count()))
		measure = "week";
	else if ((_value = duration_cast<days>(diff).count()))
		measure = "day";
	else if ((_value = duration_cast<hours>(diff).count()))
		measure = "hour";
	else if ((_value = duration_cast<minutes>(diff).count()))
		measure = "minute";

	if (!measure) {
		strncpy(buf, "just now", size);
		return;
	}

	if (_value == 1)
		snprintf(buf, size, "%s ago", measure);
	else
		snprintf(buf, size, "%d %s%s ago", _value, measure, _value == 1 ? "" : "s");
}

void LaunchUrl(const wchar_t* url)
{
	ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
}

HANDLE OpenProcessByWindow(HWND hwnd, DWORD access)
{
	DWORD id;
	GetWindowThreadProcessId(hwnd, &id);
	return OpenProcess(access, FALSE, id);
}

std::filesystem::path GetProcessPath(HANDLE hProcess)
{
	wchar_t filePath[MAX_PATH] = { 0 };
	if (!GetModuleFileNameExW(hProcess, NULL, filePath, std::size(filePath)))
		return {};
	return filePath;
}

std::filesystem::path GetProcessPath(HWND hwnd)
{
	std::filesystem::path result;
	HANDLE hProc = OpenProcessByWindow(hwnd, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ);
	if (hProc) {
		result = GetProcessPath(hProc);
		CloseHandle(hProc);
	}
	return result;
}

BOOL EnumWindows(EnumWindowsProc_t&& func)
{
	struct Dummy {
	static BOOL CALLBACK EnumWindowProc(HWND hwnd, LPARAM lParam)
	{
		return (*(EnumWindowsProc_t*)lParam)(hwnd);
	}
	};
	return EnumWindows(&Dummy::EnumWindowProc, (LPARAM)&func);
}