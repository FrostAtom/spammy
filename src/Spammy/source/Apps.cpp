#include "Apps.h"
#include "Text.h"
#include <commdlg.h>
#include <fmt/xchar.h>
#include <shlobj_core.h>

static constexpr size_t kWindowsMaxPathChars = 32767;

void LaunchUrl(const wchar_t* url)
{
    ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

static uint32_t GrayscalePixel(uint32_t argb)
{
    const uint32_t b = argb & 0xFF;
    const uint32_t g = (argb >> 8) & 0xFF;
    const uint32_t r = (argb >> 16) & 0xFF;
    const uint32_t luma = (r * 77 + g * 150 + b * 29) >> 8;
    const uint32_t alpha = argb & 0xFF000000;
    return alpha | (luma << 16) | (luma << 8) | luma;
}

static BITMAPINFO TopDown32BitDib(int width, int height)
{
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    return bi;
}

HICON CreateGrayscaleIcon(HICON source)
{
    ICONINFO info;
    if (!GetIconInfo(source, &info)) return nullptr;

    HICON result = nullptr;
    BITMAP bm;
    if (info.hbmColor && GetObject(info.hbmColor, sizeof(bm), &bm)) {
        BITMAPINFO bi = TopDown32BitDib(bm.bmWidth, bm.bmHeight);
        HDC dc = GetDC(nullptr);
        void* bits = nullptr;
        HBITMAP color = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (color && GetDIBits(dc, info.hbmColor, 0, bm.bmHeight, bits, &bi, DIB_RGB_COLORS)) {
            for (uint32_t *px = (uint32_t*)bits, *end = px + bm.bmWidth * bm.bmHeight; px != end; px++)
                *px = GrayscalePixel(*px);
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
        std::wstring buf(MAX_PATH, L'\0');
        for (;;) {
            const DWORD length = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
            const bool truncated = length >= buf.size();
            if (!truncated) {
                buf.resize(length);
                return std::filesystem::path(std::move(buf));
            }
            buf.resize(buf.size() * 2);
        }
    }();
    return s_path;
}

std::filesystem::path GetProcessPath(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    // unlike PROCESS_VM_READ, granted even for elevated and protected processes
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};

    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        auto size = (DWORD)buf.size();
        if (QueryFullProcessImageNameW(process, 0, buf.data(), &size)) {
            buf.resize(size);
            break;
        }
        const bool bufferTooSmall = GetLastError() == ERROR_INSUFFICIENT_BUFFER;
        if (!bufferTooSmall || buf.size() > kWindowsMaxPathChars) {
            buf.clear();
            break;
        }
        buf.resize(buf.size() * 2);
    }
    CloseHandle(process);
    return buf;
}

// not path::u8string(): it throws on the lone surrogates NTFS allows in names
std::string Utf8FileName(const std::filesystem::path& path)
{
    return una::utf16to8(path.filename().native());
}

static std::wstring VersionString(const void* data, const wchar_t* field)
{
    struct LangCodePage {
        WORD lang, codePage;
    };
    constexpr LangCodePage kUsEnglishUtf16 = {0x0409, 0x04B0};
    constexpr LangCodePage kUsEnglishCp1252 = {0x0409, 0x04E4};

    boost::container::static_vector<LangCodePage, 3> blocks;
    LangCodePage* translations = nullptr;
    UINT size = 0;
    if (VerQueryValueW(data, L"\\VarFileInfo\\Translation", (void**)&translations, &size) && translations &&
        size >= sizeof(LangCodePage))
        blocks.push_back(translations[0]);
    blocks.push_back(kUsEnglishUtf16);
    blocks.push_back(kUsEnglishCp1252);

    for (const LangCodePage& block : blocks) {
        wchar_t subBlock[96];
        const auto written =
            fmt::format_to_n(subBlock, std::size(subBlock) - 1, L"\\StringFileInfo\\{:04x}{:04x}\\{}", block.lang,
                             block.codePage, field);
        *written.out = L'\0';
        wchar_t* value = nullptr;
        UINT lengthInChars = 0;
        if (!VerQueryValueW(data, subBlock, (void**)&value, &lengthInChars) || !value || !lengthInChars) continue;
        if (const std::wstring_view text = TrimSpaces(std::wstring_view(value, wcsnlen(value, lengthInChars)));
            !text.empty())
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
                if (const std::wstring name = VersionString(data.data(), field); !name.empty())
                    return una::utf16to8(name);
        }
    }
    return una::utf16to8(exePath.stem().native());
}

static std::optional<IconImage> IconToImage(HICON icon)
{
    ICONINFO info;
    if (!GetIconInfo(icon, &info)) return std::nullopt;

    std::optional<IconImage> image;
    BITMAP bm;
    if (info.hbmColor && GetObject(info.hbmColor, sizeof(bm), &bm)) {
        BITMAPINFO bi = TopDown32BitDib(bm.bmWidth, bm.bmHeight);
        const size_t pixelCount = (size_t)bm.bmWidth * bm.bmHeight;
        std::vector<uint32_t> argb(pixelCount);
        std::vector<uint32_t> mask(pixelCount);
        HDC dc = GetDC(nullptr);
        const bool read = GetDIBits(dc, info.hbmColor, 0, bm.bmHeight, argb.data(), &bi, DIB_RGB_COLORS) &&
                          GetDIBits(dc, info.hbmMask, 0, bm.bmHeight, mask.data(), &bi, DIB_RGB_COLORS);
        ReleaseDC(nullptr, dc);

        if (read) {
            const bool hasAlphaChannel = std::ranges::any_of(argb, [](uint32_t px) { return (px >> 24) != 0; });
            image = IconImage{bm.bmWidth, bm.bmHeight, std::vector<ImU32>(pixelCount)};
            for (size_t i = 0; i < pixelCount; i++) {
                const uint32_t px = argb[i];
                uint32_t alpha = px >> 24;
                if (!hasAlphaChannel) {
                    const bool maskedOut = (mask[i] & 0xFFFFFF) != 0;
                    alpha = maskedOut ? 0 : 0xFF;
                }
                image->pixels[i] = IM_COL32((px >> 16) & 0xFF, (px >> 8) & 0xFF, px & 0xFF, alpha);
            }
        }
    }
    if (info.hbmColor) DeleteObject(info.hbmColor);
    if (info.hbmMask) DeleteObject(info.hbmMask);
    return image;
}

std::optional<IconImage> ExtractExeIcon(const std::filesystem::path& exePath, int sizePx)
{
    HICON icon = nullptr;
    const UINT largeAndSmallSize = MAKELONG(sizePx, sizePx);
    if (SHDefExtractIconW(exePath.c_str(), 0, 0, &icon, nullptr, largeAndSmallSize) != S_OK || !icon)
        return std::nullopt;
    std::optional<IconImage> image = IconToImage(icon);
    DestroyIcon(icon);
    return image;
}

static bool IsCloaked(HWND hwnd)
{
    DWORD cloaked = 0;
    return SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked;
}

static bool IsAppWindow(HWND hwnd)
{
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) || GetWindowTextLengthW(hwnd) == 0) return false;
    if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return false;
    return !IsCloaked(hwnd);
}

std::vector<RunningApp> ListRunningApps()
{
    static boost::unordered_flat_map<std::wstring, std::string> s_friendlyNameCache; // main thread only

    std::vector<RunningApp> apps;
    boost::container::flat_set<std::string, CaseInsensitiveLess> seen;
    auto visit = [&](HWND hwnd) -> BOOL {
        if (!IsAppWindow(hwnd)) return TRUE;

        std::filesystem::path path = GetProcessPath(hwnd);
        if (path.empty() || path == GetModulePath()) return TRUE;
        std::string exe = Utf8FileName(path);
        if (!seen.insert(exe).second) return TRUE;

        auto it = s_friendlyNameCache.find(path.native());
        if (it == s_friendlyNameCache.end())
            it = s_friendlyNameCache.emplace(path.native(), AppFriendlyName(path)).first;
        apps.push_back({std::move(exe), it->second, std::move(path)});
        return TRUE;
    };
    using Visit = decltype(visit);
    struct Thunk {
        static BOOL CALLBACK Call(HWND hwnd, LPARAM lParam) { return (*(Visit*)lParam)(hwnd); }
    };
    EnumWindows(&Thunk::Call, (LPARAM)&visit);

    auto byNameThenExe = [](const RunningApp& a, const RunningApp& b) {
        const CaseInsensitiveLess less{};
        if (less(a.friendlyName, b.friendlyName)) return true;
        if (less(b.friendlyName, a.friendlyName)) return false;
        return less(a.exeName, b.exeName);
    };
    std::ranges::sort(apps, byNameThenExe);
    return apps;
}

std::optional<std::filesystem::path> BrowseForExe(HWND owner)
{
    std::wstring file(kWindowsMaxPathChars + 1, L'\0');
    OPENFILENAMEW ofn = {sizeof(ofn)};
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Programs (*.exe)\0*.exe\0";
    ofn.lpstrFile = file.data();
    ofn.nMaxFile = (DWORD)file.size();
    // the config is opened relative to the current directory
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return std::nullopt;
    return file.c_str();
}
