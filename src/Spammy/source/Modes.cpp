#include "Modes.h"
#include "Win32/Keyboard.h"

// swallow the physical event and do nothing else
static void Swallow(unsigned short) {}

// injects a full down+up; typematic repeats never get past the hook, so onPress needs no repeat check
static void Tap(unsigned short vkCode)
{
    sKeyboard.Press(vkCode);
}

static const KeyMode s_modes[] = {
    {Action_Spammy, "SPAMMY", "autofire while held", ImGui::UiKeyStyle_Spam, ImGui::UiCol::Spam, &Tap, &Swallow, &Tap},
    {Action_Speedy, "SPEEDY", "release instantly", ImGui::UiKeyStyle_Speedy, ImGui::UiCol::Speedy, &Tap, &Swallow,
     nullptr},
    {Action_Disabled, "NOTHING", "don't inherit global", ImGui::UiKeyStyle_Blocked, ImGui::UiCol::Danger, &Swallow,
     &Swallow, nullptr},
};

std::span<const KeyMode> KeyModes()
{
    return s_modes;
}

const KeyMode* FindKeyMode(Action action)
{
    auto it = std::ranges::find(s_modes, action, &KeyMode::action);
    return it != std::end(s_modes) ? it : nullptr;
}

// a modifier layer without its own binding falls back to the unmodified key's binding
Action ResolveKeyAction(const Profile& profile, size_t vkCode, unsigned mods, bool* inherited)
{
    const Action action = profile.keys[vkCode][mods].action;
    if (action != Action_None || mods == KeyMod_None) return action;
    const Action base = profile.keys[vkCode][KeyMod_None].action;
    if (base != Action_None && inherited) *inherited = true;
    return base;
}
