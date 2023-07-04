#include "MainWindow.h"
#include "App.h"

MainWindow* MainWindow::_self = NULL;
MainWindow& MainWindow::instance() { return *_self; }

MainWindow::MainWindow(const wchar_t* className, const wchar_t* wndName)
	: Window(className, wndName), _selectedAction(Action_Spammy),
		_editPause(false)
{
	_self = this;

	wchar_t path[MAX_PATH] = { 0 };
	GetModuleFileNameW(NULL, path, std::size(path));
	_appFilePath = path;
}

MainWindow::~MainWindow()
{

}

bool MainWindow::initialize()
{
	HWND hwnd = FindWindowW(L"" APP_NAME, L"" APP_NAME);
	if (hwnd) {
		PostMessageW(hwnd, WM_USER_FOCUS, NULL, NULL);
		return false;
	}

	ErrorCode ec = Window::initialize();
	if (ec != ErrorCode_OK) {
		MessageBoxA(NULL, formatError(ec), APP_NAME, MB_ICONERROR | MB_OK);
		return false;
	}

	setRenderBackend(new WindowRenderer_D3d9());
	setIcon(IDI_ICON1);
	enableMoving();
	enableMenuBar();
	setSize({ 680, 435 });
	resetPosition();

	TrayIcon* trayIcon = new TrayIcon();
	trayIcon->setTip(L"" APP_NAME);
	trayIcon->setOnClick(std::bind_front(&MainWindow::onTrayClick, this));
	trayIcon->setMenu(std::bind_front(&MainWindow::onTrayMenu, this));
	setTrayIcon(trayIcon);

	loadStyle();

	return true;
}

bool MainWindow::handleKeyPress(unsigned short vkCode, bool)
{
	if (_editPause) return true;
	return false;
}

bool MainWindow::handleKeyRelease(unsigned short vkCode, bool)
{
	if (_editPause) {
		if (auto profile = sApp.editingProfile())
			profile->vkPause = MAKE_KEY_BUNDLE(vkCode, sKeyboard.testModifiers());
		_editPause = false;
		return true;
	}
	return false;
}

void MainWindow::onTrayClick()
{
	if (!isShown()) {
		show();
		focus();
	} else {
		hide();
	}
}

void MainWindow::onTrayMenu(TrayIconMenu& menu)
{
	menu.disabled(L"" APP_NAME);
	if (!isShown())
		menu.button(L"Show", std::bind_front(&MainWindow::show, this));
	else
		menu.button(L"Hide", std::bind_front(&MainWindow::hide, this));
	menu.toggle(L"Enable", sApp.isEnabled(), [] { sApp.enable(!sApp.isEnabled()); });
	menu.button(L"Exit", std::bind_front(&MainWindow::close, this));
}

void MainWindow::loadStyle()
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowTitleAlign = { 0.5f, 0.5f };
	style.WindowPadding = ImVec2(12.00f, 10.00f);
	style.FramePadding = ImVec2(8.00f, 3.00f);
	style.CellPadding = ImVec2(6.00f, 6.00f);
	style.ItemSpacing = ImVec2(8.00f, 5.00f);
	style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
	style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
	style.IndentSpacing = 25;
	style.WindowBorderSize = 0;
	style.ChildBorderSize = 1;
	style.PopupBorderSize = 1;
	style.FrameBorderSize = 1;
	style.TabBorderSize = 1;
	style.WindowRounding = 0;
	style.ChildRounding = 4;
	style.FrameRounding = 3;
	style.PopupRounding = 4;
	style.ScrollbarRounding = 9;
	style.GrabRounding = 3;
	style.LogSliderDeadzone = 4;
	style.TabRounding = 4;

	ImGui::StyleColorsLight();
}

static const char* s_keyMods[] = {
	"None",
	"Shift", "Alt", "Shift+Alt", "Ctrl",
	"Shift+Ctrl", "Alt+Ctrl", "Shift+Alt+Ctrl"
};

void MainWindow::draw()
{
	auto editingProfile = sApp.editingProfile();
	if (ImGui::BeginMenuBar()) {
		char bufProfilesName[96] = { 0 };
		snprintf(bufProfilesName, std::size(bufProfilesName), "%s##Profiles",
			editingProfile ? editingProfile->name.c_str() : "Profiles");
		if (ImGui::BeginMenu(bufProfilesName)) {
			if (ImGui::BeginMenu("Create")) {
				static char bufName[64] = { 0 };
				if (ImGui::IsWindowAppearing()) bufName[0] = '\0';
				ImGui::PushItemWidth(120.f);
				bool enter = ImGui::InputTextWithHint("##test", "Enter name", bufName, std::size(bufName), ImGuiInputTextFlags_EnterReturnsTrue);
				ImGui::PopItemWidth();
				bool validName = strlen(bufName) >= 3 && !sApp.isProfileExists(bufName);
				if (!validName) ImGui::BeginDisabled();
				if ((ImGui::MenuItem("Save") || enter)) {
					sApp.createProfile(bufName);
					ImGui::CloseCurrentPopup();
				}
				if (!validName) ImGui::EndDisabled();
				ImGui::EndMenu();
			}
			ImGui::Separator();
			const auto& profiles = sApp.getProfiles();
			if (!profiles.empty()) {
				const char* pendingDelete = NULL;
				for (const std::shared_ptr<Profile>& item : profiles) {
					bool activate = false;
					if (ImGui::BeginMenu(item->name.c_str())) {
						if (ImGui::MenuItem("Select"))
							activate = true;
						if (ImGui::BeginMenu("Delete")) {
							ImGui::MenuItem("No");
							if (ImGui::MenuItem("Yes"))
								pendingDelete = item->name.c_str();
							ImGui::EndMenu();
						}
						ImGui::EndMenu();
					}
					if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
						activate = true;
						ImGui::CloseCurrentPopup();
					}
					if (activate) sApp.editingProfile(item->name.c_str());
				}
				if (pendingDelete) {
					sApp.deleteProfile(pendingDelete);
					editingProfile = NULL;
				}
			}
			else {
				ImGui::BeginDisabled();
				ImGui::MenuItem("Empty");
				ImGui::EndDisabled();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Settings")) {
			bool startWithWindows = sApp.isAutoStartEnabled();
			if (ImGui::MenuItem("Start with windows", NULL, &startWithWindows))
				sApp.enableAutoStart(startWithWindows);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("About")) {
			if (ImGui::MenuItem("GitHub repo")) LaunchUrl(L"https://github.com/FrostAtom/spammy");
			if (ImGui::MenuItem("Discord")) LaunchUrl(L"https://discord.gg/NNnBTK5c8e");
			if (ImGui::MenuItem("Author")) LaunchUrl(L"https://t.me/boredatom");
			ImGui::EndMenu();
		}
		ImGui::BeginMenu(__DATE__, false);
		ImGui::EndMenuBar();
	}

	if (ImGui::BeginTable("Header", 3, 0, { 0.f, 60.f })) {
		ImGui::TableSetupColumn("1", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("2", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("3", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (editingProfile) {
			if (_selectedAction == Action_Spammy) {
				if (ImGui::Button("Mode: Spam##Mode", { 75.f, 0 }))
					_selectedAction = Action_Speedy;
				ImGui::Tip("While the button is pressed, key spamming is simulated");
			}
			else {
				if (ImGui::Button("Mode: Speed##Mode", { 75.f, 0 }))
					_selectedAction = Action_Spammy;
				ImGui::Tip("When the button is pressed, it is immediately released");
			}

			bool spammyMode = _selectedAction == Action_Spammy;
			if (!spammyMode) ImGui::BeginDisabled();
			ImGui::SameLine();
			ImGui::PushItemWidth(140.f);
			ImGui::SliderInt("##Frequency", (int*)&editingProfile->speed, 10, 1000, "Frequency: %d");
			ImGui::PopItemWidth();
			ImGui::Tip("Delay between spams in miliseconds");
			if (!spammyMode) ImGui::EndDisabled();

			ImGui::Checkbox("Disable Win", &editingProfile->disableWin);
			ImGui::Tip("Disables Windows button");
			ImGui::SameLine();;
			ImGui::Checkbox("Disable Alt-F4", &editingProfile->disableAltF4);
			ImGui::Tip("Disables Alt-F4 combination");
		}
		ImGui::TableNextColumn();
		ImGui::TableNextColumn();
		{
			float width = 80.f;
			if (editingProfile) {
				if (_editPause) {
					if (ImGui::Button("Press any key##pause", { width, 0.f }))
						_editPause = false;
				} else {
					const char* keyName = (editingProfile && editingProfile->vkPause) ? sKeyboard.geyKeyName(GET_KEY_VKCODE(editingProfile->vkPause)) : "not set";
					if (!keyName[0]) keyName = "Unknown";
					char buf[32];
					snprintf(buf, std::size(buf), "Pause: %s##pause", keyName);
					if (ImGui::Button(buf, { width, 0.f })) {
						_editPause = true;
						editingProfile->vkPause = 0;
					}
				}
				ImGui::SameLine();
			}
			if (sApp.isEnabled()) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.35f, .8f, .35f, 1.f));
				if (ImGui::Button("Enabled", { width, 0.f }))
					sApp.enable(false);
			} else {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.8f, .35f, .35f, 1.f));
				if (ImGui::Button("Disabled", { width, 0.f }))
					sApp.enable(true);
			}
			if (editingProfile) {
				bool colored = false;
				char bufPreviewEnableFor[64];
				if (editingProfile->apps.empty()) {
					ImGui::PushStyleColorTriplet(ImGuiCol_FrameBg, ImGui::FlashColor(.95f, .35f, .35f, 2.f, .2f, .8f));
					colored = true;
					strncpy(bufPreviewEnableFor, "Select apps", std::size(bufPreviewEnableFor));
				}
				else {
					snprintf(bufPreviewEnableFor, std::size(bufPreviewEnableFor), "%d apps selected", editingProfile->apps.size());
				}
				ImGui::PushItemWidth(width + width + ImGui::GetStyle().ItemSpacing.x);
				if (ImGui::BeginCombo("##enablefor", bufPreviewEnableFor, ImGuiComboFlags_NoArrowButton)) {
					static std::vector<std::string> appList;
					if (ImGui::IsWindowAppearing()) {
						appList.clear();
						EnumWindows([this, editingProfile](HWND hwnd) -> BOOL {
							std::filesystem::path path = GetProcessPath(hwnd);
							if (!path.has_filename() || path == _appFilePath) return TRUE;
							std::string fileName = (const char*)path.filename().u8string().c_str();
							if (std::find(editingProfile->apps.begin(), editingProfile->apps.end(), fileName)
								!= editingProfile->apps.end()) return TRUE; // already added programs
							if (std::find(appList.begin(), appList.end(), fileName) != appList.end()) return TRUE; // dublicates
							appList.emplace_back(std::move(fileName));
							return TRUE;
						});
						LexicographicalSort(appList);
					}

					const char* unbindApp = NULL;
					for (std::string& app : editingProfile->apps) {
						if (ImGui::Selectable(app.c_str(), true))
							unbindApp = app.c_str();
					}
					if (unbindApp) sApp.unbindProfile(editingProfile->name.c_str(), unbindApp);

					for (const std::string& app : appList) {
						auto appProfile = sApp.findProfileByApp(app.c_str());
						bool enabledNotForThis = appProfile && editingProfile != appProfile;
						if (enabledNotForThis) ImGui::BeginDisabled();
						if (ImGui::Selectable(app.c_str()))
							sApp.bindProfile(editingProfile->name.c_str(), app.c_str());
						if (enabledNotForThis) ImGui::EndDisabled();
					}
					ImGui::EndCombo();
				}
				ImGui::PopItemWidth();
				if (colored) ImGui::PopStyleColorTriplet();
			}
			ImGui::PopStyleColor();
		}
	}
	ImGui::EndTable();

	static ImVec4 ButtonNormal = { 1.f, 1.f, 1.f, 1.f };
	static ImVec4 ButtonEnabled = { .4f, .4f, 1.f, 1.f };
	static ImVec4 ButtonMode[] = {
		{},
		{1.f, 0.f, 0.f, 1.f},
		{1.f, .5f, 0.f, 1.f},
		{1.f, 1.f, 0.f, 1.f},
		{0.f, 1.f, 0.f, 1.f},
	};

	const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	ImGui::PushStyleColor(ImGuiCol_ChildBg, { .5f, .5f, .5f, .35f });
	if (ImGui::BeginChild("Keyboard", { 0.f, -footer_height_to_reserve }, true, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.f, 0.f });
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, { .2f, .2f });
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
		ImGui::PushStyleColor(ImGuiCol_Button, ButtonNormal);
		float KeySize = 38.f;
		float KeySpacing = KeySize * (1.f / 8.f);

		unsigned mods = sKeyboard.testModifiers();
		for (size_t i = 0; i < std::size(KeyboardLayout); i++) {
			KeyboardItem& key = KeyboardLayout[i];
			if (i > 0) {
				KeyboardItem& prev = KeyboardLayout[i - 1];
				if (prev.type == KIT_Key || prev.type == KIT_Spacing)
					ImGui::SameLine();
				if (prev.type != KIT_NewLine) {
					ImGui::Dummy({ KeySpacing, KeySize });
					if (key.type != KIT_NewLine)
						ImGui::SameLine();
				}
			}
			switch (key.type) {
			case KIT_NewLine:
				ImGui::Dummy({ ImGui::GetContentRegionAvail().x, KeySpacing + KeySize * key.width });
				break;
			case KIT_Spacing:
				ImGui::Dummy({ KeySize * key.width, KeySize });
				break;
			case KIT_Key: {
				bool disabled = sKeyboard.isModifier(key.vkCode) || (key.vkCode == VK_LWIN || key.vkCode == VK_RWIN);
				if (disabled) ImGui::BeginDisabled();

				bool hasColor = false;
				ImVec4 color;
				KeyConfig* cfg = editingProfile ? &editingProfile->keys[key.vkCode][mods] : NULL;

				bool pressed = sKeyboard.isPressed(key.vkCode);
				if (pressed) {
					color = ImColor(ImGui::GetColorU32(ImGuiCol_ButtonActive));
					hasColor = true;
				} else if (cfg) {
					if (cfg->action != Action_None) {
						color = ButtonMode[cfg->action];
						hasColor = true;
					}
					else if (mods != KeyMod_None) {
						KeyConfig& parentCfg = editingProfile->keys[key.vkCode][KeyMod_None];
						if (parentCfg.action != Action_None) {
							color = ButtonMode[parentCfg.action];
							color.w *= .66f;
							hasColor = true;
						}
					}
				}
				if (hasColor) ImGui::PushStyleColor(ImGuiCol_Button, color);

				char buf[32];
				snprintf(buf, std::size(buf), "%s##%02x", key.name, i);
				if (ImGui::Button(buf, { KeySize * key.width, KeySize }) && cfg) {
					cfg->action = cfg->action != _selectedAction ? _selectedAction : Action_None;
				}

				if (hasColor) ImGui::PopStyleColor();
				if (disabled) ImGui::EndDisabled();

				if (editingProfile) {
					snprintf(buf, std::size(buf), "##popup%02x", i);
					if (ImGui::BeginPopupContextItem(buf)) {
						static DWORD mods;
						if (ImGui::IsWindowAppearing()) mods = sKeyboard.testModifiers();
						ImGui::MenuItem(s_keyMods[mods], NULL, false, false);

						KeyConfig& cfg = editingProfile->keys[key.vkCode][mods];
						if (cfg.action != Action_Disabled) {
							if (ImGui::BeginMenu("Mode")) {
								static const char* s_modes[] = {
									NULL,
									NULL,
									"Spammy",
									"Speedy",
								};
								for (int i = 2; i < std::size(s_modes); i++) {
									bool selected = false;
									if (ImGui::MenuItem(s_modes[i], NULL, &selected))
										cfg.action = (Action)i;
								}
								ImGui::EndMenu();
							}

							if (mods != KeyMod_None) {
								if (ImGui::MenuItem("Disable"))
									cfg.action = Action_Disabled;
							}
						}
						else {
							if (ImGui::MenuItem("Enable##Disable"))
								cfg.action = Action_None;
						}
						ImGui::MenuItem("Close");
						ImGui::EndPopup();
					}
				}
				break;
			}
			}
		}
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();

	if (ImGui::BeginTable("Footer", 3)) {
		ImGui::TableSetupColumn("1", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("2", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("3", ImGuiTableColumnFlags_WidthFixed);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		ImGui::ColorButton("Disabled", ButtonMode[Action_Disabled], ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop);
		ImGui::SameLine();
		ImGui::TextUnformatted("Disabled");

		ImGui::SameLine();
		ImGui::ColorButton("Spammy", ButtonMode[Action_Spammy], ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop);
		ImGui::SameLine();
		ImGui::TextUnformatted("Spammy");

		ImGui::SameLine();
		ImGui::ColorButton("Speedy", ButtonMode[Action_Speedy], ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop);
		ImGui::SameLine();
		ImGui::TextUnformatted("Speedy");

		ImGui::TableNextColumn();
		ImGui::TableNextColumn();
		DWORD mods = sKeyboard.testModifiers();
		if (mods != KeyMod_None)
			ImGui::TextUnformatted(s_keyMods[mods]);
	}
	ImGui::EndTable();
}

bool MainWindow::handleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
	if (Window::handleWndProc(msg, wParam, lParam, result)) return true;
	switch (msg) {
	case WM_USER_FOCUS: {
		if (!isShown())
			show();
		focus();
		return true;
	}
	}
	return false;
}