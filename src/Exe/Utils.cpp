#include "Utils.h"

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

void LexicographicalSort(std::vector<std::string>& dict)
{
	std::sort(dict.begin(), dict.end(), [](const std::string& a, const std::string& b) {
		return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](const char& a, const char& b) {
			return tolower(a) < tolower(b);
		});
	});
}