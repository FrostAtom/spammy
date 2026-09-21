#pragma once
#include "Headers.h"
#include "Utils.h"
#include "Win32/Keyboard.h"
#define GET_KEY_MODIFIER(bundle) ((bundle >> 16) & 0xFFFF)
#define GET_KEY_VKCODE(bundle) (bundle & 0xFFFF)
#define MAKE_KEY_BUNDLE(vkCode, mods) (vkCode | (mods << 16))
#define PROFILE_SPEED_MIN 10
#define PROFILE_SPEED_MAX 200

enum Action {
    Action_None,
    Action_Disabled,
    Action_Spammy,
    Action_Speedy,
};

struct KeyConfig {
    Action action = Action_None;
};

struct Profile {
    using KeyModList_t = std::array<KeyConfig, kKeyboardKeyModCount>;
    // executable names, case-insensitive like the Windows file system, kept sorted for display
    using AppList_t = boost::container::flat_set<std::string, CaseInsensitiveLess>;

    std::string name;
    AppList_t apps;
    std::array<KeyModList_t, kKeyboardKeysCount> keys = {};
    unsigned speed = 20;
    unsigned vkPause = 0;
    bool disableAltF4 = false;
    bool disableWin = false;

    bool IsGlobal() const { return apps.empty(); }

    // true if any mouse button is bound (or is the pause key) — decides whether the mouse hook is installed
    bool UsesMouse() const
    {
        if (Keyboard::IsMouseButton(GET_KEY_VKCODE(vkPause))) return true;
        for (unsigned short vk = VK_LBUTTON; vk <= VK_XBUTTON2; vk++) {
            if (!Keyboard::IsMouseButton(vk)) continue;
            if (std::ranges::any_of(keys[vk], [](const KeyConfig& cfg) { return cfg.action != Action_None; }))
                return true;
        }
        return false;
    }
};

inline void to_json(nlohmann::json& json, const Profile& value)
{
    json = {
        {"name", value.name},
        {"apps", value.apps},
        {"speed", value.speed},
        {"vkPause", value.vkPause},
        {"disableAltF4", value.disableAltF4},
        {"disableWin", value.disableWin},
    };

    nlohmann::json keys = nlohmann::json::array();
    for (unsigned k = 0; k < kKeyboardKeysCount; k++) {
        for (unsigned m = 0; m < kKeyboardKeyModCount; m++) {
            if (Action action = value.keys[k][m].action; action != Action_None)
                keys.push_back({{"key", (unsigned)MAKE_KEY_BUNDLE(k, m)}, {"action", (unsigned)action}});
        }
    }
    json["keys"] = std::move(keys);
}

inline void from_json(const nlohmann::json& json, Profile& value)
{
    json.at("name").get_to(value.name);
    json.at("apps").get_to(value.apps);
    value.speed =
        std::clamp(json.at("speed").get<unsigned>(), (unsigned)PROFILE_SPEED_MIN, (unsigned)PROFILE_SPEED_MAX);
    json.at("vkPause").get_to(value.vkPause);
    json.at("disableAltF4").get_to(value.disableAltF4);
    json.at("disableWin").get_to(value.disableWin);

    const nlohmann::json& keys = json.at("keys");
    if (!keys.is_array()) return;
    for (const nlohmann::json& item : keys) {
        const nlohmann::json& key = item.at("key");
        const nlohmann::json& action = item.at("action");
        if (!key.is_number_unsigned() || !action.is_number_unsigned()) continue;
        unsigned bundle = key.get<unsigned>();
        unsigned vkCode = GET_KEY_VKCODE(bundle);
        unsigned mods = GET_KEY_MODIFIER(bundle);
        if (vkCode < kKeyboardKeysCount && mods < kKeyboardKeyModCount)
            value.keys[vkCode][mods].action = (Action)action.get<unsigned>();
    }
}

template <class T>
inline void to_json(nlohmann::json& json, const std::shared_ptr<T>& value)
{
    to_json(json, *value);
}

template <class T>
inline void from_json(const nlohmann::json& json, std::shared_ptr<T>& value)
{
    value = std::make_shared<T>();
    from_json(json, *value);
}
