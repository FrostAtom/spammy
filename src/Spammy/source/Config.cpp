#include "Config.h"

static constexpr DWORD CONFIG_SAVE_DELAY_MS = 10000;
static constexpr const char* CONFIG_TEMP_FILE = "Spammy.json.tmp";

const char* UiSizeName(UiSize size)
{
    static constexpr const char* s_names[UiSize_Count] = {"Small", "Medium", "Large"};
    return s_names[size];
}

float UiSizeFactor(UiSize size)
{
    static constexpr float s_factors[UiSize_Count] = {0.8f, 1.f, 1.25f};
    return s_factors[size];
}

const char* CloseActionName(CloseAction action)
{
    static constexpr const char* s_names[CloseAction_Count] = {"Ask", "Hide", "Exit"};
    return s_names[action];
}

Config& Config::Instance()
{
    static Config s_config;
    return s_config;
}

template <class Bool>
static void TryReadBool(const nlohmann::json& json, const char* key, Bool& out)
{
    if (auto it = json.find(key); it != json.end() && it->is_boolean()) out = it->get<bool>();
}

template <class Enum>
static void TryReadEnum(const nlohmann::json& json, const char* key, Enum& out, int count)
{
    if (auto it = json.find(key); it != json.end() && it->is_number_integer()) {
        const int value = it->get<int>();
        if (value >= 0 && value < count) out = (Enum)value;
    }
}

bool Config::Load()
{
    _lastSaveAttemptTicks = GetTickCount();
    if (!std::filesystem::is_regular_file(CONFIG_FILE)) return true;
    std::ifstream file(CONFIG_FILE);
    if (!file) return false;
    try {
        const nlohmann::json json = nlohmann::json::parse(file);
        // the only read that throws: done before anything is applied, so a refused file leaves the defaults
        ProfileList_t loaded;
        if (auto it = json.find("profiles"); it != json.end() && it->is_array()) it->get_to(loaded);
        TryReadBool(json, "enabled", enabled);
        std::optional<bool> legacyMinimizeToTray;
        TryReadBool(json, "minimizeToTray", legacyMinimizeToTray);
        if (legacyMinimizeToTray) closeAction = *legacyMinimizeToTray ? CloseAction_Hide : CloseAction_Exit;
        TryReadEnum(json, "closeAction", closeAction, CloseAction_Count);
        TryReadBool(json, "soundsEnabled", soundsEnabled);
        TryReadEnum(json, "form", form, KeyboardForm_Count);
        TryReadEnum(json, "variant", variant, KeyboardVariant_Count);
        TryReadEnum(json, "mouse", mouse, MouseForm_Count);
        TryReadEnum(json, "uiSize", uiSize, UiSize_Count);
        profiles = std::move(loaded);
        if (auto it = json.find("editingProfile"); it != json.end() && it->is_string())
            editingProfile = FindProfile(it->get_ref<const std::string&>().c_str());
        if (!editingProfile && !profiles.empty()) editingProfile = profiles.front();
    } catch (nlohmann::json::exception&) {
        return false;
    }
    return true;
}

static bool WriteFileAtomically(const char* path, const char* tempPath, std::string_view data)
{
    HANDLE file = CreateFileA(tempPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool tempOnDisk = WriteFile(file, data.data(), (DWORD)data.size(), &written, nullptr) &&
                            written == data.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (tempOnDisk && MoveFileExA(tempPath, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    DeleteFileA(tempPath);
    return false;
}

void Config::Save()
{
    // cleared before the state is read: a change made meanwhile on the input worker stays dirty
    _dirty = false;
    _lastSaveAttemptTicks = GetTickCount();

    nlohmann::json json = nlohmann::json::object();
    if (!enabled) json["enabled"] = false;
    if (closeAction != CloseAction_Ask) json["closeAction"] = (int)closeAction;
    if (!soundsEnabled) json["soundsEnabled"] = false;
    if (form != KeyboardForm_75) json["form"] = (int)form;
    if (variant != KeyboardVariant_Ansi) json["variant"] = (int)variant;
    if (mouse != MouseForm_5) json["mouse"] = (int)mouse;
    if (uiSize != UiSize_Medium) json["uiSize"] = (int)uiSize;
    if (editingProfile) json["editingProfile"] = editingProfile->name;
    if (!profiles.empty()) json["profiles"] = profiles;

    if (!WriteFileAtomically(CONFIG_FILE, CONFIG_TEMP_FILE, json.dump(2))) _dirty = true;
}

void Config::MarkDirty()
{
    _dirty = true;
}

void Config::SaveIfDirty()
{
    if (_dirty && GetTickCount() - _lastSaveAttemptTicks >= CONFIG_SAVE_DELAY_MS) Save();
}

template <class Pred>
static std::shared_ptr<Profile> FindProfileIf(const Config::ProfileList_t& profiles, Pred pred)
{
    auto it = std::ranges::find_if(profiles, [&](const std::shared_ptr<Profile>& item) { return pred(*item); });
    return it != profiles.end() ? *it : nullptr;
}

std::shared_ptr<Profile> Config::FindProfile(const char* name) const
{
    return FindProfileIf(profiles, [name](const Profile& item) { return item.name == name; });
}

std::shared_ptr<Profile> Config::FindProfileByApp(const char* app) const
{
    return FindProfileIf(profiles, [app](const Profile& item) { return item.apps.contains(std::string_view(app)); });
}

static std::string_view TrimSpaces(std::string_view text)
{
    constexpr std::string_view kSpaces = " \t\r\n";
    const size_t first = text.find_first_not_of(kSpaces);
    if (first == std::string_view::npos) return {};
    return text.substr(first, text.find_last_not_of(kSpaces) - first + 1);
}

static std::string_view CutUtf8(std::string_view text, size_t maxBytes)
{
    if (text.size() <= maxBytes) return text;
    size_t size = maxBytes;
    while (size > 0 && IsUtf8ContinuationByte(text[size]))
        size--;
    return text.substr(0, size);
}

std::string Config::UniqueProfileName(std::string_view base) const
{
    base = TrimSpaces(CutUtf8(TrimSpaces(base), kProfileNameMax));
    if (base.empty()) base = "Game";
    auto isFree = [&](std::string_view name) {
        return std::ranges::none_of(profiles, [&](const std::shared_ptr<Profile>& item) { return item->name == name; });
    };
    if (isFree(base)) return std::string(base);
    for (unsigned n = 2;; n++) {
        const std::string suffix = " " + std::to_string(n);
        std::string name(TrimSpaces(CutUtf8(base, kProfileNameMax - suffix.size())));
        name += suffix;
        if (isFree(name)) return name;
    }
}

std::shared_ptr<Profile> Config::CreateGame(std::string_view name, std::string_view app)
{
    auto profile = std::make_shared<Profile>();
    profile->name = UniqueProfileName(name);
    if (!app.empty()) profile->apps.emplace(std::string(app));
    editingProfile = profiles.emplace_back(std::move(profile));
    MarkDirty();
    return editingProfile;
}

bool Config::RenameProfile(const std::shared_ptr<Profile>& profile, std::string_view name)
{
    name = TrimSpaces(name);
    if (!profile || name.empty() || name.size() > kProfileNameMax) return false;
    if (name == profile->name) return true;
    auto taken = [&](const std::shared_ptr<Profile>& item) { return item != profile && item->name == name; };
    if (std::ranges::any_of(profiles, taken)) return false;
    profile->name = name;
    MarkDirty();
    return true;
}

void Config::SetEditingProfile(const std::shared_ptr<Profile>& profile)
{
    editingProfile = profile;
    MarkDirty();
}

void Config::DeleteProfile(const std::shared_ptr<Profile>& profile)
{
    const auto it = std::ranges::find(profiles, profile);
    if (it == profiles.end()) return;
    if (profile == editingProfile) {
        if (it + 1 != profiles.end())
            editingProfile = *(it + 1);
        else if (it != profiles.begin())
            editingProfile = *(it - 1);
        else
            editingProfile = nullptr;
    }
    profiles.erase(it);
    MarkDirty();
}

void Config::BindProfile(Profile& profile, const std::string& app)
{
    if (profile.apps.emplace(app).second) MarkDirty();
}

void Config::UnbindProfile(Profile& profile, const std::string& app)
{
    if (profile.apps.erase(app)) MarkDirty();
}
