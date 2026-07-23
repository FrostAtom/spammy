#include "Config.h"
#include "Utils.h"

Config& Config::GetInstance()
{
    static Config s_config;
    return s_config;
}

bool Config::Load()
{
    _lastSaveTicks = GetTickCount();
    if (!std::filesystem::is_regular_file(CONFIG_FILE)) return true;
    std::ifstream file(CONFIG_FILE);
    if (!file) return false;
    try {
        nlohmann::json json = nlohmann::json::parse(file);
        if (auto item = json["enabled"]; item.is_boolean()) enabled = item.get<bool>();
        if (auto item = json["minimizeToTray"]; item.is_boolean()) minimizeToTray = item.get<bool>();
        if (auto item = json["showTrayIcon"]; item.is_boolean()) showTrayIcon = item.get<bool>();
        if (auto item = json["soundsEnabled"]; item.is_boolean()) soundsEnabled = item.get<bool>();
        if (auto item = json["form"]; item.is_number_integer()) {
            int value = item.get<int>();
            if (value >= 0 && value < KeyboardForm_Count) form = (KeyboardForm)value;
        }
        if (auto item = json["variant"]; item.is_number_integer()) {
            int value = item.get<int>();
            if (value >= 0 && value < KeyboardVariant_Count) variant = (KeyboardVariant)value;
        }
        if (auto item = json["mouse"]; item.is_number_integer()) {
            int value = item.get<int>();
            if (value >= 0 && value < MouseForm_Count) mouse = (MouseForm)value;
        }
        if (auto item = json["profiles"]; item.is_array()) profiles = item.get<std::list<std::shared_ptr<Profile>>>();
        if (auto item = json["editingProfile"]; item.is_string())
            editingProfile = FindProfile(item.get_ref<const std::string&>().c_str());
    } catch (nlohmann::json::exception&) {
        return false;
    }
    return true;
}

void Config::Save()
{
    std::ofstream file(CONFIG_FILE);
    if (!file) return;

    nlohmann::json json = nlohmann::json::object();
    if (!enabled) json["enabled"] = false;
    if (!minimizeToTray) json["minimizeToTray"] = false;
    if (!showTrayIcon) json["showTrayIcon"] = false;
    if (!soundsEnabled) json["soundsEnabled"] = false;
    if (form != KeyboardForm_75) json["form"] = (int)form;
    if (variant != KeyboardVariant_Ansi) json["variant"] = (int)variant;
    if (mouse != MouseForm_5) json["mouse"] = (int)mouse;
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

void Config::SaveIfDirty(DWORD ticks)
{
    if (_dirty && ticks - _lastSaveTicks >= CONFIG_SAVE_DELAY_MS) Save();
}

std::shared_ptr<Profile> Config::FindProfile(const char* name)
{
    auto it = std::find_if(profiles.begin(), profiles.end(),
                           [=](const std::shared_ptr<Profile>& item) { return item->name == name; });
    return it != profiles.end() ? *it : NULL;
}

std::shared_ptr<Profile> Config::FindProfileByApp(const char* app)
{
    auto it = std::find_if(profiles.begin(), profiles.end(), [app](const std::shared_ptr<Profile>& item) {
        return std::find(item->apps.begin(), item->apps.end(), app) != item->apps.end();
    });
    return it != profiles.end() ? *it : NULL;
}

bool Config::IsProfileExists(const char* name)
{
    return (bool)FindProfile(name);
}

void Config::CreateProfile(const char* name)
{
    std::shared_ptr<Profile> profile = std::make_shared<Profile>();
    profile->name = name;

    profiles.emplace_back(profile);
    editingProfile = profile;
    MarkDirty();
}

void Config::SetEditingProfile(const char* name)
{
    editingProfile = FindProfile(name);
    MarkDirty();
}

void Config::DeleteProfile(const char* name)
{
    auto it = std::find_if(profiles.begin(), profiles.end(),
                           [name](const std::shared_ptr<Profile>& item) { return item->name == name; });
    if (it == profiles.end()) return;
    if (*it == editingProfile) editingProfile = NULL;
    profiles.erase(it);
    MarkDirty();
}

bool Config::IsProfileBinded(const char* name, const char* app)
{
    if (std::shared_ptr<Profile> profile = FindProfile(name))
        return std::find(profile->apps.begin(), profile->apps.end(), app) != profile->apps.end();
    return false;
}

void Config::BindProfile(const char* name, const char* app)
{
    std::shared_ptr<Profile> profile = FindProfile(name);
    if (!profile || std::find(profile->apps.begin(), profile->apps.end(), app) != profile->apps.end()) return;
    profile->apps.emplace_back(app);
    LexicographicalSort(profile->apps);
    MarkDirty();
}

void Config::UnbindProfile(const char* name, const char* app)
{
    std::shared_ptr<Profile> profile = FindProfile(name);
    if (!profile) return;
    auto removed = std::remove(profile->apps.begin(), profile->apps.end(), app);
    if (removed == profile->apps.end()) return;
    profile->apps.erase(removed, profile->apps.end());
    MarkDirty();
}
