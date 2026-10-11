#pragma once
#include "Profile.h"

using KeyHandler_t = void (*)(unsigned short vkCode);

struct KeyMode {
    Action action;
    // run on the input worker thread; a non-null onPress/onRelease makes the hook swallow the physical event
    KeyHandler_t onPress;
    KeyHandler_t onRelease;
    KeyHandler_t onTick;
};

const KeyMode* FindKeyMode(Action action);
Action ResolveKeyAction(const Profile& profile, size_t vkCode, unsigned mods);
