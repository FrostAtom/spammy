#include "Config.h"

const char* UiSizeName(UiSize size)
{
    static constexpr const char* s_names[UiSize_Count] = {"SMALL", "MEDIUM", "LARGE"};
    return s_names[size];
}

float UiSizeFactor(UiSize size)
{
    static constexpr float s_factors[UiSize_Count] = {0.8f, 1.f, 1.25f};
    return s_factors[size];
}

Config& Config::GetInstance()
{
    static Config s_config;
    return s_config;
}

// silently keeps the default on a missing or mistyped field
template <class Bool>
static void ReadBool(const nlohmann::json& json, const char* key, Bool& out)
{
    if (auto it = json.find(key); it != json.end() && it->is_boolean()) out = it->get<bool>();
}

template <class Enum>
static void ReadEnum(const nlohmann::json& json, const char* key, Enum& out, int count)
{
    if (auto it = json.find(key); it != json.end() && it->is_number_integer()) {
        int value = it->get<int>();
        if (value >= 0 && value < count) out = (Enum)value;
    }
}

bool Config::Load()
{
    _lastSaveTicks = GetTickCount();
    if (!std::filesystem::is_regular_file(CONFIG_FILE)) return true;
    std::ifstream file(CONFIG_FILE);
    if (!file) return false;
    try {
        const nlohmann::json json = nlohmann::json::parse(file);
        ReadBool(json, "enabled", enabled);
        ReadBool(json, "minimizeToTray", minimizeToTray);
        ReadBool(json, "soundsEnabled", soundsEnabled);
        ReadEnum(json, "form", form, KeyboardForm_Count);
        ReadEnum(json, "variant", variant, KeyboardVariant_Count);
        ReadEnum(json, "mouse", mouse, MouseForm_Count);
        ReadEnum(json, "uiSize", uiSize, UiSize_Count);
        if (auto it = json.find("profiles"); it != json.end() && it->is_array()) it->get_to(profiles);
        if (auto it = json.find("editingProfile"); it != json.end() && it->is_string())
            editingProfile = FindProfile(it->get_ref<const std::string&>().c_str());
    } catch (nlohmann::json::exception&) {
        return false;
    }
    return true;
}

void Config::Save()
{
    std::ofstream file(CONFIG_FILE);
    if (!file) return;

    // defaults are omitted so the file only contains what the user changed
    nlohmann::json json = nlohmann::json::object();
    if (!enabled) json["enabled"] = false;
    if (!minimizeToTray) json["minimizeToTray"] = false;
    if (!soundsEnabled) json["soundsEnabled"] = false;
    if (form != KeyboardForm_75) json["form"] = (int)form;
    if (variant != KeyboardVariant_Ansi) json["variant"] = (int)variant;
    if (mouse != MouseForm_5) json["mouse"] = (int)mouse;
    if (uiSize != UiSize_Medium) json["uiSize"] = (int)uiSize;
    if (editingProfile) json["editingProfile"] = editingProfile->name;
    if (!profiles.empty()) json["profiles"] = profiles;

    file << json.dump(2, ' ');
    _dirty = false;
    _lastSaveTicks = GetTickCount();
}

void Config::MarkDirty()
{
    _dirty = true;
}

void Config::SaveIfDirty()
{
    if (_dirty && GetTickCount() - _lastSaveTicks >= CONFIG_SAVE_DELAY_MS) Save();
}

std::shared_ptr<Profile> Config::FindProfile(const char* name) const
{
    auto it =
        std::ranges::find_if(profiles, [name](const std::shared_ptr<Profile>& item) { return item->name == name; });
    return it != profiles.end() ? *it : nullptr;
}

std::shared_ptr<Profile> Config::FindProfileByApp(const char* app) const
{
    auto it = std::ranges::find_if(
        profiles, [app](const std::shared_ptr<Profile>& item) { return item->apps.contains(std::string_view(app)); });
    return it != profiles.end() ? *it : nullptr;
}

std::shared_ptr<Profile> Config::FindGlobalProfile() const
{
    auto it = std::ranges::find_if(profiles, [](const std::shared_ptr<Profile>& item) { return item->IsGlobal(); });
    return it != profiles.end() ? *it : nullptr;
}

bool Config::IsProfileExists(const char* name) const
{
    return FindProfile(name) != nullptr;
}

void Config::CreateProfile(const char* name)
{
    editingProfile = profiles.emplace_back(std::make_shared<Profile>());
    editingProfile->name = name;
    MarkDirty();
}

void Config::SetEditingProfile(const char* name)
{
    editingProfile = FindProfile(name);
    MarkDirty();
}

void Config::DeleteProfile(const char* name)
{
    std::shared_ptr<Profile> profile = FindProfile(name);
    if (!profile) return;
    if (profile == editingProfile) editingProfile = nullptr;
    profiles.erase(std::ranges::find(profiles, profile));
    MarkDirty();
}

bool Config::IsProfileBinded(const char* name, const char* app) const
{
    std::shared_ptr<Profile> profile = FindProfile(name);
    return profile && profile->apps.contains(std::string_view(app));
}

void Config::BindProfile(const char* name, const char* app)
{
    std::shared_ptr<Profile> profile = FindProfile(name);
    if (profile && profile->apps.emplace(app).second) MarkDirty();
}

void Config::UnbindProfile(const char* name, const char* app)
{
    std::shared_ptr<Profile> profile = FindProfile(name);
    if (!profile) return;
    auto it = profile->apps.find(std::string_view(app));
    if (it == profile->apps.end()) return;
    profile->apps.erase(it);
    MarkDirty();
}
