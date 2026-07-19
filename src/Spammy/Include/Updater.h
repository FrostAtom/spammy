#pragma once
#include "Headers.h"
#define sUpdater Updater::Instance()

class Updater {
    std::jthread _thread;
    std::atomic<bool> _updateAvailable = false;
    std::string _latestDate;
    std::wstring _releaseUrl;

    Updater() = default;

public:
    ~Updater() = default;
    static Updater& Instance();
    void CheckAsync();
    bool IsUpdateAvailable();
    const char* LatestDate();
    void OpenReleasePage();
};
