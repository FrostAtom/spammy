#include "Utils.h"

bool CaseInsensitiveLess::operator()(std::string_view a, std::string_view b) const noexcept
{
    auto lower = [](unsigned char c) { return std::tolower(c); };
    return std::ranges::lexicographical_compare(a, b, {}, lower, lower);
}

void LaunchUrl(const wchar_t* url)
{
    ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

HICON CreateGrayscaleIcon(HICON source)
{
    ICONINFO info;
    if (!GetIconInfo(source, &info)) return nullptr;

    HICON result = nullptr;
    BITMAP bm;
    if (info.hbmColor && GetObject(info.hbmColor, sizeof(bm), &bm)) {
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = bm.bmWidth;
        bi.bmiHeader.biHeight = -bm.bmHeight; // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;

        HDC dc = GetDC(nullptr);
        void* bits = nullptr;
        HBITMAP color = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
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
        ReleaseDC(nullptr, dc);
    }
    if (info.hbmColor) DeleteObject(info.hbmColor);
    if (info.hbmMask) DeleteObject(info.hbmMask);
    return result;
}

const std::filesystem::path& GetModulePath()
{
    static const std::filesystem::path s_path = [] {
        wchar_t buf[MAX_PATH] = {0};
        GetModuleFileNameW(nullptr, buf, std::size(buf));
        return std::filesystem::path(buf);
    }();
    return s_path;
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

static std::string WideToUtf8(std::wstring_view text)
{
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), result.data(), size, nullptr, nullptr);
    return result;
}

static std::wstring_view TrimWide(std::wstring_view text)
{
    constexpr std::wstring_view kSpaces = L" \t\r\n";
    const size_t first = text.find_first_not_of(kSpaces);
    if (first == std::wstring_view::npos) return {};
    return text.substr(first, text.find_last_not_of(kSpaces) - first + 1);
}

// a non-blank string from the version resource: the block of the first translation it lists, then the US English
// blocks that resources without a translation table usually still have
static std::wstring VersionString(const void* data, const wchar_t* field)
{
    struct LangCodePage {
        WORD lang, codePage;
    };
    boost::container::static_vector<LangCodePage, 3> blocks;
    LangCodePage* translations = nullptr;
    UINT size = 0;
    if (VerQueryValueW(data, L"\\VarFileInfo\\Translation", (void**)&translations, &size) && translations &&
        size >= sizeof(LangCodePage))
        blocks.push_back(translations[0]);
    blocks.push_back({0x0409, 0x04B0});
    blocks.push_back({0x0409, 0x04E4});

    for (const LangCodePage& block : blocks) {
        wchar_t subBlock[96];
        swprintf_s(subBlock, std::size(subBlock), L"\\StringFileInfo\\%04x%04x\\%ls", block.lang, block.codePage, field);
        wchar_t* value = nullptr;
        UINT length = 0; // in characters for string values
        if (!VerQueryValueW(data, subBlock, (void**)&value, &length) || !value || !length) continue;
        if (std::wstring_view text = TrimWide({value, wcsnlen(value, length)}); !text.empty())
            return std::wstring(text);
    }
    return {};
}

std::string AppFriendlyName(const std::filesystem::path& exePath)
{
    DWORD handle = 0;
    if (const DWORD size = GetFileVersionInfoSizeW(exePath.c_str(), &handle)) {
        std::vector<BYTE> data(size);
        if (GetFileVersionInfoW(exePath.c_str(), 0, size, data.data())) {
            for (const wchar_t* field : {L"FileDescription", L"ProductName"})
                if (std::wstring name = VersionString(data.data(), field); !name.empty()) return WideToUtf8(name);
        }
    }
    std::u8string stem = exePath.stem().u8string();
    return std::string(stem.begin(), stem.end());
}

std::vector<RunningApp> ListRunningApps()
{
    // the UI polls this every couple of seconds and reading version resources means disk I/O; an exe's resource
    // doesn't change while it runs. Main thread only
    static std::unordered_map<std::wstring, std::string> s_friendlyNames;

    std::vector<RunningApp> apps;
    boost::container::flat_set<std::string, CaseInsensitiveLess> seen;
    EnumWindows([&](HWND hwnd) -> BOOL {
        if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) || GetWindowTextLengthW(hwnd) == 0) return TRUE;
        if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
        // suspended UWP apps and windows on other virtual desktops are "visible" but cloaked
        DWORD cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) return TRUE;

        const std::filesystem::path path = GetProcessPath(hwnd);
        if (path.empty() || path == GetModulePath()) return TRUE;
        std::string exe = Utf8FileName(path);
        if (!seen.insert(exe).second) return TRUE;

        auto it = s_friendlyNames.find(path.native());
        if (it == s_friendlyNames.end()) it = s_friendlyNames.emplace(path.native(), AppFriendlyName(path)).first;
        apps.push_back({std::move(exe), it->second});
        return TRUE;
    });

    // names can repeat across executables; the exe name breaks the tie so the list doesn't shuffle between polls
    std::ranges::sort(apps, [](const RunningApp& a, const RunningApp& b) {
        CaseInsensitiveLess less;
        if (less(a.name, b.name)) return true;
        if (less(b.name, a.name)) return false;
        return less(a.exe, b.exe);
    });
    return apps;
}

bool BrowseForExe(HWND owner, std::filesystem::path& out)
{
    wchar_t file[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {sizeof(ofn)};
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Programs (*.exe)\0*.exe\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = std::size(file);
    // the config is found relative to the current directory, so the dialog must not move it
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return false;
    out = file;
    return true;
}
