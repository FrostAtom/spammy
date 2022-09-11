#pragma once
#include <Windows.h>
#include <Shlwapi.h>
#include <shellapi.h>
#include <psapi.h>
#include <functional>
#include <filesystem>
#include <chrono>

void LaunchUrl(const wchar_t* url);

HANDLE OpenProcessByWindow(HWND hwnd, DWORD access);
std::filesystem::path GetProcessPath(HANDLE hProcess);
std::filesystem::path GetProcessPath(HWND hwnd);

using EnumWindowsProc_t = std::function<BOOL(HWND)>;
BOOL EnumWindows(EnumWindowsProc_t&& func);

void LexicographicalSort(std::vector<std::string>& dict);