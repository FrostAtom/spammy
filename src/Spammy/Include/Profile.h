#pragma once
#include "Headers.h"
#include "Text.h"
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

enum Action : std::uint8_t {
    Action_None,
    Action_Disabled,
    Action_Spammy,
    Action_Speedy,
    Action_Count,
};

constexpr bool IsBindableKey(unsigned vkCode)
{
    return !Keyboard::IsModifier((unsigned short)vkCode) && vkCode != VK_LWIN && vkCode != VK_RWIN;
}

struct KeyConfig {
    std::atomic<Action> action = Action_None;
};

struct Profile {
    using KeyModList_t = std::array<KeyConfig, kKeyboardKeyModCount>;
    using AppList_t = boost::container::flat_set<std::string, CaseInsensitiveLess>;

    // main thread only
    std::string name;
    AppList_t apps;

    std::array<KeyModList_t, kKeyboardKeysCount> keys = {};
    std::atomic<unsigned> intervalMs = 20;
    std::atomic<KeyBundle_t> pauseKey = 0;
    std::atomic<bool> disableAltF4 = false;
    std::atomic<bool> disableWin = false;

    bool UsesMouse() const
    {
        if (Keyboard::IsMouseButton(KeyBundleVk(pauseKey))) return true;
        auto isBound = [](const KeyConfig& cfg) { return cfg.action != Action_None; };
        for (unsigned short vk = VK_LBUTTON; vk <= VK_XBUTTON2; vk++)
            if (Keyboard::IsMouseButton(vk) && std::ranges::any_of(keys[vk], isBound)) return true;
        return false;
    }
};
