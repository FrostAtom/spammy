#include "Modes.h"
#include "Win32/Keyboard.h"

static void Swallow(unsigned short) {}

static void Tap(unsigned short vkCode)
{
    sKeyboard.Press(vkCode);
}

static const KeyMode s_modes[] = {
    {Action_Spammy, &Tap, &Swallow, &Tap},
    {Action_Speedy, &Tap, &Swallow, nullptr},
    {Action_Disabled, &Swallow, &Swallow, nullptr},
};

const KeyMode* FindKeyMode(Action action)
{
    auto it = std::ranges::find(s_modes, action, &KeyMode::action);
    return it != std::end(s_modes) ? it : nullptr;
}

Action ResolveKeyAction(const Profile& profile, size_t vkCode, unsigned mods)
{
    const Action modifiedAction = profile.keys[vkCode][mods].action;
    return modifiedAction != Action_None ? modifiedAction : profile.keys[vkCode][KeyMod_None].action;
}
