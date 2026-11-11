#include "KeyboardLayout.h"

namespace {

constexpr size_t kMaxKeysPerLayout = 160;
using KeyList = boost::container::static_vector<KeyboardKey, kMaxKeysPerLayout>;

struct SingleCharLabels {
    char str[128][2] = {};
    constexpr SingleCharLabels()
    {
        for (int c = 0; c < 128; c++)
            str[c][0] = (char)c;
    }
};
constexpr SingleCharLabels kSingleCharLabels;

constexpr UINT VkOfDigitOrCapital(char c)
{
    return (unsigned char)c;
}

constexpr const char* kFKeyNames[] = {"F1",  "F2",  "F3",  "F4",  "F5",  "F6",  "F7",  "F8",
                                      "F9",  "F10", "F11", "F12", "F13", "F14", "F15", "F16",
                                      "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24"};

constexpr float kNavClusterX = 15.5f;
constexpr float kNumpadX = 18.75f;

constexpr KeyboardKey kNavCluster[] = {
    {"Ins", VK_INSERT, 0, 0}, {"Home", VK_HOME, 1, 0},   {"PgUp", VK_PRIOR, 2, 0}, {"Del", VK_DELETE, 0, 1},
    {"End", VK_END, 1, 1},    {"PgDn", VK_NEXT, 2, 1},   {"Up", VK_UP, 1, 3},      {"Left", VK_LEFT, 0, 4},
    {"Down", VK_DOWN, 1, 4},  {"Right", VK_RIGHT, 2, 4},
};

constexpr KeyboardKey kNumpad[] = {
    {"Num", VK_NUMLOCK, 0, 0}, {"/", VK_DIVIDE, 1, 0},  {"*", VK_MULTIPLY, 2, 0},       {"-", VK_SUBTRACT, 3, 0},
    {"7", VK_NUMPAD7, 0, 1},   {"8", VK_NUMPAD8, 1, 1}, {"9", VK_NUMPAD9, 2, 1},        {"+", VK_ADD, 3, 1, 1, 2},
    {"4", VK_NUMPAD4, 0, 2},   {"5", VK_NUMPAD5, 1, 2}, {"6", VK_NUMPAD6, 2, 2},        {"1", VK_NUMPAD1, 0, 3},
    {"2", VK_NUMPAD2, 1, 3},   {"3", VK_NUMPAD3, 2, 3}, {"Ent", VK_RETURN, 3, 3, 1, 2}, {"0", VK_NUMPAD0, 0, 4, 2},
    {".", VK_DECIMAL, 2, 4},
};

struct Grid {
    KeyList keys;

    void Put(const char* name, UINT vk, float x, float y, float w = 1.f, float h = 1.f)
    {
        keys.push_back({name, vk, x, y, w, h});
    }

    void PutRun(const char* digitsOrCapitals, float x, float y)
    {
        for (const char* c = digitsOrCapitals; *c; c++, x += 1.f)
            Put(kSingleCharLabels.str[(unsigned char)*c], VkOfDigitOrCapital(*c), x, y);
    }

    void PutBlock(std::span<const KeyboardKey> block, float left, float top)
    {
        for (const KeyboardKey& key : block)
            Put(key.name, key.vkCode, left + key.x, top + key.y, key.w, key.h);
    }
};

void AddNumberRow(Grid& g, float oy, KeyboardVariant v, bool compact)
{
    const bool jis = v == KeyboardVariant_Jis;
    g.Put(jis ? "Hz" : "`", jis ? 0xF3 : 0xC0, 0.f, oy);
    g.PutRun("1234567890", 1.f, oy);
    g.Put("-", 0xBD, 11.f, oy);
    if (jis) {
        g.Put("^", 0xDE, 12.f, oy);
        g.Put("Yen", 0xDC, 13.f, oy);
        g.Put("Back", VK_BACK, 14.f, oy);
    } else {
        g.Put("=", 0xBB, 12.f, oy);
        g.Put("Back", VK_BACK, 13.f, oy, 2.f);
    }
    if (compact) g.Put("Del", VK_DELETE, 15.f, oy);
}

void AddQwertyRow(Grid& g, float oy, KeyboardVariant v, bool compact)
{
    g.Put("Tab", VK_TAB, 0.f, oy, 1.5f);
    g.PutRun("QWERTYUIOP", 1.5f, oy);
    if (v == KeyboardVariant_Jis) {
        g.Put("@", 0xC0, 11.5f, oy);
        g.Put("[", 0xDB, 12.5f, oy);
    } else {
        g.Put("[", 0xDB, 11.5f, oy);
        g.Put("]", 0xDD, 12.5f, oy);
        if (v == KeyboardVariant_Ansi) g.Put("\\", 0xDC, 13.5f, oy, 1.5f);
    }
    if (compact) g.Put("PgUp", VK_PRIOR, 15.f, oy);
}

void AddHomeRow(Grid& g, float oy, KeyboardVariant v, bool compact)
{
    g.Put("Caps", VK_CAPITAL, 0.f, oy, 1.75f);
    g.PutRun("ASDFGHJKL", 1.75f, oy);
    const bool jis = v == KeyboardVariant_Jis;
    g.Put(";", jis ? 0xBB : 0xBA, 10.75f, oy);
    switch (v) {
    case KeyboardVariant_Jis:
        g.Put(":", 0xBA, 11.75f, oy);
        g.Put("]", 0xDD, 12.75f, oy);
        break;
    case KeyboardVariant_Iso:
        g.Put("'", 0xDE, 11.75f, oy);
        g.Put("#", 0xDF, 12.75f, oy);
        break;
    default: g.Put("'", 0xDE, 11.75f, oy); break;
    }
    if (v == KeyboardVariant_Ansi)
        g.Put("Enter", VK_RETURN, 12.75f, oy, 2.25f);
    else
        g.Put("Enter", VK_RETURN, 13.75f, oy - 1.f, 1.25f, 2.f);
    if (compact) g.Put("PgDn", VK_NEXT, 15.f, oy);
}

void AddBottomLetterRow(Grid& g, float oy, KeyboardVariant v, bool compact)
{
    const bool jis = v == KeyboardVariant_Jis;
    const float x = jis && compact ? 2.f : 2.25f;
    if (v == KeyboardVariant_Iso) {
        g.Put("Shift", VK_LSHIFT, 0.f, oy, 1.25f);
        g.Put("\\", 0xE2, 1.25f, oy);
    } else {
        g.Put("Shift", VK_LSHIFT, 0.f, oy, x);
    }
    g.PutRun("ZXCVBNM", x, oy);
    g.Put(",", 0xBC, x + 7.f, oy);
    g.Put(".", 0xBE, x + 8.f, oy);
    g.Put("/", 0xBF, x + 9.f, oy);
    if (jis) g.Put("Ro", 0xE2, x + 10.f, oy);

    if (compact) {
        if (jis)
            g.Put("Shift", VK_RSHIFT, 13.f, oy);
        else
            g.Put("Shift", VK_RSHIFT, 12.25f, oy, 1.75f);
        g.Put("Up", VK_UP, 14.f, oy);
        g.Put("End", VK_END, 15.f, oy);
    } else if (jis) {
        g.Put("Shift", VK_RSHIFT, 13.25f, oy, 1.75f);
    } else {
        g.Put("Shift", VK_RSHIFT, 12.25f, oy, 2.75f);
    }
}

void AddSpaceRow(Grid& g, float oy, KeyboardVariant v, bool compact)
{
    g.Put("Ctrl", VK_LCONTROL, 0.f, oy, 1.25f);
    g.Put("Win", VK_LWIN, 1.25f, oy, 1.25f);
    g.Put("Alt", VK_LMENU, 2.5f, oy, 1.25f);

    const bool jis = v == KeyboardVariant_Jis;
    if (jis) {
        g.Put("Mu", VK_NONCONVERT, 3.75f, oy, 1.25f);
        g.Put("Space", VK_SPACE, 5.f, oy, 3.75f);
        g.Put("Hen", VK_CONVERT, 8.75f, oy, 1.25f);
        g.Put("Kana", VK_KANA, 10.f, oy, 1.25f);
    } else {
        g.Put("Space", VK_SPACE, 3.75f, oy, 6.25f);
    }

    if (compact) {
        if (jis) {
            g.Put("Win", VK_RWIN, 11.25f, oy, 1.75f);
        } else {
            g.Put("Alt", VK_RMENU, 10.f, oy, 1.5f);
            g.Put("Win", VK_RWIN, 11.5f, oy, 1.5f);
        }
        g.Put("Left", VK_LEFT, 13.f, oy);
        g.Put("Down", VK_DOWN, 14.f, oy);
        g.Put("Right", VK_RIGHT, 15.f, oy);
    } else {
        if (!jis) g.Put("Alt", VK_RMENU, 10.f, oy, 1.25f);
        g.Put("Win", VK_RWIN, 11.25f, oy, 1.25f);
        g.Put("Menu", VK_APPS, 12.5f, oy, 1.25f);
        g.Put("Ctrl", VK_RCONTROL, 13.75f, oy, 1.25f);
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

float FKeyX(int index, bool compact)
{
    if (compact) return 1.f + index;
    constexpr float kGapBetweenFours = 0.5f;
    const int groupOfFour = index / 4;
    return 2.f + index + kGapBetweenFours * groupOfFour;
}

void AddFunctionRow(Grid& g, float oy, bool compact)
{
    g.Put("Esc", VK_ESCAPE, 0.f, oy);
    for (int i = 0; i < 12; i++)
        g.Put(kFKeyNames[i], VK_F1 + i, FKeyX(i, compact), oy);
    const float systemKeysX = compact ? 13.f : kNavClusterX;
    g.Put("Prt", VK_SNAPSHOT, systemKeysX, oy);
    g.Put("Slk", VK_SCROLL, systemKeysX + 1.f, oy);
    g.Put("Pse", VK_PAUSE, systemKeysX + 2.f, oy);
}

void AddExtraRows(Grid& g)
{
    constexpr UINT kVkNumpad5WithoutNumLock = VK_CLEAR;
    constexpr UINT kVkCtrlPause = VK_CANCEL;
    constexpr float kAbovePauseX = kNavClusterX + 2.f;
    static constexpr KeyboardKey kBrowser[] = {
        {"W<", VK_BROWSER_BACK, kNavClusterX, 0},          {"W>", VK_BROWSER_FORWARD, kNavClusterX + 1.f, 0},
        {"Whom", VK_BROWSER_HOME, kNavClusterX + 2.f, 0},  {"Wrld", VK_BROWSER_REFRESH, kNumpadX, 0},
        {"Wstp", VK_BROWSER_STOP, kNumpadX + 1.f, 0},      {"Wsrc", VK_BROWSER_SEARCH, kNumpadX + 2.f, 0},
        {"Wfav", VK_BROWSER_FAVORITES, kNumpadX + 3.f, 0},
    };
    struct NamedKey {
        const char* name;
        UINT vk;
    };
    static constexpr NamedKey kMedia[] = {
        {"Mute", VK_VOLUME_MUTE},      {"Vol-", VK_VOLUME_DOWN},         {"Vol+", VK_VOLUME_UP},
        {"Play", VK_MEDIA_PLAY_PAUSE}, {"Prev", VK_MEDIA_PREV_TRACK},    {"Next", VK_MEDIA_NEXT_TRACK},
        {"Stop", VK_MEDIA_STOP},       {"Msel", VK_LAUNCH_MEDIA_SELECT}, {"Mail", VK_LAUNCH_MAIL},
        {"App1", VK_LAUNCH_APP1},      {"App2", VK_LAUNCH_APP2},
    };

    g.Put("Slp", VK_SLEEP, 0.f, 0.f);
    for (int i = 0; i < 12; i++)
        g.Put(kFKeyNames[12 + i], VK_F13 + i, FKeyX(i, false), 0.f);
    g.PutBlock(kBrowser, 0.f, 0.f);

    g.Put("Clr", kVkNumpad5WithoutNumLock, 0.f, 1.f);
    for (size_t i = 0; i < std::size(kMedia); i++)
        g.Put(kMedia[i].name, kMedia[i].vk, FKeyX((int)i, false), 1.f);
    g.Put("Brk", kVkCtrlPause, kAbovePauseX, 1.f);
}

KeyList Build(KeyboardForm form, KeyboardVariant variant)
{
    Grid g;
    switch (form) {
    case KeyboardForm_60: AddCore(g, 0.f, variant, false); break;
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
        g.PutBlock(kNavCluster, kNavClusterX, oy + 1.25f);
        if (form != KeyboardForm_Tkl) g.PutBlock(kNumpad, kNumpadX, oy + 1.25f);
        break;
    }
    case KeyboardForm_Count: break;
    }
    return g.keys;
}

} // namespace

std::span<const KeyboardKey> GetKeyboardLayout(KeyboardForm form, KeyboardVariant variant)
{
    static KeyList cache[KeyboardForm_Count][KeyboardVariant_Count];
    KeyList& layout = cache[form][variant];
    if (layout.empty()) layout = Build(form, variant);
    return layout;
}

const char* KeyboardFormName(KeyboardForm form)
{
    static constexpr const char* kNames[KeyboardForm_Count] = {"60%", "65%", "75%", "TKL 80%", "Full 100%", "Extra"};
    return kNames[form];
}

const char* KeyboardVariantName(KeyboardVariant variant)
{
    static constexpr const char* kNames[KeyboardVariant_Count] = {"ANSI", "ISO", "JIS"};
    return kNames[variant];
}

const char* MouseFormName(MouseForm form)
{
    static constexpr const char* kNames[MouseForm_Count] = {"Hidden", "3 buttons", "5 buttons"};
    return kNames[form];
}
