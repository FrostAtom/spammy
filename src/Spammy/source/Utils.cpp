#include "Utils.h"

bool CaseInsensitiveLess::operator()(std::string_view a, std::string_view b) const noexcept
{
    auto lower = [](unsigned char c) { return std::tolower(c); };
    return std::ranges::lexicographical_compare(a, b, {}, lower, lower);
}

void LaunchUrl(const wchar_t* url)
{
    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
}

std::filesystem::path GetModulePath()
{
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, buf, std::size(buf));
    return buf;
}

std::filesystem::path GetProcessPath(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    // LIMITED_INFORMATION is granted for elevated/protected processes where VM_READ (GetModuleFileNameEx) is denied
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};

    wchar_t buf[MAX_PATH] = {0};
    DWORD size = std::size(buf);
    std::filesystem::path result = QueryFullProcessImageNameW(process, 0, buf, &size) ? buf : L"";
    CloseHandle(process);
    return result;
}

std::string Utf8FileName(const std::filesystem::path& path)
{
    std::u8string name = path.filename().u8string();
    return std::string(name.begin(), name.end());
}

BOOL EnumWindows(EnumWindowsProc_t&& func)
{
    struct Thunk {
        static BOOL CALLBACK Call(HWND hwnd, LPARAM lParam) { return (*(EnumWindowsProc_t*)lParam)(hwnd); }
    };
    return EnumWindows(&Thunk::Call, (LPARAM)&func);
}
