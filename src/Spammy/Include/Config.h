#pragma once
#include "Headers.h"
#include "KeyboardLayout.h"
#include "Profile.h"
#define sConfig Config::GetInstance()

inline constexpr const char* CONFIG_FILE = "Spammy.json";
inline constexpr DWORD CONFIG_SAVE_DELAY_MS = 10000;

enum UiSize {
    UiSize_Small,
    UiSize_Medium,
    UiSize_Large,
    UiSize_Count,
};

enum CloseAction {
    CloseAction_Ask,
    CloseAction_Hide,
    CloseAction_Exit,
    CloseAction_Count,
};

const char* UiSizeName(UiSize size);
float UiSizeFactor(UiSize size);
const char* CloseActionName(CloseAction action);

struct Config {
    using ProfileList_t = boost::container::small_vector<std::shared_ptr<Profile>, 4>;

    // toggled from the input worker, read on the hook thread — hence atomic
    std::atomic<bool> enabled = true;
    CloseAction closeAction = CloseAction_Ask;
    std::atomic<bool> soundsEnabled = true;
    KeyboardForm form = KeyboardForm_75;
    KeyboardVariant variant = KeyboardVariant_Ansi;
    MouseForm mouse = MouseForm_5;
    UiSize uiSize = UiSize_Medium;
    ProfileList_t profiles;
    std::shared_ptr<Profile> editingProfile;

    static Config& GetInstance();

    bool Load();
    void Save();
    void MarkDirty();
    void SaveIfDirty();

    std::shared_ptr<Profile> FindProfile(const char* name) const;
    std::shared_ptr<Profile> FindProfileByApp(const char* app) const;
    std::shared_ptr<Profile> FindGlobalProfile() const;
    bool IsProfileExists(const char* name) const;
    void CreateProfile(const char* name);
    void SetEditingProfile(const char* name);
    void DeleteProfile(const char* name);
    bool IsProfileBinded(const char* name, const char* app) const;
    void BindProfile(const char* name, const char* app);
    void UnbindProfile(const char* name, const char* app);

private:
    std::atomic<bool> _dirty = false;
    DWORD _lastSaveTicks = 0;
};
