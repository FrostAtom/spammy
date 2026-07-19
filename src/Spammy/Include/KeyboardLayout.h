#pragma once
#include <Windows.h>
#include <vector>

struct KeyboardKey {
    const char* name;
    UINT vkCode;
    float x, y, w, h;
};

enum KeyboardForm {
    KeyboardForm_60,
    KeyboardForm_65,
    KeyboardForm_75,
    KeyboardForm_Tkl,
    KeyboardForm_Full,
    KeyboardForm_Count,
};

enum KeyboardVariant {
    KeyboardVariant_Ansi,
    KeyboardVariant_Iso,
    KeyboardVariant_Jis,
    KeyboardVariant_Count,
};

enum MouseForm {
    MouseForm_Off,
    MouseForm_3,
    MouseForm_5,
    MouseForm_Count,
};

const std::vector<KeyboardKey>& GetKeyboardLayout(KeyboardForm form, KeyboardVariant variant);
const char* KeyboardFormName(KeyboardForm form);
const char* KeyboardVariantName(KeyboardVariant variant);
const char* MouseFormName(MouseForm form);
