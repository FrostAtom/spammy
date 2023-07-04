#pragma once
#include "Headers.h"

void LaunchUrl(const wchar_t* url);

HANDLE OpenProcessByWindow(HWND hwnd, DWORD access);
std::filesystem::path GetProcessPath(HANDLE hProcess);
std::filesystem::path GetProcessPath(HWND hwnd);

using EnumWindowsProc_t = std::function<BOOL(HWND)>;
BOOL EnumWindows(EnumWindowsProc_t&& func);

void LexicographicalSort(std::vector<std::string>& dict);