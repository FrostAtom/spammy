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

	using RenderCallback_t = std::function<void()>;
	using PendingTask_t = std::function<void()>;
private:
	wchar_t _wndName[64], _className[64];
	char _u8wndName[64];
	HICON _icon;
	ATOM _atom;
	HWND _hwnd;
	ImGuiContext* _imCtx;
	bool _wantQuit, _mustQuit;

	DWORD _imWndFlags;
	bool _sizable, _movable;
	Vec2D<size_t> _sizeMin, _sizeMax, _size;
	Vec2D<size_t> _position;

	bool _moving;
	Vec2D<size_t> _movePos;

	WindowRenderer* _renderBackend;
	TrayIcon* _trayIcon;
	static const char* ErrorCodeNames[];
public:
	Window(const wchar_t* className, const wchar_t* wndName = NULL);
	virtual ~Window();

	ErrorCode initialize();
	void setRenderBackend(WindowRenderer* renderer);
	void setTrayIcon(TrayIcon* icon);
	HWND native();
	void update();

	void quit();
	void close();
	void cleanup();
	bool mustQuit();
	bool wantQuit();

	void minimize();
	bool isMinimized();
	void maximize();
	bool isMaximized();
	void normalize();
	bool isNormalized();
	void show();
	void hide();
	bool isShown();
	void focus();
	bool isFocused();

	void enableMenuBar(bool v = true);
	bool isEnabledMenuBar();

	void enableScroll(bool v = true);
	bool isEnabledScroll();

	void enableSizing(bool v = true);
	bool isEnabledSizing();

	void enableMoving(bool v = true);
	bool isEnabledMoving();

	void setIcon(HICON icon);
	void setIcon(unsigned id);
	HICON getIcon();

	void setName(const wchar_t* name);
	const wchar_t* getName();

	void setSizeMin(const Vec2D<size_t>& v);
	Vec2D<size_t> getSizeMin();
	void setSizeMax(const Vec2D<size_t>& v);
	Vec2D<size_t> getSizeMax();
	void setSizeMinMax(const Vec2D<size_t>& min, const Vec2D<size_t>& max);
	void setSize(const Vec2D<size_t>& v);
	Vec2D<size_t> getSize();
	void setPosition(const Vec2D<size_t>& pos);
	Vec2D<size_t> getPosition();
	void resetPosition();

	static Vec2D<size_t> getScreenSize();
	static const char* formatError(ErrorCode code);

protected:
	bool isReady();
	virtual void draw() = 0;
	virtual bool beginFrame();
	virtual void endFrame();

	bool createWindow();
	void cleanupWindow();

	void startMove();
	void stopMove();
	void updateMove();

	virtual bool handleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result);

	static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};