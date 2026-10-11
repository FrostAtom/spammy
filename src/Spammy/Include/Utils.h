#pragma once
#include "Headers.h"

// ASCII case-insensitive ordering; transparent so string_view/const char* lookups work in sorted containers
struct CaseInsensitiveLess {
    using is_transparent = void;
    bool operator()(std::string_view a, std::string_view b) const noexcept;
};

// lock-free single-producer/single-consumer ring: neither side ever blocks, so the producer may be a hook thread
template <class T, unsigned N>
class SpscQueue {
    static_assert(std::has_single_bit(N), "free-running indices must wrap cleanly modulo N");

    std::array<T, N> _items = {};
    std::atomic<unsigned> _head = 0; // written by the producer only
    std::atomic<unsigned> _tail = 0; // written by the consumer only

public:
    // producer side; false (item dropped) when the consumer has fallen N items behind
    bool Push(const T& item)
    {
        const unsigned head = _head.load(std::memory_order_relaxed);
        if (head - _tail.load(std::memory_order_acquire) == N) return false;
        _items[head % N] = item;
        _head.store(head + 1, std::memory_order_release);
        return true;
    }

    // consumer side
    void Drain(auto&& consume)
    {
        const unsigned head = _head.load(std::memory_order_acquire);
        unsigned tail = _tail.load(std::memory_order_relaxed);
        for (; tail != head; tail++)
            consume(_items[tail % N]);
        _tail.store(tail, std::memory_order_release);
    }
};

void LaunchUrl(const wchar_t* url);
// desaturated copy of an icon (alpha preserved); the caller owns the result, NULL on failure
HICON CreateGrayscaleIcon(HICON source);

const std::filesystem::path& GetModulePath(); // cached: the loader-recorded path is fixed for the process lifetime
std::filesystem::path GetProcessPath(HWND hwnd);
std::string Utf8FileName(const std::filesystem::path& path);

using EnumWindowsProc_t = std::function<BOOL(HWND)>;
BOOL EnumWindows(EnumWindowsProc_t&& func);

// what a person would call the program: the version resource's FileDescription, else its ProductName, else the
// file name without ".exe"; UTF-8
std::string AppFriendlyName(const std::filesystem::path& exePath);

struct RunningApp {
    std::string exe;  // file name as Utf8FileName gives it, e.g. "Wow.exe"
    std::string name; // AppFriendlyName
};
// one entry per executable that owns a visible, unowned, titled top-level window, Spammy itself left out;
// sorted by name, case-insensitively
std::vector<RunningApp> ListRunningApps();

// "Open" dialog filtered to *.exe; false when cancelled
bool BrowseForExe(HWND owner, std::filesystem::path& out);
