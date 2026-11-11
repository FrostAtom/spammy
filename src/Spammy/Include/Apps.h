#pragma once
#include "Headers.h"

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

std::optional<std::filesystem::path> BrowseForExe(HWND owner);
