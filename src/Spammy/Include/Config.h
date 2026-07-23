#pragma once
#include "Headers.h"
#include "KeyboardLayout.h"
#include "Profile.h"
#define CONFIG_FILE "Spammy.json"
#define CONFIG_SAVE_DELAY_MS 10000
#define sConfig Config::GetInstance()

struct Config {
    std::atomic<bool> enabled = true;
    bool minimizeToTray = true;
    bool showTrayIcon = true;
    std::atomic<bool> soundsEnabled = true;
    KeyboardForm form = KeyboardForm_75;
    KeyboardVariant variant = KeyboardVariant_Ansi;
    MouseForm mouse = MouseForm_5;
    std::list<std::shared_ptr<Profile>> profiles;
    std::shared_ptr<Profile> editingProfile;

    static Config& GetInstance();

    bool Load();
    void Save();
    void MarkDirty();
    void SaveIfDirty(DWORD ticks);

    std::shared_ptr<Profile> FindProfile(const char* name);
    std::shared_ptr<Profile> FindProfileByApp(const char* app);
    bool IsProfileExists(const char* name);
    void CreateProfile(const char* name);
    void SetEditingProfile(const char* name);
    void DeleteProfile(const char* name);
    bool IsProfileBinded(const char* name, const char* app);
    void BindProfile(const char* name, const char* app);
    void UnbindProfile(const char* name, const char* app);

private:
    std::atomic<bool> _dirty = false;
    DWORD _lastSaveTicks = 0;
};
