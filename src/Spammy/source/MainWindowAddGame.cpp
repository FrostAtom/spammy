#include "MainWindow.h"
#include "App.h"
#include "Config.h"
#include "Utils.h"
#include "Win32/Keyboard.h"

using namespace ImGui;

namespace {
constexpr ImVec2 kRowsPos(44.f, 214.f);
constexpr ImVec2 kRowSize(696.f, 56.f);
constexpr float kRowPitch = 66.f;
constexpr float kRowsHeight = 538.f - 214.f; // exactly five rows, lips included
constexpr DWORD kRefreshMs = 2000;
constexpr DWORD kBrowseNoteMs = 4000;
constexpr const char* kEnDash = "\xE2\x80\x93";
constexpr const char* kEllipsis = "\xE2\x80\xA6";

bool SameExe(std::string_view a, std::string_view b)
{
    const CaseInsensitiveLess less{};
    return !less(a, b) && !less(b, a);
}

// Unicode-aware, so "É" or "Ж" in a game name come out uppercased too
std::string ToUpper(std::string_view text)
{
    if (text.empty()) return {};
    const int wideLen = MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0);
    std::wstring wide(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), wide.data(), wideLen);
    CharUpperBuffW(wide.data(), (DWORD)wide.size());
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLen, nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLen, out.data(), len, nullptr, nullptr);
    return out;
}

std::string FirstLetter(const std::string& name)
{
    if (name.empty()) return "?";
    unsigned c = 0;
    const int len = ImTextCharFromUtf8(&c, name.data(), name.data() + name.size());
    return ToUpper(std::string_view(name.data(), (size_t)len));
}

// cut short with an ellipsis until it fits maxW
std::string FitText(ImFont* font, float size, std::string text, float maxW, float tracking = 0.f)
{
    if (UiTextWidth(font, size, text.c_str(), tracking) <= maxW) return text;
    while (!text.empty()) {
        size_t cut = text.size() - 1;
        while (cut && ((unsigned char)text[cut] & 0xC0) == 0x80) // back to the lead byte of a multi-byte char
            cut--;
        text.erase(cut);
        while (!text.empty() && text.back() == ' ')
            text.pop_back();
        const std::string candidate = text + kEllipsis;
        if (UiTextWidth(font, size, candidate.c_str(), tracking) <= maxW) return candidate;
    }
    return kEllipsis;
}

void AddTextCentered(ImDrawList* dl, ImFont* font, float size, const ImVec2& min, const ImVec2& max, ImU32 col,
                     const char* text)
{
    const float w = UiTextWidth(font, size, text);
    const float h = font->CalcTextSizeA(size, FLT_MAX, 0.f, text).y;
    dl->AddText(font, size, ImFloor(ImVec2((min.x + max.x - w) * .5f, (min.y + max.y - h) * .5f)), col, text);
}

unsigned BoundKeyCount(const Profile& profile)
{
    unsigned count = 0;
    for (const Profile::KeyModList_t& mods : profile.keys)
        count += mods[KeyMod_None].action != Action_None;
    return count;
}

const char* LocksText(const Profile& profile)
{
    if (profile.disableWin && profile.disableAltF4) return "Win, Alt+F4";
    if (profile.disableWin) return "Win";
    if (profile.disableAltF4) return "Alt+F4";
    return "off";
}

// the new game as a cartridge with a tape label, previewing what it will start with
void DrawCartridge(ImDrawList* dl, const ImVec2& o, const std::string& name, const std::string& exe, const char* keys,
                   const char* rate, const char* locks)
{
    // a cartridge stands taller than a key: an 8px lip where AddCap gives 4
    const ImVec2 bodyMin = o + ImVec2(846.f, 96.f);
    const ImVec2 bodyMax = bodyMin + ImVec2(320.f, 268.f);
    dl->AddRectFilled(bodyMin + ImVec2(0.f, 8.f), bodyMax + ImVec2(0.f, 8.f), UiCol::CapLip, 14.f);
    dl->AddRectFilled(bodyMin, bodyMax, UiCol::Cap, 14.f);
    dl->AddRect(bodyMin, bodyMax, UiCol::CapStroke, 14.f);
    dl->AddRectFilled(o + ImVec2(956.f, 336.f), o + ImVec2(1056.f, 346.f), UiCol::CapLip, 5.f);
    for (const float left : {886.f, 1054.f})
        for (int i = 0; i < 7; i++) {
            const float x = left + 12.f * i;
            dl->AddLine(o + ImVec2(x, 112.f), o + ImVec2(x, 134.f), UiCol::CapLip, 3.f);
        }

    dl->AddRectFilled(o + ImVec2(874.f, 152.f), o + ImVec2(1138.f, 312.f), UiCol::Tape, 8.f);
    dl->AddRectFilled(o + ImVec2(874.f, 152.f), o + ImVec2(1138.f, 162.f), UiCol::Signal, 8.f,
                      ImDrawFlags_RoundCornersTop);

    constexpr float kLabelW = 228.f;
    std::string title = ToUpper(name);
    float size = 22.f;
    while (size > 14.f && UiTextWidth(UiFonts::Heavy, size, title.c_str(), .5f) > kLabelW)
        size -= 1.f;
    title = FitText(UiFonts::Heavy, size, std::move(title), kLabelW, .5f);
    AddTrackedText(dl, UiFonts::Heavy, size, o + ImVec2(892.f, ImFloor(182.f + (22.f - size) * .5f)), UiCol::TapeInk,
                   title.c_str(), .5f);
    const std::string file = FitText(UiFonts::Bold, 10.f, ToUpper(exe), kLabelW, 1.4f);
    AddTrackedText(dl, UiFonts::Bold, 10.f, o + ImVec2(892.f, 212.f), UiCol::TapeInk2, file.c_str(), 1.4f);

    const struct {
        float x;
        const char* caption;
        const char* value;
    } fields[] = {{892.f, "keys", keys}, {976.f, "rate", rate}, {1048.f, "locks", locks}};
    for (const auto& field : fields) {
        AddSilk(dl, o + ImVec2(field.x, 256.f), field.caption, UiCol::TapeInk2);
        dl->AddText(UiFonts::Bold, 14.f, o + ImVec2(field.x, 270.f), UiCol::TapeInk, field.value);
    }
}
} // namespace

void MainWindow::DrawAddGame(ImDrawList* dl, const ImVec2& o, bool firstRun)
{
    static bool s_autoStart = false;
    static std::string s_browseNote; // why the last browsed .exe was not taken
    static DWORD s_browseNoteTicks = 0;
    static const unsigned s_defaultSpeed = Profile().speed;

    PushID("##addgame");
    const DWORD now = GetTickCount();
    if (_addGame.refreshTicks == 0) s_browseNote.clear();
    if (_addGame.refreshTicks == 0 || now - _addGame.refreshTicks > kRefreshMs) {
        _addGame.running = ListRunningApps();
        _addGame.refreshTicks = now ? now : 1; // 0 means "just opened"
        // re-read along with the list: the settings popup can flip it behind our back
        if (firstRun) s_autoStart = sApp.IsAutoStartEnabled();
    }

    // apps that already belong to a game can't start another one
    std::vector<const RunningApp*> rows;
    if (_addGame.browsed) rows.push_back(&*_addGame.browsed);
    for (const RunningApp& app : _addGame.running) {
        if (_addGame.browsed && SameExe(app.exe, _addGame.browsed->exe)) continue;
        if (!sConfig.FindProfileByApp(app.exe.c_str())) rows.push_back(&app);
    }
    const RunningApp* selected = nullptr;
    for (const RunningApp* app : rows)
        if (SameExe(app->exe, _addGame.selectedExe)) {
            selected = app;
            break;
        }
    // the picked game quit (or got bound meanwhile): a pick nobody can see would be a surprise on confirm
    if (!selected) {
        _addGame.selectedExe.clear();
        _addGame.selectedName.clear();
    }
    const bool hasPick = selected != nullptr;

    // a copy reads the current game; held here since CreateGame replaces the editing profile
    const std::shared_ptr<Profile> current = firstRun ? nullptr : sConfig.editingProfile;
    const Profile* source = current && _addGame.copyCurrent ? current.get() : nullptr;

    // header
    AddSilk(dl, o + ImVec2(24.f, 80.f), firstRun ? "welcome to spammy" : "add a game");
    dl->AddText(UiFonts::Heavy, 28.f, o + ImVec2(24.f, 96.f), UiCol::Ink,
                firstRun ? "Which game do you play?" : "Which game should it work in?");
    dl->AddText(UiFonts::Medium, 14.f, o + ImVec2(24.f, 136.f), UiCol::Ink2,
                "Start the game, then pick it here. It gets its own keys, rate and locks.");

    // running list
    dl->AddRectFilled(o + ImVec2(24.f, 176.f), o + ImVec2(760.f, 624.f), UiCol::Well, 16.f);
    dl->AddLine(o + ImVec2(40.f, 177.f), o + ImVec2(744.f, 177.f), UiCol::WellEdge, 2.f);
    AddSilk(dl, o + ImVec2(44.f, 194.f), "running now");

    const RunningApp* clickedApp = nullptr;
    float scrollY = 0.f, scrollMax = 0.f;
    const float contentH = rows.empty() ? 0.f : (float)(rows.size() - 1) * kRowPitch + kRowSize.y + 4.f;
    SetCursorScreenPos(o + kRowsPos);
    SetNextWindowContentSize(ImVec2(kRowSize.x, contentH));
    const ImGuiWindowFlags listFlags =
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (BeginChild("##games", ImVec2(kRowSize.x, kRowsHeight), ImGuiChildFlags_None, listFlags)) {
        ImDrawList* cdl = GetWindowDrawList();
        // a full row per wheel notch instead of ImGui's font-based step
        if (const float wheel = GetIO().MouseWheel; wheel != 0.f && IsWindowHovered())
            SetScrollY(ImClamp(GetScrollY() - wheel * kRowPitch, 0.f, GetScrollMaxY()));

        const ImVec2 base = GetCursorScreenPos(); // already shifted by the scroll
        for (size_t i = 0; i < rows.size(); i++) {
            const RunningApp& app = *rows[i];
            const bool isSelected = rows[i] == selected;
            const ImVec2 pos = base + ImVec2(0.f, (float)i * kRowPitch);
            PushID(app.exe.c_str());

            ImVec2 face = pos;
            float hoverT = 0.f;
            if (isSelected) {
                // a picked row has nothing left to click: drawn by hand with the Signal lip and outline
                cdl->AddRectFilled(pos + ImVec2(0.f, 4.f), pos + kRowSize + ImVec2(0.f, 4.f), UiCol::SignalLip, 10.f);
                cdl->AddRectFilled(pos, pos + kRowSize, UiCol::Cap, 10.f);
                cdl->AddRect(pos + ImVec2(1.f, 1.f), pos + kRowSize - ImVec2(1.f, 1.f), UiCol::Signal, 9.f, 0, 2.f);
            } else {
                const UiCapHit hit = UiCapFrame("##row", pos, kRowSize, UiCapKind_Plain, 10.f);
                if (hit.clicked) clickedApp = &app;
                face = hit.face;
                hoverT = hit.hoverT;
            }

            const ImVec2 iconMin = face + ImVec2(12.f, 12.f);
            cdl->AddRectFilled(iconMin, iconMin + ImVec2(32.f, 32.f), isSelected ? UiCol::Signal : UiCol::Tap, 7.f);
            const std::string letter = FirstLetter(app.name);
            AddTextCentered(cdl, UiFonts::Heavy, 15.f, iconMin, iconMin + ImVec2(32.f, 32.f), UiCol::White,
                            letter.c_str());

            const std::string name = FitText(UiFonts::Bold, 14.f, app.name, 520.f);
            const std::string exe = FitText(UiFonts::Medium, 12.f, app.exe, 520.f);
            cdl->AddText(UiFonts::Bold, 14.f, face + ImVec2(58.f, 10.f), UiCol::Ink, name.c_str());
            cdl->AddText(UiFonts::Medium, 12.f, face + ImVec2(58.f, 30.f), UiCol::Ink3, exe.c_str());

            if (isSelected) {
                AddSilk(cdl, face + ImVec2(600.f, 24.f), "selected", UiCol::Signal);
                cdl->AddCircleFilled(face + ImVec2(672.f, 28.f), 4.f, UiCol::Signal);
            } else {
                const float w = UiTextWidth(UiFonts::Bold, 12.f, "Pick");
                cdl->AddText(UiFonts::Bold, 12.f, face + ImVec2(676.f - w, 20.f),
                             UiMixColor(UiCol::Ink2, UiCol::Ink, hoverT), "Pick");
            }
            PopID();
        }
        if (rows.empty())
            AddTextCentered(cdl, UiFonts::Medium, 13.f, base, base + ImVec2(kRowSize.x, kRowsHeight), UiCol::Ink3,
                            "Nothing running yet. Start the game, then it shows up here.");
        scrollY = GetScrollY();
        scrollMax = GetScrollMaxY();
    }
    EndChild();

    if (scrollMax > 0.f) {
        // there is no scrollbar; a thin thumb in the well's margin tells the list goes on
        const float thumbH = ImMax(24.f, kRowsHeight * kRowsHeight / (kRowsHeight + scrollMax));
        const float thumbY = kRowsPos.y + (kRowsHeight - thumbH) * (scrollY / scrollMax);
        dl->AddRectFilled(o + ImVec2(748.f, thumbY), o + ImVec2(752.f, thumbY + thumbH), UiWithAlpha(UiCol::Ink3, .5f),
                          2.f);
    }

    // browse box
    bool browse = false;
    {
        const ImRect bb(o + ImVec2(44.f, 548.f), o + ImVec2(740.f, 594.f));
        const ImGuiID id = GetID("##browse");
        ItemAdd(bb, id);
        bool hovered = false, held = false;
        browse = ButtonBehavior(bb, id, &hovered, &held);
        if (hovered) dl->AddRectFilled(bb.Min, bb.Max, UiCol::Recess, 10.f);
        dl->AddRect(bb.Min, bb.Max, UiWithAlpha(UiCol::Ink3, .7f), 10.f);
        const bool showNote = !s_browseNote.empty() && now - s_browseNoteTicks < kBrowseNoteMs;
        const char* text =
            showNote ? s_browseNote.c_str() : "Not in the list?  Browse for the game\xE2\x80\x99s .exe file";
        AddTextCentered(dl, UiFonts::SemiBold, 12.f, bb.Min, bb.Max, showNote ? UiCol::Block : UiCol::Ink2, text);
    }

    // cartridge preview
    {
        std::string keys = kEnDash, rate = kEnDash;
        const char* locks = kEnDash;
        if (hasPick) {
            if (firstRun) {
                keys = std::string("1") + kEnDash + "5";
            } else if (source) {
                const unsigned count = BoundKeyCount(*source);
                keys = count == 0 ? "none" : count == 1 ? "1 key" : std::to_string(count) + " keys";
            } else {
                keys = "none";
            }
            rate = std::to_string(SpeedToRate(source ? source->speed : s_defaultSpeed)) + "/s";
            locks = source ? LocksText(*source) : "off";
        }
        DrawCartridge(dl, o, hasPick ? _addGame.selectedName : "Pick a game",
                      hasPick ? _addGame.selectedExe : "from the list", keys.c_str(), rate.c_str(), locks);
    }

    if (firstRun) {
        AddSilk(dl, o + ImVec2(822.f, 412.f), "setup");
        if (UiToggle("##autostart", o + ImVec2(822.f, 432.f), s_autoStart) && sApp.EnableAutoStart(!s_autoStart))
            s_autoStart = !s_autoStart;
        dl->AddText(UiFonts::SemiBold, 13.f, o + ImVec2(882.f, 433.f), UiCol::Ink, "Start with Windows");
        dl->AddText(UiFonts::Medium, 10.f, o + ImVec2(882.f, 449.f), UiCol::Ink3,
                    s_autoStart ? "starts hidden in the tray" : "off until you turn it on");

        char keyBuf[64];
        const bool capturing = IsCapturingPauseKey();
        const char* keyName = MainWindowUtil::PauseKeyName(_setupPauseKey, keyBuf, sizeof(keyBuf));
        ImU32 keyCol = keyName ? UiCol::Ink : UiCol::Signal;
        if (capturing) {
            keyName = "Press\xE2\x80\xA6";
            keyCol = UiFlash(UiCol::Signal, 1.f, .35f);
        } else if (!keyName) {
            keyName = "Set key";
        }
        // grows for long combos like "Ctrl+NumDel" so the name never spills out of the cap
        const float capW = ImMax(70.f, ceilf(UiTextWidth(UiFonts::Bold, 12.f, keyName)) + 20.f);
        const UiCapHit hit =
            UiCapFrame("##pausekey", o + ImVec2(822.f, 478.f), ImVec2(capW, 26.f), UiCapKind_Plain, 6.f);
        if (hit.clicked)
            TogglePauseKeyCapture();
        else if (!capturing && IsItemClicked(ImGuiMouseButton_Right))
            _setupPauseKey = 0;
        AddTextCentered(dl, UiFonts::Bold, 12.f, hit.face, hit.face + ImVec2(capW, 26.f), keyCol, keyName);
        const float labelX = 822.f + capW + 16.f;
        dl->AddText(UiFonts::SemiBold, 13.f, o + ImVec2(labelX, 479.f), UiCol::Ink, "Pause key");
        dl->AddText(UiFonts::Medium, 10.f, o + ImVec2(labelX, 495.f), UiCol::Ink3,
                    "click, then press any key to change");
    } else if (current) {
        AddSilk(dl, o + ImVec2(822.f, 412.f), "start from");
        const std::string copyLabel = FitText(UiFonts::Bold, 12.f, "Copy of " + current->name, 150.f);
        const char* labels[] = {"Empty", copyLabel.c_str()};
        int index = _addGame.copyCurrent ? 1 : 0;
        if (UiSegmented("##startfrom", o + ImVec2(822.f, 432.f), ImVec2(340.f, 34.f), labels, index))
            _addGame.copyCurrent = index == 1;
        const std::string note = _addGame.copyCurrent
                                     ? "Takes " + current->name + "\xE2\x80\x99s keys, rate, locks and pause key."
                                     : "No keys set yet. Copy of " + current->name + " takes its keys, rate and locks.";
        dl->AddText(UiFonts::Medium, 11.f, o + ImVec2(822.f, 478.f), UiCol::Ink3, note.c_str(), nullptr, 340.f);
    }

    // buttons
    bool confirm = false, cancel = false;
    const ImVec2 confirmPos = o + ImVec2(822.f, 570.f), confirmSize(220.f, 44.f);
    if (hasPick) {
        const std::string label = FitText(UiFonts::Bold, 13.f,
                                          std::string(firstRun ? "Start with " : "Add ") + _addGame.selectedName,
                                          confirmSize.x - 24.f);
        confirm = UiButton("##confirm", confirmPos, confirmSize, label.c_str(), UiButtonKind_Primary);
    } else {
        // nothing to add yet: a dead plain cap with greyed text
        AddCap(dl, confirmPos, confirmSize, UiCapKind_Plain, 8.f);
        AddTextCentered(dl, UiFonts::Bold, 13.f, confirmPos, confirmPos + confirmSize, UiCol::Ink3,
                        "Pick a game first");
    }
    if (!firstRun) {
        cancel = UiButton("##cancel", o + ImVec2(1054.f, 570.f), ImVec2(108.f, 44.f), "Cancel", UiButtonKind_Secondary);
        // Escape belongs to an open popup or an active text field first
        if (IsKeyPressed(ImGuiKey_Escape, false) && !IsAnyItemActive() &&
            !IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
            cancel = true;
    }

    // footer
    dl->AddLine(o + ImVec2(0.f, 640.f), o + ImVec2(kWindowSize.x, 640.f), UiCol::Line);
    dl->AddText(UiFonts::Medium, 12.f, o + ImVec2(24.f, 652.f), UiCol::Ink3,
                firstRun ? "Spammy lives in the tray. Closing the window keeps it working; the tray icon turns grey "
                           "while paused."
                         : "Rename it, add more apps or remove it later from its tab.");
    PopID();

    if (clickedApp) {
        _addGame.selectedExe = clickedApp->exe;
        _addGame.selectedName = clickedApp->name;
    }

    // the dialog is modal: run it after everything of this frame has been drawn
    if (browse) {
        std::filesystem::path path;
        if (BrowseForExe(Native(), path)) {
            RunningApp app{Utf8FileName(path), AppFriendlyName(path)};
            if (const std::shared_ptr<Profile> owner = sConfig.FindProfileByApp(app.exe.c_str())) {
                s_browseNote = app.exe + " already belongs to " + owner->name;
                s_browseNoteTicks = GetTickCount();
            } else {
                s_browseNote.clear();
                _addGame.selectedExe = app.exe;
                _addGame.selectedName = app.name;
                _addGame.browsed = std::move(app);
            }
        }
    }

    if (confirm) {
        // a capture still armed would otherwise land on the new game long after setup is gone
        if (firstRun && IsCapturingPauseKey()) TogglePauseKeyCapture();
        const std::shared_ptr<Profile> game =
            sConfig.CreateGame(_addGame.selectedName, _addGame.selectedExe, source);
        if (game && firstRun) {
            // the first game starts out useful: 1-5 on autofire and the pause key picked in setup
            for (unsigned short vk = '1'; vk <= '5'; vk++)
                game->keys[vk][KeyMod_None].action = Action_Spammy;
            game->vkPause = _setupPauseKey;
        }
        sConfig.MarkDirty();
        CloseAddGame();
    } else if (cancel) {
        CloseAddGame();
    }
}
