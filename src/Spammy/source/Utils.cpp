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

HICON CreateGrayscaleIcon(HICON source)
{
    ICONINFO info;
    if (!GetIconInfo(source, &info)) return NULL;

    HICON result = NULL;
    BITMAP bm;
    if (info.hbmColor && GetObject(info.hbmColor, sizeof(bm), &bm)) {
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = bm.bmWidth;
        bi.bmiHeader.biHeight = -bm.bmHeight; // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;

        HDC dc = GetDC(NULL);
        void* bits = NULL;
        HBITMAP color = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
        // 32bpp icons keep their alpha byte through GetDIBits, so the copy only needs its RGB flattened to luma
        if (color && GetDIBits(dc, info.hbmColor, 0, bm.bmHeight, bits, &bi, DIB_RGB_COLORS)) {
            for (uint32_t *px = (uint32_t*)bits, *end = px + bm.bmWidth * bm.bmHeight; px != end; px++) {
                const uint32_t b = *px & 0xFF, g = (*px >> 8) & 0xFF, r = (*px >> 16) & 0xFF;
                const uint32_t y = (r * 77 + g * 150 + b * 29) >> 8;
                *px = (*px & 0xFF000000) | (y << 16) | (y << 8) | y;
            }
            ICONINFO gray = info;
            gray.hbmColor = color;
            result = CreateIconIndirect(&gray);
        }
        if (color) DeleteObject(color);
        ReleaseDC(NULL, dc);
    }
    if (info.hbmColor) DeleteObject(info.hbmColor);
    if (info.hbmMask) DeleteObject(info.hbmMask);
    return result;
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
