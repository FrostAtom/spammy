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
    {"INS", VK_INSERT, 0, 0}, {"HOME", VK_HOME, 1, 0},   {"PGUP", VK_PRIOR, 2, 0}, {"DEL", VK_DELETE, 0, 1},
    {"END", VK_END, 1, 1},    {"PGDN", VK_NEXT, 2, 1},   {"UP", VK_UP, 1, 3},      {"LEFT", VK_LEFT, 0, 4},
    {"DOWN", VK_DOWN, 1, 4},  {"RIGHT", VK_RIGHT, 2, 4},
};

constexpr KeyboardKey kNumpad[] = {
    {"NUM", VK_NUMLOCK, 0, 0}, {"/", VK_DIVIDE, 1, 0},  {"*", VK_MULTIPLY, 2, 0},       {"-", VK_SUBTRACT, 3, 0},
    {"7", VK_NUMPAD7, 0, 1},   {"8", VK_NUMPAD8, 1, 1}, {"9", VK_NUMPAD9, 2, 1},        {"+", VK_ADD, 3, 1, 1, 2},
    {"4", VK_NUMPAD4, 0, 2},   {"5", VK_NUMPAD5, 1, 2}, {"6", VK_NUMPAD6, 2, 2},        {"1", VK_NUMPAD1, 0, 3},
    {"2", VK_NUMPAD2, 1, 3},   {"3", VK_NUMPAD3, 2, 3}, {"ENT", VK_RETURN, 3, 3, 1, 2}, {"0", VK_NUMPAD0, 0, 4, 2},
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

float FKeyX(int index, bool compact)
{
    if (compact) return 1.f + index;
    constexpr float kGapBetweenFours = 0.5f;
    const int groupOfFour = index / 4;
    return 2.f + index + kGapBetweenFours * groupOfFour;
}

void AddFunctionRow(Grid& g, float oy, bool compact)
{
    g.Put("ESC", VK_ESCAPE, 0.f, oy);
    for (int i = 0; i < 12; i++)
        g.Put(kFKeyNames[i], VK_F1 + i, FKeyX(i, compact), oy);
    const float systemKeysX = compact ? 13.f : kNavClusterX;
    g.Put("PRT", VK_SNAPSHOT, systemKeysX, oy);
    g.Put("SLK", VK_SCROLL, systemKeysX + 1.f, oy);
    g.Put("PSE", VK_PAUSE, systemKeysX + 2.f, oy);
}

void AddExtraRows(Grid& g)
{
    constexpr UINT kVkNumpad5WithoutNumLock = VK_CLEAR;
    constexpr UINT kVkCtrlPause = VK_CANCEL;
    constexpr float kAbovePauseX = kNavClusterX + 2.f;
    static constexpr KeyboardKey kBrowser[] = {
        {"W<", VK_BROWSER_BACK, kNavClusterX, 0},          {"W>", VK_BROWSER_FORWARD, kNavClusterX + 1.f, 0},
        {"WHOM", VK_BROWSER_HOME, kNavClusterX + 2.f, 0},  {"WRLD", VK_BROWSER_REFRESH, kNumpadX, 0},
        {"WSTP", VK_BROWSER_STOP, kNumpadX + 1.f, 0},      {"WSRC", VK_BROWSER_SEARCH, kNumpadX + 2.f, 0},
        {"WFAV", VK_BROWSER_FAVORITES, kNumpadX + 3.f, 0},
    };
    struct NamedKey {
        const char* name;
        UINT vk;
    };
    static constexpr NamedKey kMedia[] = {
        {"MUTE", VK_VOLUME_MUTE},      {"VOL-", VK_VOLUME_DOWN},         {"VOL+", VK_VOLUME_UP},
        {"PLAY", VK_MEDIA_PLAY_PAUSE}, {"PREV", VK_MEDIA_PREV_TRACK},    {"NEXT", VK_MEDIA_NEXT_TRACK},
        {"STOP", VK_MEDIA_STOP},       {"MSEL", VK_LAUNCH_MEDIA_SELECT}, {"MAIL", VK_LAUNCH_MAIL},
        {"APP1", VK_LAUNCH_APP1},      {"APP2", VK_LAUNCH_APP2},
    };

    g.Put("SLP", VK_SLEEP, 0.f, 0.f);
    for (int i = 0; i < 12; i++)
        g.Put(kFKeyNames[12 + i], VK_F13 + i, FKeyX(i, false), 0.f);
    g.PutBlock(kBrowser, 0.f, 0.f);

    g.Put("CLR", kVkNumpad5WithoutNumLock, 0.f, 1.f);
    for (int i = 0; i < (int)std::size(kMedia); i++)
        g.Put(kMedia[i].name, kMedia[i].vk, FKeyX(i, false), 1.f);
    g.Put("BRK", kVkCtrlPause, kAbovePauseX, 1.f);
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
    static constexpr const char* kNames[KeyboardForm_Count] = {"60%", "65%", "75%", "TKL 80%", "Full 100%", "Extra"};
    return NameOf(form, kNames);
}

const char* KeyboardVariantName(KeyboardVariant variant)
{
    static constexpr const char* kNames[KeyboardVariant_Count] = {"ANSI", "ISO", "JIS"};
    return NameOf(variant, kNames);
}

const char* MouseFormName(MouseForm form)
{
    static constexpr const char* kNames[MouseForm_Count] = {"Hidden", "3 buttons", "5 buttons"};
    return NameOf(form, kNames);
}
