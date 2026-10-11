#pragma once
#include "Headers.h"
#define sUpdater Updater::Instance()

class Updater {
    struct Result {
        // set last by the worker: the strings below are valid only once it's true
        std::atomic<bool> updateAvailable = false;
        std::string latestDate;
        std::wstring releaseUrl;
    };
    // shared with the detached worker, which exit doesn't wait for and which may outlive the Updater
    std::shared_ptr<Result> _result = std::make_shared<Result>();
    bool _started = false;

    Updater() = default;

public:
    static Updater& Instance();
    void CheckAsync();
    bool IsUpdateAvailable() const { return _result->updateAvailable; }
    const char* LatestDate() const { return _result->latestDate.c_str(); }
    void OpenReleasePage();
};
