#include "Modes.h"
#include "Win32/Keyboard.h"

// swallow the physical event and do nothing else
static void Swallow(const KeyModeContext&) {}

static void PressOnce(const KeyModeContext& ctx)
{
    if (!ctx.repeat) sKeyboard.Press(ctx.vkCode);
}

static void Autofire(const KeyModeContext& ctx)
{
    sKeyboard.Press(ctx.vkCode);
}

static const KeyMode s_modes[] = {
    {Action_Spammy, "SPAMMY", "autofire while held", ImGui::UiKeyStyle_Spam, ImGui::UiCol::Spam, &PressOnce, &Swallow,
     &Autofire},
    {Action_Speedy, "SPEEDY", "release instantly", ImGui::UiKeyStyle_Speedy, ImGui::UiCol::Speedy, &PressOnce, &Swallow,
     NULL},
    {Action_Disabled, "NOTHING", "don't inherit global", ImGui::UiKeyStyle_Blocked, ImGui::UiCol::Danger, &Swallow,
     &Swallow, NULL},
};

std::span<const KeyMode> KeyModes()
{
    return s_modes;
}

const KeyMode* FindKeyMode(Action action)
{
    auto it = std::ranges::find(s_modes, action, &KeyMode::action);
    return it != std::end(s_modes) ? it : NULL;
}

// a modifier layer without its own binding falls back to the unmodified key's binding
Action ResolveKeyAction(const Profile& profile, size_t vkCode, unsigned mods, bool* inherited)
{
    Action action = profile.keys[vkCode][mods].action;
    if (action != Action_None || mods == KeyMod_None) return action;
    Action base = profile.keys[vkCode][KeyMod_None].action;
    if (base != Action_None && inherited) *inherited = true;
    return base;
}
