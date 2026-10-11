#include "Config.h"

static constexpr DWORD CONFIG_SAVE_DELAY_MS = 10000;

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
        const int value = it->get<int>();
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
        // older builds stored the close behaviour as a plain "hide on close" flag
        if (auto it = json.find("minimizeToTray"); it != json.end() && it->is_boolean())
            closeAction = it->get<bool>() ? CloseAction_Hide : CloseAction_Exit;
        ReadEnum(json, "closeAction", closeAction, CloseAction_Count);
        ReadBool(json, "soundsEnabled", soundsEnabled);
        ReadEnum(json, "form", form, KeyboardForm_Count);
        ReadEnum(json, "variant", variant, KeyboardVariant_Count);
        ReadEnum(json, "mouse", mouse, MouseForm_Count);
        ReadEnum(json, "uiSize", uiSize, UiSize_Count);
        if (auto it = json.find("profiles"); it != json.end() && it->is_array()) it->get_to(profiles);
        if (auto it = json.find("editingProfile"); it != json.end() && it->is_string())
            editingProfile = FindProfile(it->get_ref<const std::string&>().c_str());
        if (!editingProfile && !profiles.empty()) editingProfile = profiles.front();
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
    if (closeAction != CloseAction_Ask) json["closeAction"] = (int)closeAction;
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

bool Config::IsProfileExists(const char* name) const
{
    return FindProfile(name) != nullptr;
}

static std::string_view TrimSpaces(std::string_view text)
{
    constexpr std::string_view kSpaces = " \t\r\n";
    const size_t first = text.find_first_not_of(kSpaces);
    if (first == std::string_view::npos) return {};
    return text.substr(first, text.find_last_not_of(kSpaces) - first + 1);
}

// at most max bytes, backing off to a code point boundary so a multi-byte character is never split
static std::string_view CutUtf8(std::string_view text, size_t max)
{
    if (text.size() <= max) return text;
    size_t size = max;
    while (size > 0 && ((unsigned char)text[size] & 0xC0) == 0x80)
        size--;
    return text.substr(0, size);
}

std::string Config::UniqueProfileName(std::string_view base, const Profile* except) const
{
    base = TrimSpaces(CutUtf8(TrimSpaces(base), kProfileNameMax));
    if (base.empty()) base = "Game";
    auto isFree = [&](std::string_view name) {
        return std::ranges::none_of(profiles, [&](const std::shared_ptr<Profile>& item) {
            return item.get() != except && item->name == name;
        });
    };
    if (isFree(base)) return std::string(base);
    for (unsigned n = 2;; n++) {
        const std::string suffix = " " + std::to_string(n);
        std::string name(TrimSpaces(CutUtf8(base, kProfileNameMax - suffix.size())));
        name += suffix;
        if (isFree(name)) return name;
    }
}

std::shared_ptr<Profile> Config::CreateGame(std::string_view name, std::string_view app, const Profile* copyFrom)
{
    auto profile = std::make_shared<Profile>();
    profile->name = UniqueProfileName(name);
    if (!app.empty()) profile->apps.emplace(std::string(app));
    if (copyFrom) {
        profile->keys = copyFrom->keys;
        profile->speed = copyFrom->speed;
        profile->vkPause = copyFrom->vkPause;
        profile->disableAltF4 = copyFrom->disableAltF4;
        profile->disableWin = copyFrom->disableWin;
    }
    editingProfile = profiles.emplace_back(std::move(profile));
    MarkDirty();
    return editingProfile;
}

bool Config::RenameProfile(const std::shared_ptr<Profile>& profile, std::string_view name)
{
    name = TrimSpaces(name);
    if (!profile || name.empty() || name.size() > kProfileNameMax) return false;
    auto taken = [&](const std::shared_ptr<Profile>& item) { return item != profile && item->name == name; };
    if (std::ranges::any_of(profiles, taken)) return false;
    profile->name = name;
    MarkDirty();
    return true;
}

void Config::SetEditingProfile(const char* name)
{
    editingProfile = FindProfile(name);
    MarkDirty();
}

void Config::DeleteProfile(const char* name)
{
    const std::shared_ptr<Profile> profile = FindProfile(name);
    if (!profile) return;
    const auto it = std::ranges::find(profiles, profile);
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

void Config::BindProfile(const char* name, const char* app)
{
    const std::shared_ptr<Profile> profile = FindProfile(name);
    if (profile && profile->apps.emplace(app).second) MarkDirty();
}

void Config::UnbindProfile(const char* name, const char* app)
{
    const std::shared_ptr<Profile> profile = FindProfile(name);
    if (!profile) return;
    if (auto it = profile->apps.find(std::string_view(app)); it != profile->apps.end()) {
        profile->apps.erase(it);
        MarkDirty();
    }
}
