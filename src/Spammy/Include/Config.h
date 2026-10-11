#pragma once
#include "Headers.h"
#include "KeyboardLayout.h"
#include "Profile.h"
#define sConfig Config::Instance()

inline constexpr const char* CONFIG_FILE = "Spammy.json";

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

    std::atomic<bool> enabled = true;
    CloseAction closeAction = CloseAction_Ask;
    std::atomic<bool> soundsEnabled = true;
    KeyboardForm form = KeyboardForm_75;
    KeyboardVariant variant = KeyboardVariant_Ansi;
    MouseForm mouse = MouseForm_5;
    UiSize uiSize = UiSize_Medium;
    ProfileList_t profiles;
    std::shared_ptr<Profile> editingProfile;

    static Config& Instance();

    bool Load();
    void Save();
    void MarkDirty();
    void SaveIfDirty();

    std::shared_ptr<Profile> FindProfileByApp(const char* app) const;
    std::shared_ptr<Profile> CreateGame(std::string_view name, std::string_view app);
    bool RenameProfile(const std::shared_ptr<Profile>& profile, std::string_view name);
    void SetEditingProfile(const std::shared_ptr<Profile>& profile);
    void DeleteProfile(const std::shared_ptr<Profile>& profile);
    void BindProfile(Profile& profile, const std::string& app);
    void UnbindProfile(Profile& profile, const std::string& app);

private:
    std::shared_ptr<Profile> FindProfile(const char* name) const;
    std::string UniqueProfileName(std::string_view base) const;

    std::atomic<bool> _dirty = false;
    DWORD _lastSaveAttemptTicks = 0;
};
