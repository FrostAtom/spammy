#pragma once
#include "Headers.h"
#include "Utils.h"
#include "Win32/Keyboard.h"

inline constexpr int kProfileIntervalMinMs = 10;
inline constexpr int kProfileIntervalMaxMs = 200;
inline constexpr size_t kProfileNameMax = 32;

inline constexpr unsigned kProfileRateMin = 1000 / kProfileIntervalMaxMs;
inline constexpr unsigned kProfileRateMax = 1000 / kProfileIntervalMinMs;
constexpr unsigned IntervalToRate(unsigned intervalMs)
{
    return (1000 + intervalMs / 2) / intervalMs;
}
constexpr unsigned RateToInterval(unsigned rate)
{
    return std::clamp((1000 + rate / 2) / rate, (unsigned)kProfileIntervalMinMs, (unsigned)kProfileIntervalMaxMs);
}

using KeyBundle_t = unsigned;

constexpr KeyBundle_t MakeKeyBundle(unsigned vkCode, unsigned mods)
{
    return vkCode | (mods << 16);
}
constexpr unsigned KeyBundleVk(KeyBundle_t bundle)
{
    return bundle & 0xFFFF;
}
constexpr unsigned KeyBundleMods(KeyBundle_t bundle)
{
    return (bundle >> 16) & 0xFFFF;
}

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
    using AppList_t = boost::container::flat_set<std::string, CaseInsensitiveLess>;

    std::string name;
    AppList_t apps;
    std::array<KeyModList_t, kKeyboardKeysCount> keys = {};
    unsigned intervalMs = 20;
    KeyBundle_t pauseKey = 0;
    bool disableAltF4 = false;
    bool disableWin = false;

    bool UsesMouse() const
    {
        if (Keyboard::IsMouseButton(KeyBundleVk(pauseKey))) return true;
        auto isBound = [](const KeyConfig& cfg) { return cfg.action != Action_None; };
        for (unsigned short vk = VK_LBUTTON; vk <= VK_XBUTTON2; vk++)
            if (Keyboard::IsMouseButton(vk) && std::ranges::any_of(keys[vk], isBound)) return true;
        return false;
    }
};

inline void to_json(nlohmann::json& json, const Profile& value)
{
    json = {
        {"name", value.name},
        {"apps", value.apps},
        {"speed", value.intervalMs},
        {"vkPause", value.pauseKey},
        {"disableAltF4", value.disableAltF4},
        {"disableWin", value.disableWin},
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

inline void from_json(const nlohmann::json& json, Profile& value)
{
    json.at("name").get_to(value.name);
    json.at("apps").get_to(value.apps);
    value.intervalMs =
        std::clamp(json.at("speed").get<unsigned>(), (unsigned)kProfileIntervalMinMs, (unsigned)kProfileIntervalMaxMs);
    json.at("vkPause").get_to(value.pauseKey);
    json.at("disableAltF4").get_to(value.disableAltF4);
    json.at("disableWin").get_to(value.disableWin);

    const nlohmann::json& keys = json.at("keys");
    if (!keys.is_array()) return;
    for (const nlohmann::json& item : keys) {
        const nlohmann::json& key = item.at("key");
        const nlohmann::json& action = item.at("action");
        if (!key.is_number_unsigned() || !action.is_number_unsigned()) continue;
        const KeyBundle_t bundle = key.get<KeyBundle_t>();
        const unsigned vkCode = KeyBundleVk(bundle);
        const unsigned mods = KeyBundleMods(bundle);
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
