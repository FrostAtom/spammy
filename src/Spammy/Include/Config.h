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

    static Config& Instance();

    bool Load();
    void Save();
    void MarkDirty();
    void SaveIfDirty();

    // a profile is a game: it only ever applies while one of its apps is in the foreground
    std::shared_ptr<Profile> FindProfile(const char* name) const;
    std::shared_ptr<Profile> FindProfileByApp(const char* app) const;
    bool IsProfileExists(const char* name) const;
    // base, or "base 2", "base 3"... whichever is free (except may keep its own name); cut to kProfileNameMax
    std::string UniqueProfileName(std::string_view base, const Profile* except = nullptr) const;
    // appends a new game bound to app (empty app: no apps yet), named UniqueProfileName(name); copyFrom, when given,
    // donates its keys, speed, pause key and locks. Becomes the editing profile
    std::shared_ptr<Profile> CreateGame(std::string_view name, std::string_view app, const Profile* copyFrom);
    // false (nothing changed) when the trimmed name is empty, too long or taken by another profile
    bool RenameProfile(const std::shared_ptr<Profile>& profile, std::string_view name);
    void SetEditingProfile(const char* name);
    // the editing profile moves to its neighbour (the next one, else the previous one, else none)
    void DeleteProfile(const char* name);
    void BindProfile(const char* name, const char* app);
    void UnbindProfile(const char* name, const char* app);

private:
    std::atomic<bool> _dirty = false;
    DWORD _lastSaveTicks = 0;
};
