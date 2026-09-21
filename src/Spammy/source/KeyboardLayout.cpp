#include "KeyboardLayout.h"
#include <boost/container/static_vector.hpp>
#include <iterator>

namespace {

// the full 100% layout is ~110 keys, EXTRA adds another ~35
using KeyList = boost::container::static_vector<KeyboardKey, 160>;

// single-character key labels; VK codes of digits and latin letters equal their ASCII codes
struct CharLabels {
    char str[128][2] = {};
    constexpr CharLabels()
    {
        for (int c = 0; c < 128; c++)
            str[c][0] = (char)c;
    }
};
constexpr CharLabels kCharLabels;

struct Grid {
    KeyList keys;

    void Put(const char* name, UINT vk, float x, float y, float w = 1.f, float h = 1.f)
    {
        keys.push_back({name, vk, x, y, w, h});
    }

    // one unit-wide key per character, left to right
    void PutRun(const char* chars, float x, float y)
    {
        for (; *chars; chars++, x += 1.f)
            Put(kCharLabels.str[(unsigned char)*chars], *chars, x, y);
    }
};

void AddNumberRow(Grid& g, float oy, KeyboardVariant v, bool compact)
{
    const bool jis = v == KeyboardVariant_Jis;
    g.Put(jis ? "HZ" : "`", jis ? 0xF3 : 0xC0, 0.f, oy);
    g.PutRun("1234567890", 1.f, oy);
    g.Put("-", 0xBD, 11.f, oy);
    if (jis) {
        g.Put("^", 0xDE, 12.f, oy);
        g.Put("YEN", 0xDC, 13.f, oy);
        g.Put("BACK", VK_BACK, 14.f, oy);
    } else {
        g.Put("=", 0xBB, 12.f, oy);
        g.Put("BACK", VK_BACK, 13.f, oy, 2.f);
    }
    if (compact) g.Put("DEL", VK_DELETE, 15.f, oy);
}

void AddQwertyRow(Grid& g, float oy, KeyboardVariant v, bool compact)
{
    g.Put("TAB", VK_TAB, 0.f, oy, 1.5f);
    g.PutRun("QWERTYUIOP", 1.5f, oy);
    if (v == KeyboardVariant_Jis) {
        g.Put("@", 0xC0, 11.5f, oy);
        g.Put("[", 0xDB, 12.5f, oy);
    } else {
        g.Put("[", 0xDB, 11.5f, oy);
        g.Put("]", 0xDD, 12.5f, oy);
        if (v == KeyboardVariant_Ansi) g.Put("\\", 0xDC, 13.5f, oy, 1.5f);
    }
    if (compact) g.Put("PGUP", VK_PRIOR, 15.f, oy);
}

void AddHomeRow(Grid& g, float oy, KeyboardVariant v, bool compact)
{
    g.Put("CAPS", VK_CAPITAL, 0.f, oy, 1.75f);
    g.PutRun("ASDFGHJKL", 1.75f, oy);
    g.Put(";", 0xBA, 10.75f, oy);
    switch (v) {
    case KeyboardVariant_Jis:
        g.Put(":", 0xBF, 11.75f, oy);
        g.Put("]", 0xDD, 12.75f, oy);
        break;
    case KeyboardVariant_Iso:
        g.Put("'", 0xDE, 11.75f, oy);
        g.Put("#", 0xDF, 12.75f, oy);
        break;
    default: g.Put("'", 0xDE, 11.75f, oy); break;
    }
    if (v == KeyboardVariant_Ansi)
        g.Put("ENTER", VK_RETURN, 12.75f, oy, 2.25f);
    else
        g.Put("ENTER", VK_RETURN, 13.75f, oy - 1.f, 1.25f, 2.f);
    if (compact) g.Put("PGDN", VK_NEXT, 15.f, oy);
}

void AddBottomLetterRow(Grid& g, float oy, KeyboardVariant v, bool compact)
{
    const bool jis = v == KeyboardVariant_Jis;
    const float x = jis && compact ? 2.f : 2.25f;
    if (v == KeyboardVariant_Iso) {
        g.Put("SHIFT", VK_LSHIFT, 0.f, oy, 1.25f);
        g.Put("\\", 0xE2, 1.25f, oy);
    } else {
        g.Put("SHIFT", VK_LSHIFT, 0.f, oy, x);
    }
    g.PutRun("ZXCVBNM", x, oy);
    g.Put(",", 0xBC, x + 7.f, oy);
    g.Put(".", 0xBE, x + 8.f, oy);
    g.Put("/", 0xBF, x + 9.f, oy);
    if (jis) g.Put("RO", 0xE2, x + 10.f, oy);

    if (compact) {
        if (jis)
            g.Put("SHIFT", VK_RSHIFT, 13.f, oy);
        else
            g.Put("SHIFT", VK_RSHIFT, 12.25f, oy, 1.75f);
        g.Put("UP", VK_UP, 14.f, oy);
        g.Put("END", VK_END, 15.f, oy);
    } else if (jis) {
        g.Put("SHIFT", VK_RSHIFT, 13.25f, oy, 1.75f);
    } else {
        g.Put("SHIFT", VK_RSHIFT, 12.25f, oy, 2.75f);
    }
}

void AddSpaceRow(Grid& g, float oy, KeyboardVariant v, bool compact)
{
    g.Put("CTRL", VK_LCONTROL, 0.f, oy, 1.25f);
    g.Put("WIN", VK_LWIN, 1.25f, oy, 1.25f);
    g.Put("ALT", VK_LMENU, 2.5f, oy, 1.25f);

    const bool jis = v == KeyboardVariant_Jis;
    if (jis) {
        g.Put("MU", VK_NONCONVERT, 3.75f, oy, 1.25f);
        g.Put("SPACE", VK_SPACE, 5.f, oy, 3.75f);
        g.Put("HEN", VK_CONVERT, 8.75f, oy, 1.25f);
        g.Put("KANA", VK_KANA, 10.f, oy, 1.25f);
    } else {
        g.Put("SPACE", VK_SPACE, 3.75f, oy, 6.25f);
    }

    if (compact) {
        if (jis) {
            g.Put("WIN", VK_RWIN, 11.25f, oy, 1.75f);
        } else {
            g.Put("ALT", VK_RMENU, 10.f, oy, 1.5f);
            g.Put("WIN", VK_RWIN, 11.5f, oy, 1.5f);
        }
        g.Put("LEFT", VK_LEFT, 13.f, oy);
        g.Put("DOWN", VK_DOWN, 14.f, oy);
        g.Put("RIGHT", VK_RIGHT, 15.f, oy);
    } else {
        if (!jis) g.Put("ALT", VK_RMENU, 10.f, oy, 1.25f);
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

// column of the i-th F-key; compact: packed without the 0.5u gaps between F4|F5 and F8|F9
float FKeyX(int i, bool compact)
{
    return compact ? 1.f + i : 2.f + i + 0.5f * (i / 4);
}

void AddFunctionRow(Grid& g, float oy, bool compact)
{
    static constexpr const char* kNames[] = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"};
    g.Put("ESC", VK_ESCAPE, 0.f, oy);
    for (int i = 0; i < 12; i++)
        g.Put(kNames[i], VK_F1 + i, FKeyX(i, compact), oy);
    const float sys = compact ? 13.f : 15.5f;
    g.Put("PRT", VK_SNAPSHOT, sys, oy);
    g.Put("SLK", VK_SCROLL, sys + 1.f, oy);
    g.Put("PSE", VK_PAUSE, sys + 2.f, oy);
}

// two rows on top of a full-size board for keys Windows maps to scancodes but no 100% board carries:
// F13-F24 sit above F1-F12, browser keys above the nav cluster / numpad, media, launch, Sleep, Clear
// (numpad 5 with NumLock off) and Break (Ctrl+Pause arrives as its own VK) fill the second row
void AddExtraRows(Grid& g)
{
    struct Extra {
        const char* name;
        UINT vk;
    };
    static constexpr const char* kFNames[] = {"F13", "F14", "F15", "F16", "F17", "F18",
                                              "F19", "F20", "F21", "F22", "F23", "F24"};
    static constexpr Extra kMedia[] = {
        {"MUTE", VK_VOLUME_MUTE},      {"VOL-", VK_VOLUME_DOWN},         {"VOL+", VK_VOLUME_UP},
        {"PLAY", VK_MEDIA_PLAY_PAUSE}, {"PREV", VK_MEDIA_PREV_TRACK},    {"NEXT", VK_MEDIA_NEXT_TRACK},
        {"STOP", VK_MEDIA_STOP},       {"MSEL", VK_LAUNCH_MEDIA_SELECT}, {"MAIL", VK_LAUNCH_MAIL},
        {"APP1", VK_LAUNCH_APP1},      {"APP2", VK_LAUNCH_APP2},
    };
    static constexpr Extra kBrowser[] = {
        {"W<", VK_BROWSER_BACK},        {"W>", VK_BROWSER_FORWARD}, {"WHOM", VK_BROWSER_HOME},
        {"WRLD", VK_BROWSER_REFRESH},   {"WSTP", VK_BROWSER_STOP},  {"WSRC", VK_BROWSER_SEARCH},
        {"WFAV", VK_BROWSER_FAVORITES},
    };

    g.Put("SLP", VK_SLEEP, 0.f, 0.f);
    for (int i = 0; i < 12; i++)
        g.Put(kFNames[i], VK_F13 + i, FKeyX(i, false), 0.f);
    for (int i = 0; i < 3; i++)
        g.Put(kBrowser[i].name, kBrowser[i].vk, 15.5f + i, 0.f);
    for (int i = 3; i < 7; i++)
        g.Put(kBrowser[i].name, kBrowser[i].vk, 18.75f + (i - 3), 0.f);

    g.Put("CLR", VK_CLEAR, 0.f, 1.f);
    for (int i = 0; i < (int)std::size(kMedia); i++)
        g.Put(kMedia[i].name, kMedia[i].vk, FKeyX(i, false), 1.f);
    g.Put("BRK", VK_CANCEL, 17.5f, 1.f); // above PSE
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

KeyList Build(KeyboardForm form, KeyboardVariant variant)
{
    Grid g;
    switch (form) {
    case KeyboardForm_65: AddCore(g, 0.f, variant, true); break;
    case KeyboardForm_75:
        AddFunctionRow(g, 0.f, true);
        AddCore(g, 1.f, variant, true);
        break;
    case KeyboardForm_Tkl:
    case KeyboardForm_Full:
    case KeyboardForm_Extra: {
        const float oy = form == KeyboardForm_Extra ? 2.f : 0.f;
        if (form == KeyboardForm_Extra) AddExtraRows(g);
        AddFunctionRow(g, oy, false);
        AddCore(g, oy + 1.25f, variant, false);
        AddNavCluster(g, 15.5f, oy + 1.25f);
        if (form != KeyboardForm_Tkl) AddNumpad(g, 18.75f, oy + 1.25f);
        break;
    }
    default: AddCore(g, 0.f, variant, false); break;
    }
    return g.keys;
}

template <class E, size_t N>
const char* NameOf(E value, const char* const (&names)[N])
{
    return (size_t)value < N ? names[value] : "?";
}

} // namespace

std::span<const KeyboardKey> GetKeyboardLayout(KeyboardForm form, KeyboardVariant variant)
{
    if (form < 0 || form >= KeyboardForm_Count) form = KeyboardForm_Tkl;
    if (variant < 0 || variant >= KeyboardVariant_Count) variant = KeyboardVariant_Ansi;
    static KeyList cache[KeyboardForm_Count][KeyboardVariant_Count];
    KeyList& layout = cache[form][variant];
    if (layout.empty()) layout = Build(form, variant);
    return layout;
}

const char* KeyboardFormName(KeyboardForm form)
{
    static constexpr const char* kNames[KeyboardForm_Count] = {"60%", "65%", "75%", "TKL 80%", "FULL 100%", "EXTRA"};
    return NameOf(form, kNames);
}

const char* KeyboardVariantName(KeyboardVariant variant)
{
    static constexpr const char* kNames[KeyboardVariant_Count] = {"ANSI", "ISO", "JIS"};
    return NameOf(variant, kNames);
}

const char* MouseFormName(MouseForm form)
{
    static constexpr const char* kNames[MouseForm_Count] = {"HIDDEN", "3 BUTTONS", "5 BUTTONS"};
    return NameOf(form, kNames);
}
