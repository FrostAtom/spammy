#pragma once
#include <Windows.h>
#include <shellapi.h>
#include <codecvt>
#include <type_traits>
#include <functional>
#include <string>
#include <unordered_map>
#include <map>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_win32.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui_internal.h>
#include "Renderer/Renderer.h"
#include "Renderer/D3d9.h"
#include "Renderer/D3d11.h"
#include "TrayIcon.h"
#ifndef WM_USER_FOCUS
#define WM_USER_FOCUS (WM_APP + 0x20)
#endif

class Window {
public:
	enum ErrorCode {
		ErrorCode_OK = 0,
		ErrorCode_InvalidCall,
		ErrorCode_WinError,
		ErrorCode_RendererError,
		ErrorCode_ImGuiError,
		ErrorCode_COUNT,
	};

	template <typename T>
	struct Vec2D {
		Vec2D() : x(0), y(0) {}
		Vec2D(T x, T y) : x(x), y(y) {}
		T x, y;
	};

	enum Status {
		Status_Running, // Normal
		Status_Quit, // Quit requested
		Status_Close, // Window closed, loop must be breaked
		Status_Fail,
	};

	enum State {
		State_Normal,
		State_Hide,
		State_Minimize,
		State_Maximize,
	};

	using RenderCallback_t = std::function<void()>;
	using PendingTask_t = std::function<void(Status& status)>;
private:
	wchar_t _wndName[64], _className[64];
	char _u8wndName[64];
	DWORD _imWndFlags;
	bool _sizable;
	bool _movable, _moving;
	Vec2D<size_t> _movePos;
	Vec2D<size_t> _sizeMin, _sizeMax, _size;
	Vec2D<size_t> _position;
	HICON _hicon;
	ATOM _atom;
	HWND _hwnd;
	bool _quitRequested;
	std::vector<PendingTask_t> _pendingTasks;

	WindowRenderer* _renderBackend;
	TrayIcon* _trayIcon;
	RenderCallback_t _renderer;
	static const char* ErrorCodeNames[];
public:
	Window(const wchar_t* className, const wchar_t* wndName = NULL);
	~Window();

	void setRenderBackend(WindowRenderer* renderer);

	void setTrayIcon(TrayIcon* icon);
	TrayIcon* getTrayIcon();

	// initializing window
	ErrorCode create(State state = State_Normal);

	// inside this function u must draw your UI
	void setOnUpdate(RenderCallback_t&& func);

	// sends exit signal to poll(), can be ignored
	void quit();

	// cleanups window interfaces
	void cleanup();

	// destroys window
	void close();

	// fires update cycle
	// returns false once when quit() called or WM_QUIT received, can be ignored
	Status poll();

	// render image
	void render();

	void enableMenuBar(bool state);
	bool isEnabledMenuBar();

	void enableScroll(bool state);
	bool isEnabledScroll();

	// returns native HWND
	HWND native();

	void setIcon(HICON icon);
	void setIcon(unsigned id);

	HICON getIcon();

	State getState();

	void setState(State state);
	// window is focused
	bool isFocused();
	// focus window
	void focus();

	void setName(const wchar_t* name);
	const wchar_t* getName();

	void enableSizing(bool state);
	bool isEnabledSizing();

	void setSizeMin(const Vec2D<size_t>& min);
	Vec2D<size_t> getSizeMin();

	void setSizeMax(const Vec2D<size_t>& max);
	Vec2D<size_t> getSizeMax();

	// schedule window resize, will be applied after next poll
	void setSize(const Vec2D<size_t>& size);
	Vec2D<size_t> getSize();

	// schedule window move, will be applied after next poll
	void setPosition(const Vec2D<size_t>& pos);
	Vec2D<size_t> getPosition();
	void setPositionCenter();

	static Vec2D<size_t> getScreenSize();

	void enableMoving(bool state);
	bool isEnabledMoving();
	bool isMoving();

	static const char* formatError(ErrorCode code);

	// looks for a copy of the window with the same name and class
	// useful if your app should only be run once
	bool focusTwin();
private:
	bool beginFrame();
	void endFrame();

	bool createWindow(State state);
	void cleanupWindow();

	void beginDrag();
	void endDrag();
	void updateDrag();

	static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};