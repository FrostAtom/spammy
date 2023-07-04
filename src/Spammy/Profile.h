#pragma once
#include "Headers.h"
#include "Win32/Keyboard.h"
#define GET_KEY_MODIFIER(bundle) ((bundle >> 16) & 0xFFFF)
#define GET_KEY_VKCODE(bundle) (bundle & 0xFFFF)
#define MAKE_KEY_BUNDLE(vkCode, mods) (vkCode | (mods << 16))

enum Action {
    Action_None,
    Action_Disabled,
    Action_Spammy,
    Action_Speedy,
};

struct KeyConfig {
    inline KeyConfig() : action(Action_None) {}
    Action action;
};

struct Profile {
    using KeyModList_t = std::array<KeyConfig, KEYBOARD_KEYMOD_COUNT>;

    inline Profile() : speed(20), vkPause(0), disableAltF4(false), disableWin(false) {}
    std::string name;
    std::vector<std::string> apps;
    std::array<KeyModList_t, KEYBOARD_KEYS_COUNT> keys;
    unsigned speed;
    unsigned vkPause;
    bool disableAltF4;
    bool disableWin;
};

#define JSON_READ_FIELD(field)\
    value.field = json.at(_CRT_STRINGIZE(field)).get<decltype(value.field)>()


//NLOHMANN_JSON_SERIALIZE_ENUM(Action, {
//    { Action_None, nullptr },
//    { Action_Disabled, "disabled" },
//    { Action_Spammy, "spammy" },
//    { Action_Speedy, "speedy" },
//})
//
//inline void to_json(nlohmann::json& json, const KeyConfig& value)
//{
//    json = {
//        {"action", value.action},
//    };
//}
//
//inline void from_json(const nlohmann::json& json, KeyConfig& value)
//{
//    JSON_READ_FIELD(action);
//}

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
    for (int k = 0; k < KEYBOARD_KEYS_COUNT; k++) {
        for (int m = 0; m < KEYBOARD_KEYMOD_COUNT; m++) {
            if (value.keys[k][m].action != Action_None) {
                keys.push_back({
                    { "key", (unsigned)MAKE_KEY_BUNDLE(k, m)},
                    { "action", (unsigned)value.keys[k][m].action },
                });
            }
        }
    }
    json["keys"] = keys;
}

inline void from_json(const nlohmann::json& json, Profile& value)
{
    JSON_READ_FIELD(name);
    JSON_READ_FIELD(apps);
    JSON_READ_FIELD(speed);
    JSON_READ_FIELD(vkPause);
    JSON_READ_FIELD(disableAltF4);
    JSON_READ_FIELD(disableWin);

    if (auto keys = json.at("keys"); keys.is_array()) {
        for (auto& item : keys) {
            if (auto key = item.at("key"); key.is_number_unsigned()) {
                if (auto action = item.at("action"); action.is_number_unsigned()) {
                    unsigned bundle = key.get<unsigned>();
                    value.keys[GET_KEY_VKCODE(bundle)][GET_KEY_MODIFIER(bundle)].action = (Action)action.get<unsigned>();
                }
            }
        }
    }
}

template<class T>
inline void to_json(nlohmann::json& json, const std::shared_ptr<T>& value)
{
    return to_json(json, *value);
}

template<class T>
inline void from_json(const nlohmann::json& json, std::shared_ptr<T>& value)
{
    value = std::make_shared<T>();
    return from_json(json, *value);
}

#undef JSON_READ_FIELD