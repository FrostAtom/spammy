#include "KeyboardLayout.h"

namespace {

struct Grid {
    std::vector<KeyboardKey> keys;
    void Put(const char* name, UINT vk, float x, float y, float w = 1.f, float h = 1.f)
    {
        keys.push_back({name, vk, x, y, w, h});
    }
};

void AddNumberRow(Grid& g, float oy, KeyboardVariant v, bool compact)
{
    if (v == KeyboardVariant_Jis) {
        g.Put("HZ", 0xF3, 0.f, oy);
        const char* digits[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
        UINT vks[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30};
        for (int i = 0; i < 10; i++)
            g.Put(digits[i], vks[i], 1.f + i, oy);
        g.Put("-", 0xBD, 11.f, oy);
        g.Put("^", 0xDE, 12.f, oy);
        g.Put("YEN", 0xDC, 13.f, oy);
        g.Put("BACK", VK_BACK, 14.f, oy);
    } else {
        g.Put("`", 0xC0, 0.f, oy);
        const char* digits[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
        UINT vks[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30};
        for (int i = 0; i < 10; i++)
            g.Put(digits[i], vks[i], 1.f + i, oy);
        g.Put("-", 0xBD, 11.f, oy);
        g.Put("=", 0xBB, 12.f, oy);
        g.Put("BACK", VK_BACK, 13.f, oy, 2.f);
    }
    if (compact) g.Put("DEL", VK_DELETE, 15.f, oy);
}

void AddQwertyRow(Grid& g, float oy, KeyboardVariant v, bool compact)
{
    g.Put("TAB", VK_TAB, 0.f, oy, 1.5f);
    const char* letters[] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"};
    UINT vks[] = {0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4F, 0x50};
    for (int i = 0; i < 10; i++)
        g.Put(letters[i], vks[i], 1.5f + i, oy);

    if (v == KeyboardVariant_Jis) {
        g.Put("@", 0xC0, 11.5f, oy);
        g.Put("[", 0xDB, 12.5f, oy);
    } else if (v == KeyboardVariant_Iso) {
        g.Put("[", 0xDB, 11.5f, oy);
        g.Put("]", 0xDD, 12.5f, oy);
    } else {
        g.Put("[", 0xDB, 11.5f, oy);
        g.Put("]", 0xDD, 12.5f, oy);
        g.Put("\\", 0xDC, 13.5f, oy, 1.5f);
    }
    if (compact) g.Put("PGUP", VK_PRIOR, 15.f, oy);
}

void AddHomeRow(Grid& g, float oy, KeyboardVariant v, bool compact)
{
    g.Put("CAPS", VK_CAPITAL, 0.f, oy, 1.75f);
    const char* letters[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L"};
    UINT vks[] = {0x41, 0x53, 0x44, 0x46, 0x47, 0x48, 0x4A, 0x4B, 0x4C};
    for (int i = 0; i < 9; i++)
        g.Put(letters[i], vks[i], 1.75f + i, oy);

    if (v == KeyboardVariant_Jis) {
        g.Put(";", 0xBA, 10.75f, oy);
        g.Put(":", 0xBF, 11.75f, oy);
        g.Put("]", 0xDD, 12.75f, oy);
        g.Put("ENTER", VK_RETURN, 13.75f, oy - 1.f, 1.25f, 2.f);
    } else if (v == KeyboardVariant_Iso) {
        g.Put(";", 0xBA, 10.75f, oy);
        g.Put("'", 0xDE, 11.75f, oy);
        g.Put("#", 0xDF, 12.75f, oy);
        g.Put("ENTER", VK_RETURN, 13.75f, oy - 1.f, 1.25f, 2.f);
    } else {
        g.Put(";", 0xBA, 10.75f, oy);
        g.Put("'", 0xDE, 11.75f, oy);
        g.Put("ENTER", VK_RETURN, 12.75f, oy, 2.25f);
    }
    if (compact) g.Put("PGDN", VK_NEXT, 15.f, oy);
}

void AddBottomLetterRow(Grid& g, float oy, KeyboardVariant v, bool compact)
{
    const char* letters[] = {"Z", "X", "C", "V", "B", "N", "M"};
    UINT vks[] = {0x5A, 0x58, 0x43, 0x56, 0x42, 0x4E, 0x4D};

    if (v == KeyboardVariant_Iso) {
        g.Put("SHIFT", VK_LSHIFT, 0.f, oy, 1.25f);
        g.Put("\\", 0xE2, 1.25f, oy);
        for (int i = 0; i < 7; i++)
            g.Put(letters[i], vks[i], 2.25f + i, oy);
        g.Put(",", 0xBC, 9.25f, oy);
        g.Put(".", 0xBE, 10.25f, oy);
        g.Put("/", 0xBF, 11.25f, oy);
    } else if (v == KeyboardVariant_Jis) {
        float lshift = compact ? 2.f : 2.25f;
        g.Put("SHIFT", VK_LSHIFT, 0.f, oy, lshift);
        for (int i = 0; i < 7; i++)
            g.Put(letters[i], vks[i], lshift + i, oy);
        g.Put(",", 0xBC, lshift + 7.f, oy);
        g.Put(".", 0xBE, lshift + 8.f, oy);
        g.Put("/", 0xBF, lshift + 9.f, oy);
        g.Put("RO", 0xE2, lshift + 10.f, oy);
    } else {
        g.Put("SHIFT", VK_LSHIFT, 0.f, oy, 2.25f);
        for (int i = 0; i < 7; i++)
            g.Put(letters[i], vks[i], 2.25f + i, oy);
        g.Put(",", 0xBC, 9.25f, oy);
        g.Put(".", 0xBE, 10.25f, oy);
        g.Put("/", 0xBF, 11.25f, oy);
    }

    if (compact) {
        if (v == KeyboardVariant_Jis) {
            g.Put("SHIFT", VK_RSHIFT, 13.f, oy, 1.f);
        } else {
            g.Put("SHIFT", VK_RSHIFT, 12.25f, oy, 1.75f);
        }
        g.Put("UP", VK_UP, 14.f, oy);
        g.Put("END", VK_END, 15.f, oy);
    } else {
        if (v == KeyboardVariant_Jis)
            g.Put("SHIFT", VK_RSHIFT, 13.25f, oy, 1.75f);
        else
            g.Put("SHIFT", VK_RSHIFT, 12.25f, oy, 2.75f);
    }
}

void AddSpaceRow(Grid& g, float oy, KeyboardVariant v, bool compact)
{
    g.Put("CTRL", VK_LCONTROL, 0.f, oy, 1.25f);
    g.Put("WIN", VK_LWIN, 1.25f, oy, 1.25f);
    g.Put("ALT", VK_LMENU, 2.5f, oy, 1.25f);

    if (v == KeyboardVariant_Jis) {
        g.Put("MU", VK_NONCONVERT, 3.75f, oy, 1.25f);
        g.Put("SPACE", VK_SPACE, 5.f, oy, 3.75f);
        g.Put("HEN", VK_CONVERT, 8.75f, oy, 1.25f);
        g.Put("KANA", VK_KANA, 10.f, oy, 1.25f);
        if (compact) {
            g.Put("WIN", VK_RWIN, 11.25f, oy, 1.75f);
            g.Put("LEFT", VK_LEFT, 13.f, oy);
            g.Put("DOWN", VK_DOWN, 14.f, oy);
            g.Put("RIGHT", VK_RIGHT, 15.f, oy);
        } else {
            g.Put("WIN", VK_RWIN, 11.25f, oy, 1.25f);
            g.Put("MENU", VK_APPS, 12.5f, oy, 1.25f);
            g.Put("CTRL", VK_RCONTROL, 13.75f, oy, 1.25f);
        }
        return;
    }

    g.Put("SPACE", VK_SPACE, 3.75f, oy, 6.25f);
    if (compact) {
        g.Put("ALT", VK_RMENU, 10.f, oy, 1.5f);
        g.Put("WIN", VK_RWIN, 11.5f, oy, 1.5f);
        g.Put("LEFT", VK_LEFT, 13.f, oy);
        g.Put("DOWN", VK_DOWN, 14.f, oy);
        g.Put("RIGHT", VK_RIGHT, 15.f, oy);
    } else {
        g.Put("ALT", VK_RMENU, 10.f, oy, 1.25f);
        g.Put("WIN", VK_RWIN, 11.25f, oy, 1.25f);
        g.Put("MENU", VK_APPS, 12.5f, oy, 1.25f);
        g.Put("CTRL", VK_RCONTROL, 13.75f, oy, 1.25f);
    }
}

void AddCore(Grid& g, float oy, KeyboardVariant v, bool compact)
{
    AddNumberRow(g, oy + 0.f, v, compact);
    AddQwertyRow(g, oy + 1.f, v, compact);
    AddHomeRow(g, oy + 2.f, v, compact);
    AddBottomLetterRow(g, oy + 3.f, v, compact);
    AddSpaceRow(g, oy + 4.f, v, compact);
}

void AddFunctionRow(Grid& g, bool sysCluster)
{
    g.Put("ESC", VK_ESCAPE, 0.f, 0.f);
    g.Put("F1", VK_F1, 2.f, 0.f);
    g.Put("F2", VK_F2, 3.f, 0.f);
    g.Put("F3", VK_F3, 4.f, 0.f);
    g.Put("F4", VK_F4, 5.f, 0.f);
    g.Put("F5", VK_F5, 6.5f, 0.f);
    g.Put("F6", VK_F6, 7.5f, 0.f);
    g.Put("F7", VK_F7, 8.5f, 0.f);
    g.Put("F8", VK_F8, 9.5f, 0.f);
    g.Put("F9", VK_F9, 11.f, 0.f);
    g.Put("F10", VK_F10, 12.f, 0.f);
    g.Put("F11", VK_F11, 13.f, 0.f);
    g.Put("F12", VK_F12, 14.f, 0.f);
    if (sysCluster) {
        g.Put("PRT", VK_SNAPSHOT, 15.5f, 0.f);
        g.Put("SLK", VK_SCROLL, 16.5f, 0.f);
        g.Put("PSE", VK_PAUSE, 17.5f, 0.f);
    }
}

void AddCompactFunctionRow(Grid& g)
{
    g.Put("ESC", VK_ESCAPE, 0.f, 0.f);
    UINT vks[] = {VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12};
    const char* names[] = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"};
    for (int i = 0; i < 12; i++)
        g.Put(names[i], vks[i], 1.f + i, 0.f);
    g.Put("PRT", VK_SNAPSHOT, 13.f, 0.f);
    g.Put("SLK", VK_SCROLL, 14.f, 0.f);
    g.Put("PSE", VK_PAUSE, 15.f, 0.f);
}

void AddNavCluster(Grid& g, float ox, float oy)
{
    g.Put("INS", VK_INSERT, ox + 0.f, oy + 0.f);
    g.Put("HOME", VK_HOME, ox + 1.f, oy + 0.f);
    g.Put("PGUP", VK_PRIOR, ox + 2.f, oy + 0.f);
    g.Put("DEL", VK_DELETE, ox + 0.f, oy + 1.f);
    g.Put("END", VK_END, ox + 1.f, oy + 1.f);
    g.Put("PGDN", VK_NEXT, ox + 2.f, oy + 1.f);
    g.Put("UP", VK_UP, ox + 1.f, oy + 3.f);
    g.Put("LEFT", VK_LEFT, ox + 0.f, oy + 4.f);
    g.Put("DOWN", VK_DOWN, ox + 1.f, oy + 4.f);
    g.Put("RIGHT", VK_RIGHT, ox + 2.f, oy + 4.f);
}

void AddNumpad(Grid& g, float ox, float oy)
{
    g.Put("NUM", VK_NUMLOCK, ox + 0.f, oy + 0.f);
    g.Put("/", VK_DIVIDE, ox + 1.f, oy + 0.f);
    g.Put("*", VK_MULTIPLY, ox + 2.f, oy + 0.f);
    g.Put("-", VK_SUBTRACT, ox + 3.f, oy + 0.f);
    g.Put("7", VK_NUMPAD7, ox + 0.f, oy + 1.f);
    g.Put("8", VK_NUMPAD8, ox + 1.f, oy + 1.f);
    g.Put("9", VK_NUMPAD9, ox + 2.f, oy + 1.f);
    g.Put("+", VK_ADD, ox + 3.f, oy + 1.f, 1.f, 2.f);
    g.Put("4", VK_NUMPAD4, ox + 0.f, oy + 2.f);
    g.Put("5", VK_NUMPAD5, ox + 1.f, oy + 2.f);
    g.Put("6", VK_NUMPAD6, ox + 2.f, oy + 2.f);
    g.Put("1", VK_NUMPAD1, ox + 0.f, oy + 3.f);
    g.Put("2", VK_NUMPAD2, ox + 1.f, oy + 3.f);
    g.Put("3", VK_NUMPAD3, ox + 2.f, oy + 3.f);
    g.Put("ENT", VK_RETURN, ox + 3.f, oy + 3.f, 1.f, 2.f);
    g.Put("0", VK_NUMPAD0, ox + 0.f, oy + 4.f, 2.f);
    g.Put(".", VK_DECIMAL, ox + 2.f, oy + 4.f);
}

std::vector<KeyboardKey> Build(KeyboardForm form, KeyboardVariant variant)
{
    Grid g;
    switch (form) {
    case KeyboardForm_60: AddCore(g, 0.f, variant, false); break;
    case KeyboardForm_65: AddCore(g, 0.f, variant, true); break;
    case KeyboardForm_75:
        AddCompactFunctionRow(g);
        AddCore(g, 1.f, variant, true);
        break;
    case KeyboardForm_Tkl:
        AddFunctionRow(g, true);
        AddCore(g, 1.25f, variant, false);
        AddNavCluster(g, 15.5f, 1.25f);
        break;
    case KeyboardForm_Full:
        AddFunctionRow(g, true);
        AddCore(g, 1.25f, variant, false);
        AddNavCluster(g, 15.5f, 1.25f);
        AddNumpad(g, 18.75f, 1.25f);
        break;
    default: AddCore(g, 0.f, variant, false); break;
    }
    return std::move(g.keys);
}

std::vector<KeyboardKey> s_cache[KeyboardForm_Count][KeyboardVariant_Count];
bool s_built[KeyboardForm_Count][KeyboardVariant_Count] = {};

} // namespace

const std::vector<KeyboardKey>& GetKeyboardLayout(KeyboardForm form, KeyboardVariant variant)
{
    if (form < 0 || form >= KeyboardForm_Count) form = KeyboardForm_Tkl;
    if (variant < 0 || variant >= KeyboardVariant_Count) variant = KeyboardVariant_Ansi;
    if (!s_built[form][variant]) {
        s_cache[form][variant] = Build(form, variant);
        s_built[form][variant] = true;
    }
    return s_cache[form][variant];
}

const char* KeyboardFormName(KeyboardForm form)
{
    switch (form) {
    case KeyboardForm_60: return "60%";
    case KeyboardForm_65: return "65%";
    case KeyboardForm_75: return "75%";
    case KeyboardForm_Tkl: return "TKL 80%";
    case KeyboardForm_Full: return "FULL 100%";
    default: return "?";
    }
}

const char* KeyboardVariantName(KeyboardVariant variant)
{
    switch (variant) {
    case KeyboardVariant_Ansi: return "ANSI";
    case KeyboardVariant_Iso: return "ISO";
    case KeyboardVariant_Jis: return "JIS";
    default: return "?";
    }
}
