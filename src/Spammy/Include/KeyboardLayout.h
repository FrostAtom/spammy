#pragma once
#include "Headers.h"

struct KeyboardKey {
    const char* name = nullptr;
    UINT vkCode = 0;
    float x = 0.f, y = 0.f, w = 1.f, h = 1.f;
};

enum KeyboardForm : std::uint8_t {
    KeyboardForm_60,
    KeyboardForm_65,
    KeyboardForm_75,
    KeyboardForm_Tkl,
    KeyboardForm_Full,
    KeyboardForm_Extra,
    KeyboardForm_Count,
};

enum KeyboardVariant : std::uint8_t {
    KeyboardVariant_Ansi,
    KeyboardVariant_Iso,
    KeyboardVariant_Jis,
    KeyboardVariant_Count,
};

enum MouseForm : std::uint8_t {
    MouseForm_Off,
    MouseForm_3,
    MouseForm_5,
    MouseForm_Count,
};

std::span<const KeyboardKey> GetKeyboardLayout(KeyboardForm form, KeyboardVariant variant);
const char* KeyboardFormName(KeyboardForm form);
const char* KeyboardVariantName(KeyboardVariant variant);
const char* MouseFormName(MouseForm form);
