#pragma once
#include "Headers.h"
#define sUpdater Updater::Instance()

class Updater {
    std::jthread _thread;
    // set last by the worker; the strings below are only read after it turns true
    std::atomic<bool> _updateAvailable = false;
    std::string _latestDate;
    std::wstring _releaseUrl;

    Updater() = default;

public:
    ~Updater() = default;
    static Updater& Instance();
    void CheckAsync();
    bool IsUpdateAvailable() const { return _updateAvailable; }
    const char* LatestDate() const { return _latestDate.c_str(); }
    void OpenReleasePage();
};
