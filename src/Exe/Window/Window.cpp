#include "Window.h"
#include "Detail/ImFont_ComicSans.h"


const char* Window::ErrorCodeNames[] = {
	"OK",
	"Invalid call",
	"Windows error",
	"Renderer error",
	"ImGui errror",
};

const char* Window::formatError(ErrorCode code)
{
	return ErrorCodeNames[code];
}

Window::Window(const wchar_t* className, const wchar_t* wndName)
	: _moving(false), _sizable(false), _sizeMin(1u, 1u), _sizeMax(0xffffu, 0xffffu), _size(32u, 32u),
		_quitRequested(false), _hicon(NULL), _atom(NULL), _hwnd(NULL), _renderBackend(NULL), _trayIcon(NULL),
		_imWndFlags(0)
{
	if (!wndName) wndName = className;
	wcsncpy_s(_className, className, std::size(_className));
	setName(wndName);
}

Window::~Window() { cleanup(); }

Window::ErrorCode Window::create(State state)
{
	if (_hwnd) return ErrorCode_InvalidCall;

	ImGuiContext* ctx = ImGui::CreateContext();
	if (!ctx) return ErrorCode_ImGuiError;

	if (!createWindow(state)) {
		ImGui::DestroyContext(ctx);
		return ErrorCode_WinError;
	}

	if (_renderBackend && !_renderBackend->create(_hwnd)) {
		cleanupWindow();
		return ErrorCode_RendererError;;
	}

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigWindowsResizeFromEdges = false;
	io.IniFilename = NULL;

	ImFontConfig fontCfg;
	fontCfg.PixelSnapH = true;
	io.Fonts->AddFontFromMemoryCompressedTTF(ComicSans_compressed_data, ComicSans_compressed_size, 14.f, &fontCfg, io.Fonts->GetGlyphRangesCyrillic());
	io.Fonts->Build();

	if (_trayIcon) _trayIcon->create(_hwnd, _hicon);

	return ErrorCode_OK;
}

void Window::setRenderBackend(WindowRenderer* renderer)
{
	_pendingTasks.push_back([this, renderer](Status& status) {
		if (!_hwnd) return;
		if (_renderBackend) { delete _renderBackend; }
		if (_hwnd && renderer) {
			if (renderer->create(_hwnd)) {
				_renderBackend = renderer;
				return;
			} else {
				delete renderer;
				status = Status_Fail;
				return;
			}
		}
		_renderBackend = renderer;
	});
}

void Window::setTrayIcon(TrayIcon* icon)
{
	if (_trayIcon) { delete _trayIcon; }
	_trayIcon = icon;
	if (_hwnd) _trayIcon->create(_hwnd, _hicon);
}

TrayIcon* Window::getTrayIcon()
{
	return _trayIcon;
}

void Window::cleanup()
{
	if (_trayIcon) { delete _trayIcon; _trayIcon = NULL; }
	if (_renderBackend) { delete _renderBackend; _renderBackend = NULL; };
	cleanupWindow();
	if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
}

void Window::setOnUpdate(RenderCallback_t&& func)
{
	_renderer = std::forward<RenderCallback_t>(func);
}

void Window::quit()
{
	_pendingTasks.push_back([this](Status& status) { status = Status_Quit; });
}

void Window::close()
{
	_pendingTasks.push_back([this](Status& status) {
		cleanup();
		status = Status_Close;
	});
}

void Window::setIcon(HICON icon)
{
	_hicon = icon;
	if (!_hwnd) return;
	SendMessageW(_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)_hicon);
	SendMessageW(_hwnd, WM_SETICON, ICON_BIG, (LPARAM)_hicon);
	if(_trayIcon) _trayIcon->updateIcon(_hicon);
}

void Window::setIcon(unsigned id)
{
	HICON icon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(id));
	setIcon(icon);
}

HICON Window::getIcon()
{
	return _hicon;
}

Window::Status Window::poll()
{
	Status status = Status_Running;
	if (!_pendingTasks.empty()) {
		for (auto& task : _pendingTasks)
			task(status);
		_pendingTasks.clear();
	}

	MSG msg;
	while (PeekMessageW(&msg, NULL, 0U, 0U, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
		if (msg.message == WM_QUIT)
			status = Status_Quit;
	}
	if (!_hwnd) return Status_Close;
	return status;
}

bool Window::isFocused()
{
	return GetForegroundWindow() == _hwnd;
}

void Window::focus()
{
	SetForegroundWindow(_hwnd);
}

Window::State Window::getState()
{
	LONG style = GetWindowLongW(_hwnd, GWL_STYLE);
	if ((style & WS_VISIBLE) == 0) return State_Hide;
	WINDOWPLACEMENT placement;
	memset(&placement, NULL, sizeof(placement));
	GetWindowPlacement(_hwnd, &placement);
	if (placement.showCmd == SW_SHOWMINIMIZED) return State_Minimize;
	if (placement.showCmd == SW_SHOWMAXIMIZED) return State_Maximize;
	return State_Normal;
}

void Window::setState(State state)
{
	int cmdShow = SW_RESTORE;
	switch (state) {
	case State_Hide:
		cmdShow = SW_HIDE;
		break;
	case State_Minimize:
		cmdShow = SW_MINIMIZE;
		break;
	case State_Maximize:
		cmdShow = SW_SHOWMAXIMIZED;
		break;
	case State_Normal:
		//cmdShow = SW_RESTORE;
		break;
	}
	ShowWindow(_hwnd, cmdShow);
}

void Window::setName(const wchar_t* name)
{
	wcsncpy_s(_wndName, name, std::size(_wndName));
	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
	strncpy_s(_u8wndName, conv.to_bytes(name).c_str(), std::size(_u8wndName));
	SetWindowTextW(_hwnd, name);
}

const wchar_t* Window::getName()
{
	return _wndName;
}

void Window::enableSizing(bool state)
{
	LONG style = GetWindowLongW(_hwnd, GWL_STYLE);
	if (state)
		style |= WS_THICKFRAME;
	else
		style &= ~WS_THICKFRAME;
	SetWindowLongW(_hwnd, GWL_STYLE, style);
	_sizable = state;
}

bool Window::isEnabledSizing()
{
	return _sizable;
}

Window::Vec2D<size_t> Window::getSizeMin()
{
	return _sizeMin;
}

void Window::setSizeMin(const Vec2D<size_t>& min)
{
	_sizeMin = min;
}

Window::Vec2D<size_t> Window::getSizeMax()
{
	return _sizeMax;
}

void Window::setSizeMax(const Vec2D<size_t>& min)
{
	_sizeMax = min;
}

Window::Vec2D<size_t> Window::getSize()
{
	return _size;
}

void Window::setSize(const Vec2D<size_t>& size)
{
	_size = size;
	_pendingTasks.push_back([this, size](Status&) {
		if (!_hwnd) return;
		if (size.x != _size.x || size.y != _size.y)
			MoveWindow(_hwnd, _position.x, _position.y, size.x, size.y, FALSE);
	});
}

void Window::setPosition(const Vec2D<size_t>& position)
{
	_pendingTasks.push_back([this, position](Status&) {
		if (!_hwnd) return;
		MoveWindow(_hwnd, position.x, position.y, _size.x, _size.y, FALSE);
	});
}

Window::Vec2D<size_t> Window::getPosition()
{
	return _position;
}

void Window::setPositionCenter()
{
	Vec2D<size_t> screenSize = getScreenSize();
	setPosition({ screenSize.x / 2 - _size.x, screenSize.y / 2 - _size.y });
}

Window::Vec2D<size_t> Window::getScreenSize()
{
	RECT rect;
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &rect, 0);
	return { size_t(rect.right - rect.left), size_t(rect.bottom - rect.top) };
}

void Window::enableMoving(bool state)
{
	_movable = state;
}

bool Window::isEnabledMoving()
{
	return _movable;
}

bool Window::isMoving()
{
	return _moving;
}

bool Window::focusTwin()
{
	HWND hwnd = FindWindowW(_className, _wndName);
	if (!hwnd) return false;
	PostMessageW(hwnd, WM_USER_FOCUS, NULL, NULL);
	return true;
}

void Window::render()
{
	if (Window::beginFrame() && _renderer)
		_renderer();
	Window::endFrame();
}

static const DWORD ImGuiWindowDisableScrollMask = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
void Window::enableScroll(bool state)
{
	if (state)
		_imWndFlags &= ~ImGuiWindowDisableScrollMask;
	else
		_imWndFlags |= ImGuiWindowDisableScrollMask;
}

bool Window::isEnabledScroll()
{
	return !((_imWndFlags & ImGuiWindowDisableScrollMask) == ImGuiWindowDisableScrollMask);
}

static const DWORD ImGuiWindowMenuBarMask = ImGuiWindowFlags_MenuBar;
void Window::enableMenuBar(bool state)
{
	if (state)
		_imWndFlags |= ImGuiWindowMenuBarMask;
	else
		_imWndFlags &= ~ImGuiWindowMenuBarMask;
}

bool Window::isEnabledMenuBar()
{
	return (_imWndFlags & ImGuiWindowMenuBarMask) == ImGuiWindowMenuBarMask;
}

HWND Window::native()
{
	return _hwnd;
}

bool Window::beginFrame()
{
	if (!_renderBackend) return false;
	_renderBackend->beginFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	bool isShown = true;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	DWORD flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus;
	bool result = ImGui::Begin(_u8wndName, &isShown, flags | _imWndFlags);

	//// sync imgui & window focus
	//bool imFocus = ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);
	//if (isFocused) {
	//	if (!imFocus)
	//		ImGui::SetWindowFocus();
	//} else {
	//	if (imFocus)
	//		ImGui::SetWindowFocus(NULL);
	//}

	ImGuiContext* ctx = ImGui::GetCurrentContext();
	if (ctx->CurrentWindow && ctx->MovingWindow == ctx->CurrentWindow)
		beginDrag();
	else
		endDrag();

	if (!isShown) PostMessageW(_hwnd, WM_QUIT, NULL, NULL);
	return result;
}

void Window::endFrame()
{
	if (!_renderBackend) return;
	ImGui::End();
	ImGui::EndFrame();
	_renderBackend->render();
}

bool Window::createWindow(State state)
{
	WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandleW(NULL), NULL, NULL, NULL, NULL, _className, NULL };
	_atom = RegisterClassExW(&wc);
	if (!_atom) return false;

	DWORD dwStyle = WS_VISIBLE | WS_POPUP;
	if (_sizable) dwStyle |= WS_THICKFRAME;
	_hwnd = CreateWindowExW(0, wc.lpszClassName, _wndName, dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, _size.x, _size.y, NULL, NULL, wc.hInstance, this);
	if (!_hwnd) {
		cleanupWindow();
		return false;
	}
	if (_hicon) {
		SendMessageW(_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)_hicon);
		SendMessageW(_hwnd, WM_SETICON, ICON_BIG, (LPARAM)_hicon);
	}
	setState(state);

	ImGui_ImplWin32_Init(_hwnd);
	return true;
}

void Window::cleanupWindow()
{
	if (ImGui::GetCurrentContext() && ImGui::GetIO().BackendPlatformUserData) ImGui_ImplWin32_Shutdown();
	if (_hwnd) { DestroyWindow(_hwnd); _hwnd = NULL; }
	if (_atom) { UnregisterClassW((LPCWSTR)(_atom & 0xFFFF), NULL); _atom = NULL; }
}

void Window::beginDrag()
{
	if (!_movable || _moving) return;
	POINT point;
	GetCursorPos(&point);
	RECT rect;
	GetWindowRect(_hwnd, &rect);
	_movePos = { size_t(point.x - rect.left), size_t(point.y - rect.top) };
	_moving = true;
}

void Window::endDrag()
{
	if (!_moving) return;
	_moving = false;
}

void Window::updateDrag()
{
	if (!_moving) return;
	if (!_movable) return endDrag();
	POINT cursorPos;
	GetCursorPos(&cursorPos);
	RECT rect;
	GetWindowRect(_hwnd, &rect);
	LONG h = rect.bottom - rect.top;
	LONG w = rect.right - rect.left;
	int x = cursorPos.x - _movePos.x;
	int y = cursorPos.y - _movePos.y;
	MoveWindow(_hwnd, x, y, w, h, TRUE);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Window* self = NULL;
	if (msg == WM_CREATE) {
		CREATESTRUCTA* createStruct = (CREATESTRUCTA*)lParam;
		self = (Window*)createStruct->lpCreateParams;
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
		return 0;
	} else {
		self = (Window*)GetWindowLongW(hwnd, GWLP_USERDATA);
	}

	LRESULT res = ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
	if (res) return res;

	if (self && self->_trayIcon && self->_trayIcon->handleMessage(msg, wParam, lParam))
		return 0;

	switch (msg) {
	case WM_ENDSESSION:
		self->close();
		break;
	case WM_USER_FOCUS:
		if (self->getState() == Window::State_Hide)
			self->setState(Window::State_Normal);
		self->focus();
		break;
	case WM_SIZING:
		self->render();
		break;
	case WM_MOVE:
		self->_position.x = LOWORD(lParam);
		self->_position.y = HIWORD(lParam);
		break;
	case WM_SIZE:
		self->_size.x = LOWORD(lParam);
		self->_size.y = HIWORD(lParam);
		if (wParam != SIZE_MINIMIZED && self->_renderBackend)
			self->_renderBackend->setSize(LOWORD(lParam), HIWORD(lParam));
		return 0;
	case WM_MOUSEMOVE: {
		if (wParam & MK_LBUTTON)
			self->updateDrag();
		break;
	}
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_GETMINMAXINFO: {
		if (self && self->_sizable) {
			LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
			lpMMI->ptMinTrackSize = { (long)self->_sizeMin.x, (long)self->_sizeMin.y };
			lpMMI->ptMaxTrackSize = { (long)self->_sizeMax.x, (long)self->_sizeMax.y };
			return 0;
		}
		break;
	}
	case WM_NCACTIVATE: {
		lParam = -1;
		break;
	}
	case WM_NCCALCSIZE:
		if (wParam) {
			WINDOWPLACEMENT wPos;
			wPos.length = sizeof(wPos);
			GetWindowPlacement(hwnd, &wPos);
			if (wPos.showCmd != SW_SHOWMAXIMIZED) {
				RECT borderThickness;
				SetRectEmpty(&borderThickness);
				AdjustWindowRectEx(&borderThickness, GetWindowLongPtr(hwnd, GWL_STYLE) & ~WS_CAPTION, FALSE, NULL);
				borderThickness.left *= -1;
				borderThickness.top *= -1;
				NCCALCSIZE_PARAMS* sz = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
				sz->rgrc[0].top += 1;
				sz->rgrc[0].left += borderThickness.left;
				sz->rgrc[0].right -= borderThickness.right;
				sz->rgrc[0].bottom -= borderThickness.bottom;
				return 0;
			}
		}
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}