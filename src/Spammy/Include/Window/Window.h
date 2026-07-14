#pragma once
#include "Headers.h"
#include "Window/TrayIcon.h"

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

private:
    wchar_t _wndName[64], _className[64];
    char _u8wndName[64];
    HICON _icon;
    ATOM _atom;
    HWND _hwnd;
    ImGuiContext* _imCtx;
    bool _wantQuit, _mustQuit;

    DWORD _imWndFlags;
    bool _movable;
    Vec2D<size_t> _size;
    Vec2D<size_t> _position;

    bool _moving;
    Vec2D<size_t> _movePos;

    LPDIRECT3D9 _d3d;
    LPDIRECT3DDEVICE9 _d3dDevice;
    D3DPRESENT_PARAMETERS _d3dParams;
    TrayIcon* _trayIcon;
    static const char* _errorCodeNames[];

public:
    Window(const wchar_t* className, const wchar_t* wndName = NULL);
    virtual ~Window();

    ErrorCode Initialize();
    void SetTrayIcon(TrayIcon* icon);
    HWND Native();
    void Update();

    void Close();
    void Cleanup();
    bool MustQuit();
    bool WantQuit();

    bool IsWndMaximized();
    bool IsWndNormalized();
    void Show();
    void Hide();
    bool IsShown();
    void Focus();

    void EnableMenuBar(bool v = true);
    void EnableTitleBar(bool v = true);
    void EnableMoving(bool v = true);

    void SetIcon(HICON icon);
    void SetIcon(unsigned id);

    void SetName(const wchar_t* name);

    void SetSize(const Vec2D<size_t>& v);
    Vec2D<size_t> GetSize();
    void SetPosition(const Vec2D<size_t>& pos);
    void ResetPosition();

    static Vec2D<size_t> GetScreenSize();
    static const char* FormatError(ErrorCode code);

protected:
    bool IsReady();
    virtual void Draw() = 0;
    virtual bool BeginFrame();
    virtual void EndFrame();

    bool CreateWnd();
    void CleanupWnd();

    bool CreateDevice();
    void CleanupDevice();
    void ResetDevice();
    void Render();

    void StartMove();
    void StopMove();
    void UpdateMove();

    virtual bool HandleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result);

    static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};
