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

static const DWORD ImGuiWindowDisableScrollMask = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
static const DWORD ImGuiWindowMenuBarMask = ImGuiWindowFlags_MenuBar;

Window::Window(const wchar_t* className, const wchar_t* wndName)
	: _icon(NULL), _atom(NULL), _hwnd(NULL), _imCtx(NULL), _wantQuit(false), _mustQuit(false),
		_imWndFlags(ImGuiWindowDisableScrollMask), _sizable(false), _movable(false), _moving(false), _renderBackend(NULL), _trayIcon(NULL),
		_position(CW_USEDEFAULT, CW_USEDEFAULT), _size(512, 512), _sizeMin(512, 512), _sizeMax(512, 512)
{
	if (!wndName) wndName = className;
	wcsncpy_s(_className, className, std::size(_className));
	setName(wndName);
}

Window::~Window() { cleanup(); }

Window::ErrorCode Window::initialize()
{
	if (_hwnd) return ErrorCode_InvalidCall;

	_imCtx = ImGui::CreateContext();
	if (!_imCtx) return ErrorCode_ImGuiError;

	if (!createWindow()) {
		ImGui::DestroyContext(_imCtx);
		_imCtx = NULL;
		return ErrorCode_WinError;
	}
	UpdateWindow(_hwnd);
	ShowWindow(_hwnd, SW_SHOWDEFAULT);

	if (_renderBackend && !_renderBackend->create(_hwnd)) {
		cleanupWindow();
		ImGui::DestroyContext(_imCtx);
		_imCtx = NULL;
		return ErrorCode_RendererError;;
	}

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigWindowsResizeFromEdges = false;
	io.IniFilename = NULL;

	ImFontConfig fontCfg;
	fontCfg.PixelSnapH = true;
	io.Fonts->AddFontFromMemoryCompressedTTF(ComicSans_compressed_data, ComicSans_compressed_size, 14.f, &fontCfg, io.Fonts->GetGlyphRangesCyrillic());
	io.Fonts->Build();

	if (_trayIcon) _trayIcon->create(_hwnd, _icon);

	return ErrorCode_OK;
}

void Window::setRenderBackend(WindowRenderer* renderer)
{
	if (_renderBackend) delete _renderBackend;
	_renderBackend = renderer;
	if (_hwnd && renderer) renderer->create(_hwnd);
}

void Window::setTrayIcon(TrayIcon* icon)
{
	if (_trayIcon) { delete _trayIcon; }
	_trayIcon = icon;
	if (_hwnd) _trayIcon->create(_hwnd, _icon);
}

void Window::cleanup()
{
	if (_trayIcon) { delete _trayIcon; _trayIcon = NULL; }
	if (_renderBackend) { delete _renderBackend; _renderBackend = NULL; };
	cleanupWindow();
	if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
}

void Window::quit()
{
	_wantQuit = true;
}

void Window::close()
{
	_mustQuit = true;
}

void Window::setIcon(HICON icon)
{
	_icon = icon;
	if (_hwnd) {
		SendMessageW(_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)_icon);
		SendMessageW(_hwnd, WM_SETICON, ICON_BIG, (LPARAM)_icon);
		if (_trayIcon) _trayIcon->updateIcon(_icon);
	}
}

void Window::setIcon(unsigned id)
{
	HICON icon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(id));
	setIcon(icon);
}

HICON Window::getIcon()
{
	return _icon;
}

bool Window::mustQuit()
{
	return _mustQuit;
}

bool Window::wantQuit()
{
	if (_wantQuit) {
		_wantQuit = false;
		return true;
	}
	return false;
}

bool Window::isFocused()
{
	return _hwnd && GetForegroundWindow() == _hwnd;
}

void Window::focus()
{
	if (_hwnd) SetForegroundWindow(_hwnd);
}

static int GetShowCmd(HWND hwnd)
{
	WINDOWPLACEMENT wPos = { sizeof(wPos) };
	GetWindowPlacement(hwnd, &wPos);
	return wPos.showCmd;
}

void Window::minimize()
{
	if (_hwnd) ShowWindow(_hwnd, SW_MINIMIZE);
}

bool Window::isMinimized()
{
	if (!_hwnd) return false;
	int cmd = GetShowCmd(_hwnd);
	return cmd == SW_MINIMIZE || cmd == SW_SHOWMINIMIZED;
}

void Window::maximize()
{
	if (_hwnd) ShowWindow(_hwnd, SW_SHOWMAXIMIZED);
}

bool Window::isMaximized()
{
	if (!_hwnd) return false;
	int cmd = GetShowCmd(_hwnd);
	return cmd == SW_MAXIMIZE;
}

void Window::normalize()
{
	if (_hwnd) ShowWindow(_hwnd, SW_NORMAL);
}

bool Window::isNormalized()
{
	if (!_hwnd) return false;
	int cmd = GetShowCmd(_hwnd);
	return cmd == SW_NORMAL;
}

void Window::show()
{
	if (_hwnd) ShowWindow(_hwnd, SW_SHOWNORMAL);
}

void Window::hide()
{
	if (_hwnd) ShowWindow(_hwnd, SW_HIDE);
}

bool Window::isShown()
{
	if (!_hwnd) return false;
	return (bool)(GetWindowStyle(_hwnd) & WS_VISIBLE);
}

void Window::setName(const wchar_t* name)
{
	wcsncpy_s(_wndName, name, std::size(_wndName));
	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
	strncpy_s(_u8wndName, conv.to_bytes(name).c_str(), std::size(_u8wndName));
	if (_hwnd) SetWindowTextW(_hwnd, name);
}

const wchar_t* Window::getName()
{
	return _wndName;
}

void Window::enableSizing(bool state)
{
	_sizable = state;
	if (_hwnd) {
		LONG style = GetWindowLongW(_hwnd, GWL_STYLE);
		if (state)
			style |= WS_THICKFRAME;
		else
			style &= ~WS_THICKFRAME;
		SetWindowLongW(_hwnd, GWL_STYLE, style);
	}
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

void Window::setSizeMinMax(const Vec2D<size_t>& min, const Vec2D<size_t>& max)
{
	_sizeMin = min;
	_sizeMax = max;
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
	if (_hwnd) MoveWindow(_hwnd, _position.x, _position.y, _size.x, _size.y, FALSE);
}

void Window::setPosition(const Vec2D<size_t>& position)
{
	_position = position;
	if (_hwnd) MoveWindow(_hwnd, _position.x, _position.y, _size.x, _size.y, FALSE);
}

Window::Vec2D<size_t> Window::getPosition()
{
	return _position;
}

void Window::resetPosition()
{
	Vec2D<size_t> pos = getScreenSize();
	pos.x -= _size.x; pos.y -= _size.y;
	pos.x /= 2; pos.y /= 2;
	setPosition(pos);
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

void Window::update()
{
	if (isReady()) {
		if (Window::beginFrame())
			draw();
		Window::endFrame();
	}
}

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

bool Window::isReady()
{
	return _hwnd && _renderBackend && _renderBackend->isCreated();
}

bool Window::beginFrame()
{
	_renderBackend->beginFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	bool isShown = true;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	DWORD flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus;
	bool result = ImGui::Begin(_u8wndName, &isShown, flags | _imWndFlags);
	if (!isShown) _wantQuit = true;

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
		startMove();
	else
		stopMove();

	if (!isShown) PostMessageW(_hwnd, WM_QUIT, NULL, NULL);
	return result;
}

void Window::endFrame()
{
	ImGui::End();
	ImGui::EndFrame();
	_renderBackend->render();
}

bool Window::createWindow()
{
	WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandleW(NULL), NULL, NULL, NULL, NULL, _className, NULL };
	_atom = RegisterClassExW(&wc);
	if (!_atom) return false;

	DWORD dwStyle = WS_VISIBLE | WS_POPUP;
	if (_sizable) dwStyle |= WS_THICKFRAME;
	_hwnd = CreateWindowExW(0, wc.lpszClassName, _wndName, dwStyle, _position.x, _position.y, _size.x, _size.y, NULL, NULL, wc.hInstance, this);
	if (!_hwnd) {
		cleanupWindow();
		return false;
	}
	if (_icon) {
		SendMessageW(_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)_icon);
		SendMessageW(_hwnd, WM_SETICON, ICON_BIG, (LPARAM)_icon);
	}

	ImGui_ImplWin32_Init(_hwnd);
	return true;
}

void Window::cleanupWindow()
{
	if (ImGui::GetCurrentContext() && ImGui::GetIO().BackendPlatformUserData) ImGui_ImplWin32_Shutdown();
	if (_hwnd) { DestroyWindow(_hwnd); _hwnd = NULL; }
	if (_atom) { UnregisterClassW((LPCWSTR)(_atom & 0xFFFF), NULL); _atom = NULL; }
}

void Window::startMove()
{
	if (!_movable || _moving) return;
	POINT point;
	GetCursorPos(&point);
	RECT rect;
	GetWindowRect(_hwnd, &rect);
	_movePos = { size_t(point.x - rect.left), size_t(point.y - rect.top) };
	_moving = true;
}

void Window::stopMove()
{
	if (!_moving) return;
	_moving = false;
}

void Window::updateMove()
{
	if (!_moving) return;
	if (!_movable) return stopMove();
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
bool Window::handleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
	*result = 0;
	if (*result = ImGui_ImplWin32_WndProcHandler(_hwnd, msg, wParam, lParam)) return true;
	if (_trayIcon && _trayIcon->handleMessage(msg, wParam, lParam)) return true;

	switch(msg) {
	case WM_CLOSE:
		_wantQuit = true;
		return true;
	case WM_ENDSESSION:
		_mustQuit = true;
		return true;
	case WM_SIZING:
		update();
		*result = TRUE;
		return true;
	case WM_MOVE:
		_position = { LOWORD(lParam), HIWORD(lParam) };
		return true;
	case WM_SIZE:
		_size = { LOWORD(lParam), HIWORD(lParam) };
		if (wParam != SIZE_MINIMIZED && _renderBackend)
			_renderBackend->setSize(_size.x, _size.y);
		return true;
	case WM_MOUSEMOVE: {
		if (wParam & MK_LBUTTON)
			updateMove();
		return true;
	}
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			return true;
		break;
	case WM_GETMINMAXINFO: {
		if (_sizable) {
			LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
			lpMMI->ptMinTrackSize = { (long)_sizeMin.x, (long)_sizeMin.y };
			lpMMI->ptMaxTrackSize = { (long)_sizeMax.x, (long)_sizeMax.y };
			return true;
		}
		break;
	}
	case WM_NCACTIVATE:
		*result = DefWindowProcW(_hwnd, msg, wParam, -1);
		return true;
	case WM_NCCALCSIZE: {
		if (wParam) {
			if (!isMaximized()) {
				RECT borderThickness;
				SetRectEmpty(&borderThickness);
				AdjustWindowRectEx(&borderThickness, GetWindowLongPtr(_hwnd, GWL_STYLE) & ~WS_CAPTION, FALSE, NULL);
				borderThickness.left *= -1;
				borderThickness.top *= -1;
				NCCALCSIZE_PARAMS* sz = (NCCALCSIZE_PARAMS*)lParam;
				sz->rgrc[0].top += borderThickness.top;
				sz->rgrc[0].left += borderThickness.left;
				sz->rgrc[0].right -= borderThickness.right;
				sz->rgrc[0].bottom -= borderThickness.bottom;
				return true;
			}
		}
		break;
	}
	}
	return false;
}

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

	LRESULT result;
	if (self && self->handleWndProc(msg, wParam, lParam, &result)) return result;
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}