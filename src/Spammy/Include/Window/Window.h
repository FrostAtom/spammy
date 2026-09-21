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
        T x = 0, y = 0;
    };

private:
    wchar_t _wndName[64], _className[64];
    char _u8wndName[64 * 4];
    HICON _icon = NULL;
    HICON _trayIconOverride = NULL; // shown in the tray instead of _icon while set
    ATOM _atom = NULL;
    HWND _hwnd = NULL;
    ImGuiContext* _imCtx = NULL;
    bool _wantQuit = false, _mustQuit = false;

    DWORD _imWndFlags;
    bool _movable = false;
    Vec2D<int> _size = {512, 512};
    Vec2D<int> _position = {CW_USEDEFAULT, CW_USEDEFAULT};

    bool _moving = false;
    Vec2D<int> _movePos;

    unsigned _dpi = 96;
    float _scaleFactor = 1.f;
    float _scale = 1.f;
    bool _scalePending = false;
    bool _inFrame = false;

    LPDIRECT3D9 _d3d = NULL;
    LPDIRECT3DDEVICE9 _d3dDevice = NULL;
    D3DPRESENT_PARAMETERS _d3dParams;
    HRESULT _lastError = 0;
    std::unique_ptr<TrayIcon> _trayIcon;

public:
    Window(const wchar_t* className, const wchar_t* wndName = NULL);
    virtual ~Window();

    ErrorCode Initialize();
    void SetTrayIcon(std::unique_ptr<TrayIcon> icon);
    HWND Native() const { return _hwnd; }
    void Update();

    void Close() { _mustQuit = true; }
    void Cleanup();
    bool MustQuit() const { return _mustQuit; }
    bool WantQuit() { return std::exchange(_wantQuit, false); }

    bool IsWndMaximized() const { return ShowCmd() == SW_MAXIMIZE; }
    bool IsWndNormalized() const { return ShowCmd() == SW_NORMAL; }
    void Show();
    void Hide();
    bool IsShown() const;
    void Focus();

    void EnableTitleBar(bool v = true);
    void EnableMoving(bool v = true) { _movable = v; }

    void SetIcon(HICON icon);
    void SetIcon(unsigned id);
    // NULL restores the window icon in the tray; the caller keeps ownership of the HICON
    void SetTrayIconOverride(HICON icon);

    void SetName(const wchar_t* name);

    void SetSize(const Vec2D<int>& v);
    void SetScaleFactor(float factor);
    void SetPosition(const Vec2D<int>& pos);
    void ResetPosition();

    static Vec2D<int> GetScreenSize();
    static const char* FormatError(ErrorCode code);
    // HRESULT of the last failed Initialize() step (0 if none)
    HRESULT LastError() const { return _lastError; }

protected:
    bool IsReady() const { return _hwnd && _d3dDevice; }
    virtual void Draw() = 0;
    bool BeginFrame();
    void EndFrame();

    bool CreateWnd();
    void CleanupWnd();

    bool CreateDevice();
    void CleanupDevice();
    void ResetDevice();
    void Render();

    void StartMove();
    void StopMove() { _moving = false; }
    void UpdateMove();

    void ApplyScale(bool keepCenter);
    Vec2D<int> ScaledSize() const;

    virtual bool HandleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result);

    static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    int ShowCmd() const;
    HICON TrayIconImage() const { return _trayIconOverride ? _trayIconOverride : _icon; }
    void ApplyWndIcon();
    void MoveToStoredRect();
};
