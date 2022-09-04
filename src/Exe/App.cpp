#include "App.h"

App* App::_self = NULL;

App::App(int argc, wchar_t* argv[])
	: _isRunning(false), _enabled(false), _mainWindow(NULL), _autoStartEnabled(false),
		_lastUpdate(0), _activeHwnd(NULL), _activeProfile(NULL), _editingProfile(NULL)
{
	_self = this;

	if (!loadConfig()) {
		int action = MessageBoxW(NULL, L"Can't load config, reset to defaults?", L"" APP_NAME, MB_ICONQUESTION | MB_OKCANCEL);
		if (action != IDOK) return;
	}
	_autoStartEnabled = isAutoStartEnabled();

	_mainWindow = new MainWindow(L"" APP_NAME);
	if (!_mainWindow->initialize()) return;

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
	if (_mainWindow) { delete _mainWindow; _mainWindow = NULL; }
}

App& App::instance() { return *_self; }

int App::run()
{
	if (!_isRunning) return EXIT_SUCCESS;

	while (!_mainWindow->mustQuit()) {
		MSG msg;
		while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		if (_mainWindow->wantQuit()) _mainWindow->hide();
		if (_mainWindow->isNormalized())
			_mainWindow->update();

		Profile* profile = _activeProfile;
		if (_enabled && profile) {
			DWORD ticks = GetTickCount();
			if (ticks - _lastUpdate >= profile->speed) {
				DWORD mods = sKeyboard.testModifiers();
				for (size_t i = 0; i < sKeyboard.count(); i++) {
					if (sKeyboard.isPressed(i)) {
						bool spammy = false;
						if (mods != KeyMod_None) {
							if (_activeProfile->keys[i][mods].action == Action_None)
								if (_activeProfile->keys[i][KeyMod_None].action == Action_Spammy)
									spammy = true;
						}
						else {
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
	_mainWindow->cleanup();

	return EXIT_SUCCESS;
}

void App::enable(bool state)
{
	_enabled = state;
}

bool App::isEnabled()
{
	return _enabled;
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
	if (_mainWindow && _mainWindow->handleKeyPress(vkCode, repeat)) return true;
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

bool App::onKeyRelease(UINT vkCode, bool repeat)
{
	if (_mainWindow && _mainWindow->handleKeyRelease(vkCode, repeat)) return true;
	unsigned mods = sKeyboard.testModifiers();
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

const std::list<Profile>& App::getProfiles()
{
	return _profiles;
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

void App::editingProfile(const char* name)
{
	_editingProfile = findProfile(name);
}

Profile* App::editingProfile()
{
	return _editingProfile;
}

void App::deleteProfile(const char* name)
{
	auto it = std::find_if(_profiles.begin(), _profiles.end(), [name](const Profile& item) {
		return item.name == name;
	});
	if (&(*it) == _activeProfile) _activeProfile = NULL;
	if (&(*it) == _editingProfile) _editingProfile = NULL;
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
		LexicographicalSort(profile->apps);
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