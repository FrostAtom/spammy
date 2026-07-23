#pragma once
#include "ImGui.h"
#include "Profile.h"
#include <span>

struct KeyModeContext {
    HWND hwnd;
    unsigned short vkCode;
    bool repeat;
    const Profile& profile;
};

struct KeyMode {
    Action action;
    const char* name;
    const char* desc;
    ImGui::UiKeyStyle keyStyle;
    ImU32 menuColor;
    bool (*onPress)(const KeyModeContext& ctx);
    bool (*onRelease)(const KeyModeContext& ctx);
    void (*onTick)(const KeyModeContext& ctx);
};

std::span<const KeyMode> KeyModes();
const KeyMode* FindKeyMode(Action action);
Action ResolveKeyAction(const Profile& profile, size_t vkCode, unsigned mods, bool* inherited = NULL);
