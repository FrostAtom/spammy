#include "Config.h"
#include "Text.h"
#include <fstream>
#include <system_error>

static constexpr DWORD CONFIG_SAVE_DELAY_MS = 10000;
static constexpr const char* CONFIG_FILE = "Spammy.json";
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

const char* UiThemeName(UiTheme theme)
{
    static constexpr const char* s_names[UiTheme_Count] = {"System", "Light", "Dark"};
    return s_names[theme];
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

static void to_json(nlohmann::json& json, const Profile& value)
{
    json = {
        {"name", value.name},
        {"apps", value.apps},
        {"speed", value.intervalMs.load()},
        {"vkPause", value.pauseKey.load()},
        {"disableAltF4", value.disableAltF4.load()},
        {"disableWin", value.disableWin.load()},
    };

    nlohmann::json keys = nlohmann::json::array();
    for (unsigned k = 0; k < kKeyboardKeysCount; k++) {
        for (unsigned m = 0; m < kKeyboardKeyModCount; m++) {
            if (const Action action = value.keys[k][m].action; action != Action_None)
                keys.push_back({{"key", MakeKeyBundle(k, m)}, {"action", (unsigned)action}});
        }
    }
    json["keys"] = std::move(keys);
}

static std::string_view CutUtf8(std::string_view text, size_t maxBytes)
{
    if (text.size() <= maxBytes) return text;
    size_t size = maxBytes;
    while (size > 0 && IsUtf8ContinuationByte(text[size]))
        size--;
    return text.substr(0, size);
}

static std::string_view FitProfileName(std::string_view name)
{
    return TrimSpaces(CutUtf8(TrimSpaces(name), kProfileNameMax));
}

static std::string UniqueProfileName(const Config::ProfileList_t& taken, std::string_view base)
{
    base = FitProfileName(base);
    if (base.empty()) base = "Game";
    auto isFree = [&](std::string_view name) {
        return std::ranges::none_of(taken, [&](const std::shared_ptr<Profile>& item) { return item->name == name; });
    };
    if (isFree(base)) return std::string(base);
    for (unsigned n = 2;; n++) {
        const std::string suffix = fmt::format(" {}", n);
        std::string name(TrimSpaces(CutUtf8(base, kProfileNameMax - suffix.size())));
        name += suffix;
        if (isFree(name)) return name;
    }
}

static void from_json(const nlohmann::json& json, Profile& value)
{
    json.at("name").get_to(value.name);
    json.at("apps").get_to(value.apps);
    value.intervalMs =
        std::clamp(json.at("speed").get<unsigned>(), (unsigned)kProfileIntervalMinMs, (unsigned)kProfileIntervalMaxMs);
    value.pauseKey = json.at("vkPause").get<KeyBundle_t>();
    value.disableAltF4 = json.at("disableAltF4").get<bool>();
    value.disableWin = json.at("disableWin").get<bool>();

    const nlohmann::json& keys = json.at("keys");
    if (!keys.is_array()) return;
    for (const nlohmann::json& item : keys) {
        const nlohmann::json& key = item.at("key");
        const nlohmann::json& action = item.at("action");
        if (!key.is_number_unsigned() || !action.is_number_unsigned()) continue;
        const KeyBundle_t bundle = key.get<KeyBundle_t>();
        const unsigned vkCode = KeyBundleVk(bundle);
        const unsigned mods = KeyBundleMods(bundle);
        const unsigned actionValue = action.get<unsigned>();
        const bool blocksKeyPressedAlone = mods == KeyMod_None && actionValue == Action_Disabled;
        const bool editorCanSetIt = IsBindableKey(vkCode) && !blocksKeyPressedAlone;
        if (vkCode < kKeyboardKeysCount && mods < kKeyboardKeyModCount && actionValue < Action_Count && editorCanSetIt)
            value.keys[vkCode][mods].action = (Action)actionValue;
    }
}

static void to_json(nlohmann::json& json, const std::shared_ptr<Profile>& value)
{
    to_json(json, *value);
}

static void from_json(const nlohmann::json& json, std::shared_ptr<Profile>& value)
{
    value = std::make_shared<Profile>();
    from_json(json, *value);
}

static void TryReadBool(const nlohmann::json& json, const char* key, std::atomic<bool>& out)
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
    std::error_code unreadableFailsAtOpen;
    const std::filesystem::file_status status = std::filesystem::status(CONFIG_FILE, unreadableFailsAtOpen);
    const bool firstRun = status.type() == std::filesystem::file_type::not_found;
    if (firstRun) return true;
    std::ifstream file(CONFIG_FILE);
    if (!file) return false;
    try {
        const nlohmann::json json = nlohmann::json::parse(file);
        // the only read that throws: done before anything is applied, so a refused file leaves the defaults
        ProfileList_t loaded;
        if (auto it = json.find("profiles"); it != json.end() && it->is_array()) it->get_to(loaded);
        TryReadBool(json, "enabled", enabled);
        TryReadEnum(json, "closeAction", closeAction, CloseAction_Count);
        TryReadBool(json, "soundsEnabled", soundsEnabled);
        TryReadEnum(json, "form", form, KeyboardForm_Count);
        TryReadEnum(json, "variant", variant, KeyboardVariant_Count);
        TryReadEnum(json, "mouse", mouse, MouseForm_Count);
        TryReadEnum(json, "uiSize", uiSize, UiSize_Count);
        TryReadEnum(json, "theme", theme, UiTheme_Count);
        profiles.clear();
        for (std::shared_ptr<Profile>& profile : loaded) {
            profile->name = UniqueProfileName(profiles, profile->name);
            profiles.push_back(std::move(profile));
        }
        if (auto it = json.find("editingProfile"); it != json.end() && it->is_string())
            editingProfile = FindProfile(FitProfileName(it->get_ref<const std::string&>()));
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
    if (theme != UiTheme_System) json["theme"] = (int)theme;
    if (editingProfile) json["editingProfile"] = editingProfile->name;
    if (!profiles.empty()) json["profiles"] = profiles;

    if (!WriteFileAtomically(CONFIG_FILE, CONFIG_TEMP_FILE, json.dump(2))) _dirty = true;
}

void Config::MarkDirty()
{
    _dirty = true;
}

DWORD Config::MsUntilSaveDue() const
{
    if (!_dirty) return INFINITE;
    const DWORD sinceLastAttemptMs = GetTickCount() - _lastSaveAttemptTicks;
    return sinceLastAttemptMs < CONFIG_SAVE_DELAY_MS ? CONFIG_SAVE_DELAY_MS - sinceLastAttemptMs : 0;
}

template <class Pred>
static std::shared_ptr<Profile> FindProfileIf(const Config::ProfileList_t& profiles, Pred pred)
{
    auto it = std::ranges::find_if(profiles, [&](const std::shared_ptr<Profile>& item) { return pred(*item); });
    return it != profiles.end() ? *it : nullptr;
}

std::shared_ptr<Profile> Config::FindProfile(std::string_view name) const
{
    return FindProfileIf(profiles, [name](const Profile& item) { return item.name == name; });
}

std::shared_ptr<Profile> Config::FindProfileByApp(std::string_view app) const
{
    return FindProfileIf(profiles, [app](const Profile& item) { return item.apps.contains(app); });
}

std::shared_ptr<Profile> Config::CreateGame(std::string_view name, std::string_view app)
{
    auto profile = std::make_shared<Profile>();
    profile->name = UniqueProfileName(profiles, name);
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
