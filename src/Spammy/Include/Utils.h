#pragma once
#include "Headers.h"
#include <algorithm>
#include <string_view>

// ASCII case-insensitive ordering; transparent so string_view/const char* lookups work in sorted containers
struct CaseInsensitiveLess {
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const noexcept;
};

void LaunchUrl(const wchar_t* url);

std::filesystem::path GetModulePath();
std::filesystem::path GetProcessPath(HWND hwnd);
std::string Utf8FileName(const std::filesystem::path& path);

using EnumWindowsProc_t = std::function<BOOL(HWND)>;
BOOL EnumWindows(EnumWindowsProc_t&& func);
