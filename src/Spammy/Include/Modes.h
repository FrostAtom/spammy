#pragma once
#include "ImGui.h"
#include "Profile.h"

using KeyHandler_t = void (*)(unsigned short vkCode);

struct KeyMode {
    Action action;
    const char* name;
    const char* desc;
    ImGui::UiKeyStyle keyStyle;
    ImU32 menuColor;
    // a non-NULL handler means the physical event is swallowed by the hook; the handler itself
    // runs later on the input worker thread, never on the hook thread
    KeyHandler_t onPress;
    KeyHandler_t onRelease;
    KeyHandler_t onTick;
};

std::span<const KeyMode> KeyModes();
const KeyMode* FindKeyMode(Action action);
Action ResolveKeyAction(const Profile& profile, size_t vkCode, unsigned mods, bool* inherited = nullptr);
