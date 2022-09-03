#include "App.h"

App* App::_self = NULL;
const char* App::_buildTime = __DATE__;

App::App(int argc, wchar_t* argv[])
	: _isRunning(false), _enabled(false), _window(NULL), _autoStartEnabled(false),
		_editPause(false), _lastUpdate(0), _activeHwnd(NULL), _selectedAction(Action_Spammy),
		_activeProfile(NULL), _editingProfile(NULL)
{
	_self = this;

	wchar_t path[MAX_PATH] = { 0 };
	GetModuleFileNameW(NULL, path, std::size(path));
	_appFilePath = path;

	_window = new Window(L"" APP_NAME);
	if (_window->focusTwin()) return;

	if (!loadConfig()) {
		int action = MessageBoxW(_window->native(), L"Can't load config, reset to defaults?", L"" APP_NAME, MB_ICONQUESTION | MB_OKCANCEL);
		if (action != IDOK) return;
	}
	_autoStartEnabled = isAutoStartEnabled();

	Window::State initialState = (argc > 1 && wcscmp(argv[1], L"autolaunch") == 0) ? Window::State_Hide : Window::State_Normal;
	if (Window::ErrorCode code = _window->create(initialState); code != Window::ErrorCode_OK) {
		MessageBoxA(NULL, _window->formatError(code), APP_NAME, MB_ICONERROR | MB_OK);
		return;
	}
	_window->setRenderBackend(new WindowRenderer_D3d9());
	_window->setIcon(IDI_ICON1);
	_window->setOnUpdate(std::bind_front(&App::onWindowUpdate, this));
	_window->enableMoving(true);
	_window->enableScroll(false);
	_window->enableMenuBar(true);
	_window->setSize({ 680, 435 });
	_window->setPositionCenter();

	TrayIcon* trayIcon = new TrayIcon();
	trayIcon->setTip(L"" APP_NAME);
	trayIcon->setOnClick(std::bind_front(&App::onTrayClick, this));
	trayIcon->setMenu(std::bind_front(&App::onTrayMenu, this));
	_window->setTrayIcon(trayIcon);

	initUIStyle();

	sKeyboard.atttach();
	sKeyboard.onPress(std::bind_front(&App::onKeyPress, this));
	sKeyboard.onRelease(std::bind_front(&App::onKeyRelease, this));
	sWinEvent.on(EVENT_SYSTEM_FOREGROUND, std::bind_front(&App::onFocusChanged, this));

	_isRunning = true;
}

App::~App()
{
	sWinEvent.reset();
	sKeyboard.detach();
	saveConfig();
	if (_window) { delete _window; _window = NULL; }
}

App& App::instance() { return *_self; }

int App::run()
{
	if (!_isRunning) return EXIT_SUCCESS;

	//MSG msg;
	//while (!_window->mustQuit()) {
	//	while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
	//		TranslateMessage(&msg);
	//		DispatchMessageA(&msg);
	//	}
	//	

	//	if (_window->wantQuit())
	//		_window->setState(Window::State_Hide);
	//}
	//_window->cleanup();

	Window::Status status;
	while ((status = _window->poll()) != Window::Status_Close) {
		if (status == Window::Status_Quit)
			_window->setState(Window::State_Hide);
		if (_window->getState() != Window::State_Hide)
			_window->render();
		
		Profile* profile = _activeProfile;
		if (_enabled && profile) {
			DWORD ticks = GetTickCount();
			if (ticks - _lastUpdate >= profile->speed) {
				DWORD mods = sKeyboard.testModifiers();
				for (int i = 0; i < sKeyboard.count(); i++) {
					if (sKeyboard.isPressed(i)) {
						bool spammy = false;
						if (mods != KeyMod_None) {
							if (_activeProfile->keys[i][mods].action == Action_None)
								if (_activeProfile->keys[i][KeyMod_None].action == Action_Spammy)
									spammy = true;
						} else {
							if (_activeProfile->keys[i][KeyMod_None].action == Action_Spammy)
								spammy = true;
						}
						if (spammy) sKeyboard.press(_activeHwnd, i);
					}
				}
				_lastUpdate = ticks;
			}
		}

		Sleep(10); // don't abuse cpu X_x
	}

	return EXIT_SUCCESS;
}

void App::initUIStyle()
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

bool App::loadConfig()
{
	if (!std::filesystem::is_regular_file(CONFIG_FILE)) return true;
	std::ifstream file(CONFIG_FILE);
	if (!file) return false;
	try {
		nlohmann::json json = nlohmann::json::parse(file);
		if (auto enabled = json["enabled"]; enabled.is_boolean())
			_enabled = enabled.get<bool>();
		if (auto profiles = json["profiles"]; profiles.is_array())
			_profiles = json["profiles"].get<std::list<Profile>>();
		if (auto editingProfile = json["editingProfile"]; editingProfile.is_string())
			_editingProfile = findProfile(editingProfile.get_ref<const std::string&>().c_str());
	} catch(nlohmann::json::exception&) {
		return false;
	}
	return true;
}

void App::saveConfig()
{
	std::ofstream file(CONFIG_FILE);
	if (!file) return;

	nlohmann::json json = nlohmann::json::object();
	if (_enabled) json["enabled"] = true;
	if (_editingProfile) json["editingProfile"] = _editingProfile->name;
	if (!_profiles.empty()) json["profiles"] = _profiles;

	file << json.dump(2, ' ');
}

static const wchar_t* getAutoStartCommand()
{
	static wchar_t command[512] = { 0 };
	if (!command[0]) {
		wchar_t buf[MAX_PATH];
		GetModuleFileNameW(NULL, buf, std::size(buf));
		_snwprintf_s(command, std::size(command), L"\"%s\" autolaunch", buf);
	}
	return command;
}

bool App::isAutoStartEnabled()
{
	HKEY hKey = NULL;
	LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey);
	if (status != ERROR_SUCCESS) return false;
	const wchar_t* command = getAutoStartCommand();
	bool result = false;

	DWORD regType = REG_SZ;
	wchar_t regPath[MAX_PATH] = { 0 };
	DWORD regReaded = sizeof(regPath);
	status = RegGetValueW(hKey, NULL, L"Spammy", RRF_RT_REG_SZ, &regType, (PVOID)regPath, &regReaded);
	if (status == ERROR_SUCCESS && wcscmp(regPath, command) == 0)
		result = true;
	RegCloseKey(hKey);
	return result;
}

bool App::enableAutoStart(bool state)
{
	HKEY hKey = NULL;
	LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey);
	if (status != ERROR_SUCCESS) return false;

	if (state) {
		const wchar_t* command = getAutoStartCommand();
		status = RegSetValueExW(hKey, L"Spammy", NULL, REG_SZ, (const BYTE*)command, (lstrlenW(command) + 1) * 2);
	} else {
		status = RegDeleteValueW(hKey, L"Spammy");
	}
	return status == ERROR_SUCCESS;
}

bool App::onKeyPress(UINT vkCode, bool repeat)
{
	unsigned mods = sKeyboard.testModifiers();
	if (_activeProfile) {
		if (_activeProfile->vkPause == MAKE_KEY_BUNDLE(vkCode, mods)) return true;
		if (_activeProfile->disableWin && (vkCode == VK_RWIN || vkCode == VK_LWIN)) return true;
		if (_activeProfile->disableAltF4 && (vkCode == VK_F4 && sKeyboard.testModifiers(KeyMod_Alt))) return true;
		if (_enabled) {
			KeyConfig& cfg = _activeProfile->keys[vkCode][mods];
			switch (cfg.action) {
			case Action_Spammy:
			case Action_Speedy:
				if (!repeat) sKeyboard.press(_activeHwnd, vkCode);
				return true;
			}
		}
	}
	return false;
}

bool App::onKeyRelease(UINT vkCode, bool)
{
	unsigned mods = sKeyboard.testModifiers();
	if (_editPause && _window->isFocused()) {
		if (_editingProfile)
			_editingProfile->vkPause = MAKE_KEY_BUNDLE(vkCode, mods);
		_editPause = false;
	}
	if (_activeProfile && _activeProfile->vkPause == MAKE_KEY_BUNDLE(vkCode, mods)) {
		_enabled = !_enabled;
		return true;
	}

	if (_activeProfile) {
		if (_activeProfile->disableWin && (vkCode == VK_RWIN || vkCode == VK_LWIN)) return true;
		if (_activeProfile->disableAltF4 && (vkCode == VK_F4 && sKeyboard.testModifiers(KeyMod_Alt))) return true;
		if (_enabled) {
			KeyConfig& cfg = _activeProfile->keys[vkCode][mods];
			switch (cfg.action) {
			case Action_Spammy:
			case Action_Speedy:
				return true;
			}
		}
	}
	return false;
}

void App::onTrayClick()
{
	if (_window->getState() == Window::State_Hide) {
		_window->setState(Window::State_Normal);
		_window->focus();
	} else {
		_window->setState(Window::State_Hide);
	}
}

void App::onTrayMenu(TrayIconMenu& menu)
{
	menu.disabled(L"" APP_NAME);
	if (_window->getState() == Window::State_Hide)
		menu.button(L"Show", [this] { _window->setState(Window::State_Normal); });
	else
		menu.button(L"Hide", [this] { _window->setState(Window::State_Hide); });
	menu.toggle(L"Enable", _enabled, [this] { _enabled = !_enabled; });
	menu.button(L"Exit", [this] { _window->close(); });
}

static void sort_string_distionary(std::vector<std::string>& span)
{
	std::sort(span.begin(), span.end(), [](const std::string& a, const std::string& b) {
		return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](const char& a, const char& b) {
			return tolower(a) < tolower(b);
		});
	});
}

static const char* s_keyMods[] = {
	"None",
	"Shift", "Alt", "Shift+Alt", "Ctrl",
	"Shift+Ctrl", "Alt+Ctrl", "Shift+Alt+Ctrl"
};

void App::onWindowUpdate()
{
	ImGui::ShowDemoWindow();

	if (ImGui::BeginMenuBar()) {
		char bufProfilesName[96] = { 0 };
		snprintf(bufProfilesName, std::size(bufProfilesName), "%s##Profiles",
			_editingProfile ? _editingProfile->name.c_str() : "Profiles");
		if (ImGui::BeginMenu(bufProfilesName)) {
			if (ImGui::BeginMenu("Create")) {
				static char bufName[64] = { 0 };
				if (ImGui::IsWindowAppearing()) bufName[0] = '\0';
				ImGui::PushItemWidth(120.f);
				bool enter = ImGui::InputTextWithHint("##test", "Enter name", bufName, std::size(bufName), ImGuiInputTextFlags_EnterReturnsTrue);
				ImGui::PopItemWidth();
				bool validName = strlen(bufName) > 4 && !isProfileExists(bufName);
				if (!validName) ImGui::BeginDisabled();
				if ((ImGui::MenuItem("Save") || enter)) {
					createProfile(bufName);
					ImGui::CloseCurrentPopup();
				}
				if (!validName) ImGui::EndDisabled();
				ImGui::EndMenu();
			}
			ImGui::Separator();
			if (!_profiles.empty()) {
				const char* pendingDelete = NULL;
				for (Profile& item : _profiles) {
					bool activate = false;
					if (ImGui::BeginMenu(item.name.c_str())) {
						if (ImGui::MenuItem("Select"))
							activate = true;
						if (ImGui::BeginMenu("Delete")) {
							ImGui::MenuItem("No");
							if (ImGui::MenuItem("Yes"))
								pendingDelete = item.name.c_str();
							ImGui::EndMenu();
						}
						ImGui::EndMenu();
					}
					if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
						activate = true;
						ImGui::CloseCurrentPopup();
					}
					if (activate) editProfile(item.name.c_str());
				}
				if (pendingDelete) deleteProfile(pendingDelete);
			} else {
				ImGui::BeginDisabled();
				ImGui::MenuItem("Empty");
				ImGui::EndDisabled();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Settings")) {
			bool startWithWindows = _autoStartEnabled;
			if (ImGui::MenuItem("Start with windows", NULL, &startWithWindows)) {
				if (enableAutoStart(startWithWindows))
					_autoStartEnabled = startWithWindows;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("About")) {
			if (ImGui::MenuItem("GitHub repo")) LaunchUrl(L"https://github.com/FrostAtom/spammy");
			if (ImGui::MenuItem("Discord")) LaunchUrl(L"https://discord.gg/NNnBTK5c8e");
			if (ImGui::MenuItem("Author")) LaunchUrl(L"https://t.me/boredatom");
			ImGui::EndMenu();
		}
		ImGui::BeginMenu(_buildTime, false);
		ImGui::EndMenuBar();
	}

	if (ImGui::BeginTable("Header", 3, 0, { 0.f, 60.f })) {
		ImGui::TableSetupColumn("1", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("2", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("3", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (_editingProfile) {
			if (_selectedAction == Action_Spammy) {
				if (ImGui::Button("Mode: Spam##Mode", { 75.f, 0 }))
					_selectedAction = Action_Speedy;
				ImGui::Tip("While the button is pressed, key spamming is simulated");
			} else {
				if (ImGui::Button("Mode: Speed##Mode", { 75.f, 0 }))
					_selectedAction = Action_Spammy;
				ImGui::Tip("When the button is pressed, it is immediately released");
			}
			
			bool spammyMode = _selectedAction == Action_Spammy;
			if (!spammyMode) ImGui::BeginDisabled();
			ImGui::SameLine();
			ImGui::PushItemWidth(140.f);
			ImGui::SliderInt("##Frequency", (int*)&_editingProfile->speed, 10, 1000, "Frequency: %d");
			ImGui::PopItemWidth();
			ImGui::Tip("Delay between spams in miliseconds");
			if (!spammyMode) ImGui::EndDisabled();

			ImGui::Checkbox("Disable Win", &_editingProfile->disableWin);
			ImGui::Tip("Disables Windows button");
			ImGui::SameLine();;
			ImGui::Checkbox("Disable Alt-F4", &_editingProfile->disableAltF4);
			ImGui::Tip("Disables Alt-F4 combination");
		}
		ImGui::TableNextColumn();
		ImGui::TableNextColumn();
		{
			float width = 80.f;
			if (_editingProfile) {
				if (_editPause) {
					if (ImGui::Button("Press any key##pause", { width, 0.f }) || !_window->isFocused())
						_editPause = false;
				} else {
					const char* keyName = (_editingProfile && _editingProfile->vkPause) ? sKeyboard.geyKeyName(GET_KEY_VKCODE(_editingProfile->vkPause)) : "not set";
					if (!keyName[0]) keyName = "Unknown";
					char buf[32];
					snprintf(buf, std::size(buf), "Pause: %s##pause", keyName);
					if (ImGui::Button(buf, { width, 0.f })) {
						_editPause = true;
						_editingProfile->vkPause = 0;
					}
				}
				ImGui::SameLine();
			}
			if (_enabled) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.35f, .8f, .35f, 1.f));
				if (ImGui::Button("Enabled", { width, 0.f }))
					_enabled = false;
			} else {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.8f, .35f, .35f, 1.f));
				if (ImGui::Button("Disabled", { width, 0.f }))
					_enabled = true;
			}
			if (_editingProfile) {
				bool colored = false;
				char bufPreviewEnableFor[64];
				if (_editingProfile->apps.empty()) {
					ImGui::PushStyleColorTriplet(ImGuiCol_FrameBg, ImGui::FlashColor(.95f, .35f, .35f, 2.f, .2f, .8f));
					colored = true;
					strncpy(bufPreviewEnableFor, "Select apps", std::size(bufPreviewEnableFor));
				} else {
					snprintf(bufPreviewEnableFor, std::size(bufPreviewEnableFor), "%d apps selected", _editingProfile->apps.size());
				}
				ImGui::PushItemWidth(width + width + ImGui::GetStyle().ItemSpacing.x);
				if (ImGui::BeginCombo("##enablefor", bufPreviewEnableFor, ImGuiComboFlags_NoArrowButton)) {
					static std::vector<std::string> appList;
					if (ImGui::IsWindowAppearing()) {
						appList.clear();
						EnumWindows([this](HWND hwnd) -> BOOL {
							std::filesystem::path path = GetProcessPath(hwnd);
							if (!path.has_filename() || path == _appFilePath) return TRUE;
							std::string fileName = (const char*)path.filename().u8string().c_str();
							if (std::find(_editingProfile->apps.begin(), _editingProfile->apps.end(), fileName)
								!= _editingProfile->apps.end()) return TRUE; // already added programs
							if (std::find(appList.begin(), appList.end(), fileName) != appList.end()) return TRUE; // dublicates
							appList.emplace_back(std::move(fileName));
							return TRUE;
						});
						sort_string_distionary(appList);
					}

					const char* unbindApp = NULL;
					for (std::string& app : _editingProfile->apps) {
						if (ImGui::Selectable(app.c_str(), true))
							unbindApp = app.c_str();
					}
					if (unbindApp) unbindProfile(_editingProfile->name.c_str(), unbindApp);

					for (const std::string& app : appList) {
						Profile* appProfile = findProfileByApp(app.c_str());
						bool enabledNotForThis = appProfile && _editingProfile != appProfile;
						if (enabledNotForThis) ImGui::BeginDisabled();
						if (ImGui::Selectable(app.c_str()))
							bindProfile(_editingProfile->name.c_str(), app.c_str());
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
				KeyConfig* cfg = _editingProfile ? &_editingProfile->keys[key.vkCode][mods] : NULL;

				bool pressed = sKeyboard.isPressed(key.vkCode);
				if (pressed) {
					color = ImColor(ImGui::GetColorU32(ImGuiCol_ButtonActive));
					hasColor = true;
				} else if (cfg) {
					if (cfg->action != Action_None) {
						color = ButtonMode[cfg->action];
						hasColor = true;
					} else if (mods != KeyMod_None) {
						KeyConfig& parentCfg = _editingProfile->keys[key.vkCode][KeyMod_None];
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

				snprintf(buf, std::size(buf), "##popup%02x", i);
				if (ImGui::BeginPopupContextItem(buf)) {
					static DWORD mods;
					if (ImGui::IsWindowAppearing()) mods = sKeyboard.testModifiers();
					ImGui::MenuItem(s_keyMods[mods], NULL, false, false);

					KeyConfig& cfg = _editingProfile->keys[key.vkCode][mods];
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

						if (ImGui::MenuItem("Disable"))
							cfg.action = Action_Disabled;
					} else {
						if (ImGui::MenuItem("Enable##Disable"))
							cfg.action = Action_None;
					}
					ImGui::MenuItem("Close");
					ImGui::EndPopup();
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

		ImGui::ColorButton("Disabled", ButtonMode[Action_Disabled], ImGuiColorEditFlags_NoTooltip);
		ImGui::SameLine();
		ImGui::TextUnformatted("Disabled");

		ImGui::SameLine();
		ImGui::ColorButton("Spammy", ButtonMode[Action_Spammy], ImGuiColorEditFlags_NoTooltip);
		ImGui::SameLine();
		ImGui::TextUnformatted("Spammy");

		ImGui::SameLine();
		ImGui::ColorButton("Speedy", ButtonMode[Action_Speedy], ImGuiColorEditFlags_NoTooltip);
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

Profile* App::findProfile(const char* name)
{
	auto it = std::find_if(_profiles.begin(), _profiles.end(), [=](const Profile& item) {
		return item.name == name;
	});
	return it != _profiles.end() ? &(*it) : NULL;
}

bool App::isProfileExists(const char* name)
{
	return (bool)findProfile(name);
}

void App::createProfile(const char* name)
{
	Profile& profile = _profiles.emplace_back();
	profile.name = name;
	_editingProfile = &profile;
}

void App::editProfile(const char* name)
{
	_editingProfile = findProfile(name);
}

void App::deleteProfile(const char* name)
{
	auto it = std::find_if(_profiles.begin(), _profiles.end(), [name](const Profile& item) {
		return item.name == name;
	});
	if (&(*it) == _editingProfile) _editingProfile = NULL;
	if (&(*it) == _activeProfile) _activeProfile = NULL;
	_profiles.erase(it);
}

Profile* App::findProfileByApp(const char* app)
{
	auto it = std::find_if(_profiles.begin(), _profiles.end(), [app](const Profile& item) {
		return std::find(item.apps.begin(), item.apps.end(), app) != item.apps.end();
	});
	return it != _profiles.end() ? &(*it) : NULL;
}

bool App::isProfileBinded(const char* name, const char* app)
{
	if (Profile* profile = findProfile(name))
		return std::find(profile->apps.begin(), profile->apps.end(), app) != profile->apps.end();
	return false;
}

void App::bindProfile(const char* name, const char* app)
{
	if (Profile* profile = findProfile(name); profile && (std::find(profile->apps.begin(), profile->apps.end(), app) == profile->apps.end())) {
		profile->apps.emplace_back(app);
		sort_string_distionary(profile->apps);
	}
}

void App::unbindProfile(const char* name, const char* app)
{
	if (Profile* profile = findProfile(name))
		profile->apps.erase(std::remove(profile->apps.begin(), profile->apps.end(), app), profile->apps.end());
}

void App::onFocusChanged()
{
	_activeProfile = NULL;
	_activeHwnd = GetForegroundWindow();

	std::filesystem::path path = GetProcessPath(_activeHwnd);
	if (path.has_filename())
		_activeProfile = findProfileByApp((const char*)path.filename().u8string().c_str());
}

auto main(int argc, wchar_t* argv[]) -> int
{
	std::unique_ptr<App> app(new App(argc, argv));
	return app->run();
}