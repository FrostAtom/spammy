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
    HICON _icon = nullptr;
    HICON _trayIconOverride = nullptr; // shown in the tray instead of _icon while set
    ATOM _atom = NULL;
    HWND _hwnd = nullptr;
    ImGuiContext* _imCtx = nullptr;
    bool _wantQuit = false, _mustQuit = false;

    ImGuiWindowFlags _imWndFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    bool _movable = false;
    Vec2D<int> _size = {512, 512};
    Vec2D<int> _position = {CW_USEDEFAULT, CW_USEDEFAULT};

    bool _moving = false;
    Vec2D<int> _movePos;

    // show/hide slides the window in from the left and out to the right while fading it (a layered window meanwhile)
    enum Swipe { Swipe_None, Swipe_In, Swipe_Out };
    Swipe _swipe = Swipe_None;
    bool _swipeQuit = false; // the swipe out ends the app instead of hiding the window
    float _swipeT = 0.f;
    float _swipeFromX = 0.f, _swipeFromAlpha = 0.f; // where the current swipe started, x relative to the rest spot
    float _swipeX = 0.f, _swipeAlpha = 1.f;         // the frame shown now
    int _restX = 0;                                 // where the window sits when no swipe is running

    unsigned _dpi = 96;
    float _scaleFactor = 1.f;
    float _scale = 1.f;
    bool _scalePending = false;
    bool _inFrame = false;

    LPDIRECT3D9 _d3d = nullptr;
    LPDIRECT3DDEVICE9 _d3dDevice = nullptr;
    D3DPRESENT_PARAMETERS _d3dParams;
    HRESULT _lastError = 0;
    std::unique_ptr<TrayIcon> _trayIcon;

public:
    Window(const wchar_t* className, const wchar_t* wndName = nullptr);
    virtual ~Window();

    ErrorCode Initialize();
    void SetTrayIcon(std::unique_ptr<TrayIcon> icon);
    HWND Native() const { return _hwnd; }
    void Update();

    // swipes the window out first when it's on screen
    void Close();
    void Cleanup();
    bool MustQuit() const { return _mustQuit; }
    bool WantQuit() { return std::exchange(_wantQuit, false); }

    bool IsWndMaximized() const { return ShowCmd() == SW_MAXIMIZE; }
    bool IsWndNormalized() const { return ShowCmd() == SW_NORMAL; }
    // Show swipes in from hidden (or turns a running swipe out back), Hide swipes out; a minimized window skips it
    void Show();
    void Hide();
    // false already while swiping out
    bool IsShown() const;
    void Focus();

    void EnableTitleBar(bool v = true);
    void EnableMoving(bool v = true) { _movable = v; }

    void SetIcon(HICON icon);
    // NULL restores the window icon in the tray; the caller keeps ownership of the HICON
    void SetTrayIconOverride(HICON icon);

    void SetName(const wchar_t* name);

    void SetSize(const Vec2D<int>& v);
    void SetScaleFactor(float factor);
    void SetPosition(const Vec2D<int>& pos);
    // centers the window on the primary monitor's work area
    void ResetPosition();

    static const char* FormatError(ErrorCode code);
    // HRESULT of the last failed Initialize() step (0 if none)
    HRESULT LastError() const { return _lastError; }

protected:
    virtual void Draw() = 0;
    virtual bool HandleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result);

private:
    bool IsReady() const { return _hwnd && _d3dDevice; }
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

    void StartSwipe(Swipe swipe);
    void StepSwipe(float dt);
    void ApplySwipeFrame();
    void FinishSwipe();
    void SetLayered(bool layered);

    float DpiScale() const { return _scaleFactor * (float)_dpi / 96.f; }
    void ApplyScale(bool keepCenter);
    Vec2D<int> ScaledSize() const;

    int ShowCmd() const;
    HICON TrayIconImage() const { return _trayIconOverride ? _trayIconOverride : _icon; }
    void ApplyWndIcon();
    void MoveToStoredRect();

    static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};
