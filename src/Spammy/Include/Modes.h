#pragma once
#include "ImGui.h"
#include "Profile.h"
#include <span>

struct KeyModeContext {
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
    // a non-NULL handler means the physical event is swallowed by the hook; the handler itself
    // runs later on the input worker thread, never on the hook thread
    void (*onPress)(const KeyModeContext& ctx);
    void (*onRelease)(const KeyModeContext& ctx);
    void (*onTick)(const KeyModeContext& ctx);
};

std::span<const KeyMode> KeyModes();
const KeyMode* FindKeyMode(Action action);
Action ResolveKeyAction(const Profile& profile, size_t vkCode, unsigned mods, bool* inherited = NULL);
