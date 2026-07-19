#include "Modes.h"
#include "Win32/Keyboard.h"

static bool Swallow(const KeyModeContext&)
{
    return true;
}

static bool PressOnce(const KeyModeContext& ctx)
{
    if (!ctx.repeat) sKeyboard.Press(ctx.hwnd, ctx.vkCode);
    return true;
}

static void Autofire(const KeyModeContext& ctx)
{
    sKeyboard.Press(ctx.hwnd, ctx.vkCode);
}

static const KeyMode s_modes[] = {
    {Action_Spammy, "SPAMMY", ImGui::UiKeyStyle_Spam, ImGui::UiCol::Spam, &PressOnce, &Swallow, &Autofire},
    {Action_Speedy, "SPEEDY", ImGui::UiKeyStyle_Speedy, ImGui::UiCol::Speedy, &PressOnce, &Swallow, NULL},
    {Action_Disabled, "DISABLED", ImGui::UiKeyStyle_Blocked, ImGui::UiCol::Danger, &Swallow, &Swallow, NULL},
};

std::span<const KeyMode> KeyModes()
{
    return s_modes;
}

const KeyMode* FindKeyMode(Action action)
{
    for (const KeyMode& mode : s_modes)
        if (mode.action == action) return &mode;
    return NULL;
}

Action ResolveKeyAction(const Profile& profile, size_t vkCode, unsigned mods, bool* inherited)
{
    Action action = profile.keys[vkCode][mods].action;
    if (action == Action_None && mods != KeyMod_None) {
        Action base = profile.keys[vkCode][KeyMod_None].action;
        if (base != Action_None) {
            action = base;
            if (inherited) *inherited = true;
        }
    }
    return action;
}
