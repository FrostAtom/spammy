#pragma once
#include "Headers.h"

struct CaseInsensitiveLess {
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const noexcept;
};

template <class T, unsigned N>
class SpscQueue {
    static_assert(std::has_single_bit(N), "free-running indices must wrap cleanly modulo N");

    std::array<T, N> _items = {};
    std::atomic<unsigned> _head = 0;
    std::atomic<unsigned> _tail = 0;

public:
    bool Push(const T& item)
    {
        const unsigned head = _head.load(std::memory_order_relaxed);
        const bool full = head - _tail.load(std::memory_order_acquire) == N;
        if (full) return false;
        _items[head % N] = item;
        _head.store(head + 1, std::memory_order_release);
        return true;
    }

    void Drain(auto&& consume)
    {
        const unsigned head = _head.load(std::memory_order_acquire);
        unsigned tail = _tail.load(std::memory_order_relaxed);
        for (; tail != head; tail++)
            consume(_items[tail % N]);
        _tail.store(tail, std::memory_order_release);
    }
};

constexpr bool IsUtf8ContinuationByte(char c)
{
    return ((unsigned char)c & 0xC0) == 0x80;
}
std::string WideToUtf8(std::wstring_view text);
std::wstring Utf8ToWide(std::string_view text);

void LaunchUrl(const wchar_t* url);
HICON CreateGrayscaleIcon(HICON source);

const std::filesystem::path& GetModulePath();
std::filesystem::path GetProcessPath(HWND hwnd);
std::string Utf8FileName(const std::filesystem::path& path);

std::string AppFriendlyName(const std::filesystem::path& exePath);

struct IconImage {
    int width = 0, height = 0;
    std::vector<ImU32> pixels;
};
std::optional<IconImage> ExtractExeIcon(const std::filesystem::path& exePath, int sizePx);

struct RunningApp {
    std::string exeName;
    std::string friendlyName;
    std::filesystem::path exePath;
};
std::vector<RunningApp> ListRunningApps();

bool BrowseForExe(HWND owner, std::filesystem::path& out);
