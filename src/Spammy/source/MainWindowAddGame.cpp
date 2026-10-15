#include "App.h"
#include "Config.h"
#include "MainWindow.h"
#include "Utils.h"
#include "Win32/Keyboard.h"

using namespace ImGui;
using namespace MainWindowUtil;

namespace {
constexpr ImVec2 kRowsPos(44.f, 214.f);
constexpr ImVec2 kRowSize(696.f, 56.f);
constexpr float kRowPitch = 66.f;
constexpr ImVec2 kAppIconOffset(12.f, 12.f);
constexpr ImVec2 kAppIconSize(32.f, 32.f);
constexpr DWORD kRefreshMs = 2000;
constexpr char kFirstGameAutofireFrom = '1';
constexpr char kFirstGameAutofireTo = '5';

constexpr float RowsHeight(size_t rowCount)
{
    return (float)(rowCount - 1) * kRowPitch + kRowSize.y + kUiCapLipDepth;
}

constexpr size_t kVisibleRows = 5;
constexpr float kRowsHeight = RowsHeight(kVisibleRows);

bool SameExe(std::string_view a, std::string_view b)
{
    const CaseInsensitiveLess less{};
    return !less(a, b) && !less(b, a);
}

std::string ToUpper(std::string_view text)
{
    std::wstring wide = Utf8ToWide(text);
    CharUpperBuffW(wide.data(), (DWORD)wide.size());
    return WideToUtf8(wide);
}

std::string FirstLetter(const std::string& name)
{
    if (name.empty()) return "?";
    unsigned c = 0;
    const int len = ImTextCharFromUtf8(&c, name.data(), name.data() + name.size());
    return ToUpper(std::string_view(name.data(), (size_t)len));
}

void AddTextCentered(ImDrawList* dl, ImFont* font, float size, const ImVec2& min, const ImVec2& max, ImU32 col,
                     const char* text)
{
    const float w = UiTextWidth(font, size, text);
    const float h = font->CalcTextSizeA(size, FLT_MAX, 0.f, text).y;
    dl->AddText(font, size, ImFloor(ImVec2((min.x + max.x - w) * .5f, (min.y + max.y - h) * .5f)), col, text);
}

void DrawCartridge(ImDrawList* dl, const ImVec2& o, const std::string& name, const std::string& exe)
{
    constexpr float kCartridgeLip = 2.f * kUiCapLipDepth;
    const ImVec2 bodyMin = o + ImVec2(846.f, 96.f);
    const ImVec2 bodyMax = bodyMin + ImVec2(320.f, 268.f);
    dl->AddRectFilled(bodyMin + ImVec2(0.f, kCartridgeLip), bodyMax + ImVec2(0.f, kCartridgeLip), UiCol.CapLip, 14.f);
    dl->AddRectFilled(bodyMin, bodyMax, UiCol.Cap, 14.f);
    dl->AddRect(bodyMin, bodyMax, UiCol.CapStroke, 14.f);
    dl->AddRectFilled(o + ImVec2(956.f, 336.f), o + ImVec2(1056.f, 346.f), UiCol.CapLip, 5.f);
    for (const float left : {886.f, 1054.f})
        for (int i = 0; i < 7; i++) {
            const float x = left + 12.f * i;
            dl->AddLine(o + ImVec2(x, 112.f), o + ImVec2(x, 134.f), UiCol.CapLip, 3.f);
        }

    dl->AddRectFilled(o + ImVec2(874.f, 152.f), o + ImVec2(1138.f, 312.f), UiCol.Tape, 8.f);
    dl->AddRectFilled(o + ImVec2(874.f, 152.f), o + ImVec2(1138.f, 162.f), UiCol.Signal, 8.f,
                      ImDrawFlags_RoundCornersTop);

    constexpr float kLabelW = 228.f, kTitleY = 212.f;
    std::string title = ToUpper(name);
    float size = 22.f;
    while (size > 14.f && UiTextWidth(UiFonts::Heavy, size, title.c_str(), .5f) > kLabelW)
        size -= 1.f;
    title = FitText(UiFonts::Heavy, size, std::move(title), kLabelW, .5f);
    AddTrackedText(dl, UiFonts::Heavy, size, o + ImVec2(892.f, ImFloor(kTitleY + (22.f - size) * .5f)), UiCol.TapeInk,
                   title.c_str(), .5f);
    const std::string file = FitText(UiFonts::Bold, 10.f, ToUpper(exe), kLabelW, 1.4f);
    AddTrackedText(dl, UiFonts::Bold, 10.f, o + ImVec2(892.f, kTitleY + 30.f), UiCol.TapeInk2, file.c_str(), 1.4f);
}

void AddSelectedRowFrame(ImDrawList* dl, const ImVec2& pos)
{
    const ImVec2 lip(0.f, kUiCapLipDepth);
    dl->AddRectFilled(pos + lip, pos + kRowSize + lip, UiCol.SignalLip, 10.f);
    dl->AddRectFilled(pos, pos + kRowSize, UiCol.Cap, 10.f);
    dl->AddRect(pos + ImVec2(1.f, 1.f), pos + kRowSize - ImVec2(1.f, 1.f), UiCol.Signal, 9.f, 0, 2.f);
}

void ApplyFirstGameDefaults(Profile& game, KeyBundle_t pauseKey)
{
    for (unsigned short vk = kFirstGameAutofireFrom; vk <= kFirstGameAutofireTo; vk++)
        game.keys[vk][KeyMod_None].action = Action_Spammy;
    game.pauseKey = pauseKey;
}
} // namespace

void MainWindow::DrawAddGame(ImDrawList* dl, const ImVec2& o, bool firstRun)
{
    static bool s_autoStart = false;

    PushID("##addgame");
    const DWORD now = GetTickCount();
    const bool refreshDue = !_addGame.refreshTicks || now - *_addGame.refreshTicks > kRefreshMs;
    if (refreshDue) {
        _addGame.runningApps = ListRunningApps();
        _addGame.refreshTicks = now;
        if (firstRun) s_autoStart = sApp.IsAutoStartEnabled(); // the settings popup can change it meanwhile
    }

    std::vector<const RunningApp*> pickable;
    if (_addGame.browsed) pickable.push_back(&*_addGame.browsed);
    for (const RunningApp& app : _addGame.runningApps) {
        if (_addGame.browsed && SameExe(app.exeName, _addGame.browsed->exeName)) continue;
        const bool belongsToAGame = sConfig.FindProfileByApp(app.exeName.c_str()) != nullptr;
        if (!belongsToAGame) pickable.push_back(&app);
    }
    const auto pick = std::ranges::find_if(
        pickable, [&](const RunningApp* app) { return SameExe(app->exeName, _addGame.selectedExe); });
    const RunningApp* selected = pick != pickable.end() ? *pick : nullptr;
    const bool hasPick = selected != nullptr;
    if (!hasPick) _addGame.selectedExe.clear();

    AddSilk(dl, o + ImVec2(24.f, 80.f), firstRun ? "welcome to spammy" : "add a game");
    dl->AddText(UiFonts::Heavy, 28.f, o + ImVec2(24.f, 96.f), UiCol.Ink,
                firstRun ? "Which game do you play?" : "Which game should it work in?");
    dl->AddText(UiFonts::Medium, 14.f, o + ImVec2(24.f, 136.f), UiCol.Ink2,
                "Start the game, then pick it here. It gets its own keys, rate and locks.");

    dl->AddRectFilled(o + ImVec2(24.f, 176.f), o + ImVec2(760.f, 624.f), UiCol.Well, 16.f);
    dl->AddLine(o + ImVec2(40.f, 177.f), o + ImVec2(744.f, 177.f), UiCol.WellEdge, 2.f);
    AddSilk(dl, o + ImVec2(44.f, 194.f), "running now");

    const RunningApp* clickedApp = nullptr;
    float scrollY = 0.f, scrollMax = 0.f;
    const float contentH = pickable.empty() ? 0.f : RowsHeight(pickable.size());
    SetCursorScreenPos(o + kRowsPos);
    SetNextWindowContentSize(ImVec2(kRowSize.x, contentH));
    const ImGuiWindowFlags listFlags =
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (BeginChild("##games", ImVec2(kRowSize.x, kRowsHeight), ImGuiChildFlags_None, listFlags)) {
        ImDrawList* cdl = GetWindowDrawList();
        constexpr float kScrollPerWheelNotch = kRowPitch;
        if (const float wheel = GetIO().MouseWheel; wheel != 0.f && IsWindowHovered())
            SetScrollY(ImClamp(GetScrollY() - wheel * kScrollPerWheelNotch, 0.f, GetScrollMaxY()));

        const ImVec2 scrolledOrigin = GetCursorScreenPos();
        const int iconPx = (int)ceilf(kAppIconSize.x * GetIO().DisplayFramebufferScale.x);
        for (size_t i = 0; i < pickable.size(); i++) {
            const RunningApp& app = *pickable[i];
            const bool isSelected = pickable[i] == selected;
            const ImVec2 pos = scrolledOrigin + ImVec2(0.f, (float)i * kRowPitch);
            PushID(app.exeName.c_str());

            ImVec2 face = pos;
            float hoverT = 0.f;
            if (isSelected) {
                AddSelectedRowFrame(cdl, pos);
            } else {
                const UiCapHit hit = UiCapFrame("##row", pos, kRowSize, UiCapKind_Plain, 10.f);
                if (hit.clicked) clickedApp = &app;
                face = hit.face;
                hoverT = hit.hoverT;
            }

            const ImVec2 iconMin = face + kAppIconOffset, iconMax = iconMin + kAppIconSize;
            if (const std::optional<ImTextureRef> icon = AppIcon(app.exePath, iconPx)) {
                cdl->AddImage(*icon, iconMin, iconMax);
            } else {
                cdl->AddRectFilled(iconMin, iconMax, isSelected ? UiCol.Signal : UiCol.Tap, 7.f);
                const std::string letter = FirstLetter(app.friendlyName);
                AddTextCentered(cdl, UiFonts::Heavy, 15.f, iconMin, iconMax,
                                isSelected ? UiCol.White : UiCol.InkInverse, letter.c_str());
            }

            const std::string name = FitText(UiFonts::Bold, 14.f, app.friendlyName, 520.f);
            const std::string exe = FitText(UiFonts::Medium, 12.f, app.exeName, 520.f);
            cdl->AddText(UiFonts::Bold, 14.f, face + ImVec2(58.f, 10.f), UiCol.Ink, name.c_str());
            cdl->AddText(UiFonts::Medium, 12.f, face + ImVec2(58.f, 30.f), UiCol.Ink3, exe.c_str());

            if (isSelected) {
                AddSilk(cdl, face + ImVec2(600.f, 24.f), "selected", UiCol.Signal);
                cdl->AddCircleFilled(face + ImVec2(672.f, 28.f), 4.f, UiCol.Signal);
            } else {
                const float w = UiTextWidth(UiFonts::Bold, 12.f, "Pick");
                cdl->AddText(UiFonts::Bold, 12.f, face + ImVec2(676.f - w, 20.f),
                             UiMixColor(UiCol.Ink2, UiCol.Ink, hoverT), "Pick");
            }
            PopID();
        }
        if (pickable.empty())
            AddTextCentered(cdl, UiFonts::Medium, 13.f, scrolledOrigin,
                            scrolledOrigin + ImVec2(kRowSize.x, kRowsHeight), UiCol.Ink3,
                            "Nothing running yet. Start the game, then it shows up here.");
        scrollY = GetScrollY();
        scrollMax = GetScrollMaxY();
    }
    EndChild();

    if (scrollMax > 0.f) {
        const float thumbH = ImMax(24.f, kRowsHeight * kRowsHeight / (kRowsHeight + scrollMax));
        const float thumbY = kRowsPos.y + (kRowsHeight - thumbH) * (scrollY / scrollMax);
        dl->AddRectFilled(o + ImVec2(748.f, thumbY), o + ImVec2(752.f, thumbY + thumbH), UiWithAlpha(UiCol.Ink3, .5f),
                          2.f);
    }

    const char* rejection = _addGame.browseRejection.Visible(now);
    const bool browseOnceDrawn = UiBrowseButton(
        "##browse", o + ImVec2(44.f, 548.f), ImVec2(kRowSize.x, 42.f), "Browse for the game\xE2\x80\x99s .exe file",
        rejection ? rejection : "Not in the list?", rejection ? UiCol.Block : UiCol.Ink3);

    DrawCartridge(dl, o, hasPick ? selected->friendlyName : "Pick a game",
                  hasPick ? _addGame.selectedExe : "from the list");

    if (firstRun) {
        AddSilk(dl, o + ImVec2(822.f, 412.f), "setup");
        if (UiToggle("##autostart", o + ImVec2(822.f, 432.f), s_autoStart) && sApp.EnableAutoStart(!s_autoStart))
            s_autoStart = !s_autoStart;
        dl->AddText(UiFonts::SemiBold, 13.f, o + ImVec2(882.f, 433.f), UiCol.Ink, "Start with Windows");
        dl->AddText(UiFonts::Medium, 10.f, o + ImVec2(882.f, 449.f), UiCol.Ink3,
                    s_autoStart ? "starts hidden in the tray" : "off until you turn it on");

        char keyBuf[64];
        const bool capturing = IsCapturingPauseKey();
        const char* keyName = PauseKeyName(_firstGamePauseKey, keyBuf, sizeof(keyBuf));
        ImU32 keyCol = keyName ? UiCol.Ink : UiCol.Signal;
        if (capturing) {
            keyName = "Press\xE2\x80\xA6";
            keyCol = UiFlash(UiCol.Signal, 1.f, .35f);
        } else if (!keyName) {
            keyName = "Set key";
        }
        const float keyNameW = ceilf(UiTextWidth(UiFonts::Bold, 12.f, keyName));
        const float capW = ImMax(70.f, keyNameW + 20.f);
        const UiCapHit hit =
            UiCapFrame("##pausekey", o + ImVec2(822.f, 478.f), ImVec2(capW, 26.f), UiCapKind_Plain, 6.f);
        if (hit.clicked)
            TogglePauseKeyCapture();
        else if (!capturing && IsItemClicked(ImGuiMouseButton_Right))
            _firstGamePauseKey = 0;
        AddTextCentered(dl, UiFonts::Bold, 12.f, hit.face, hit.face + ImVec2(capW, 26.f), keyCol, keyName);
        const float labelX = 822.f + capW + 16.f;
        dl->AddText(UiFonts::SemiBold, 13.f, o + ImVec2(labelX, 479.f), UiCol.Ink, "Pause key");
        dl->AddText(UiFonts::Medium, 10.f, o + ImVec2(labelX, 495.f), UiCol.Ink3,
                    "click, then press any key to change");
    }

    bool confirm = false, cancel = false;
    constexpr float kButtonsUnderSetupY = 570.f, kButtonsUnderCartridgeY = 412.f;
    const float buttonsY = firstRun ? kButtonsUnderSetupY : kButtonsUnderCartridgeY;
    const ImVec2 confirmPos = o + ImVec2(822.f, buttonsY), confirmSize(220.f, 44.f);
    if (hasPick) {
        const std::string label =
            FitText(UiFonts::Bold, 13.f, std::string(firstRun ? "Start with " : "Add ") + selected->friendlyName,
                    confirmSize.x - 24.f);
        confirm = UiButton("##confirm", confirmPos, confirmSize, label.c_str(), UiButtonKind_Primary);
    } else {
        AddCap(dl, confirmPos, confirmSize, UiCapKind_Plain, 8.f);
        AddTextCentered(dl, UiFonts::Bold, 13.f, confirmPos, confirmPos + confirmSize, UiCol.Ink3,
                        "Pick a game first");
    }
    if (!firstRun) {
        cancel =
            UiButton("##cancel", o + ImVec2(1054.f, buttonsY), ImVec2(108.f, 44.f), "Cancel", UiButtonKind_Secondary);
        const bool escapeTakenElsewhere =
            IsAnyItemActive() || IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
        if (IsKeyPressed(ImGuiKey_Escape, false) && !escapeTakenElsewhere) cancel = true;
    }

    const ImVec2 footer = GetWindowPos() + ImVec2(0.f, WindowSize().y - 40.f);
    dl->AddLine(footer, footer + ImVec2(WindowSize().x, 0.f), UiCol.Line);
    dl->AddText(UiFonts::Medium, 12.f, footer + ImVec2(24.f, 12.f), UiCol.Ink3,
                firstRun ? "Spammy lives in the tray. Closing the window keeps it working; the tray icon turns grey "
                           "while paused."
                         : "Rename it, add more apps or remove it later from its tab.");
    PopID();

    if (clickedApp) _addGame.selectedExe = clickedApp->exeName;

    if (browseOnceDrawn) {
        const Profile* noGameMayOwnIt = nullptr;
        if (const auto path = BrowseForUnclaimedExe(noGameMayOwnIt, _addGame.browseRejection)) {
            RunningApp app{Utf8FileName(*path), AppFriendlyName(*path), *path};
            _addGame.selectedExe = app.exeName;
            _addGame.browsed = std::move(app);
        }
    }

    if (confirm) {
        if (firstRun) CancelPauseKeyCapture();
        const std::shared_ptr<Profile> game = sConfig.CreateGame(selected->friendlyName, selected->exeName);
        if (game && firstRun) ApplyFirstGameDefaults(*game, _firstGamePauseKey);
        sConfig.MarkDirty();
        CloseAddGame();
    } else if (cancel) {
        CloseAddGame();
    }
}

std::optional<ImTextureRef> MainWindow::AppIcon(const std::filesystem::path& exePath, int sizePx)
{
    const auto [it, firstAsked] = _appIcons.try_emplace({exePath.native(), sizePx});
    if (firstAsked)
        if (const std::optional<IconImage> icon = ExtractExeIcon(exePath, sizePx))
            it->second = CreateTexture(icon->width, icon->height, icon->pixels);
    return it->second;
}
