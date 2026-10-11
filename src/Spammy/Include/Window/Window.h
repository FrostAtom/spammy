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

    struct Vec2i {
        int x = 0, y = 0;
    };

private:
    wchar_t _wndName[64], _className[64];
    char _u8wndName[64 * 4];
    HICON _icon = nullptr;
    HICON _trayIconOverride = nullptr;
    ATOM _atom = NULL;
    HWND _hwnd = nullptr;
    ImGuiContext* _imCtx = nullptr;
    bool _wantQuit = false, _mustQuit = false;

    Vec2i _size = {512, 512};
    Vec2i _position = {CW_USEDEFAULT, CW_USEDEFAULT};

    bool _dragging = false;
    Vec2i _dragOffset;

    enum Swipe {
        Swipe_None,
        Swipe_In,
        Swipe_Out
    };
    Swipe _swipe = Swipe_None;
    bool _quitAfterSwipe = false;
    float _swipeProgress = 0.f;
    float _swipeStartOffset = 0.f, _swipeStartAlpha = 0.f;
    float _swipeOffset = 0.f, _swipeAlpha = 1.f;
    int _swipeRestX = 0;

    unsigned _dpi = 96;
    float _scaleFactor = 1.f;
    float _scale = 1.f;
    bool _sizeAndScalePending = false;
    bool _inFrame = false;

    LPDIRECT3D9 _d3d = nullptr;
    LPDIRECT3DDEVICE9 _d3dDevice = nullptr;
    D3DPRESENT_PARAMETERS _d3dParams; // CreateDevice fills in the back buffer size
    bool _needsReset = false;
    HRESULT _lastError = 0;
    std::vector<std::unique_ptr<ImTextureData>> _textures;
    std::unique_ptr<TrayIcon> _trayIcon;

public:
    Window(const wchar_t* className, const wchar_t* wndName = nullptr);
    virtual ~Window();

    ErrorCode Initialize();
    void SetTrayIcon(std::unique_ptr<TrayIcon> icon);
    HWND Native() const { return _hwnd; }
    void Update();

    void Close();
    void Cleanup();
    bool MustQuit() const { return _mustQuit; }
    bool WantQuit() { return std::exchange(_wantQuit, false); }

    bool IsOnScreen() const;
    void Show();
    void Hide();
    bool IsHiddenOrHiding() const;
    void Focus();

    void SetIcon(HICON icon);
    void SetTrayIconOverride(HICON icon);

    void SetSize(const Vec2i& size);
    void SetScaleFactor(float factor);
    void CenterOnWorkArea();

    static const char* FormatError(ErrorCode code);
    HRESULT LastInitError() const { return _lastError; }

protected:
    virtual void Draw() = 0;
    virtual bool HandleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result);

    ImTextureRef CreateTexture(int width, int height, std::span<const ImU32> pixels);

private:
    bool IsReady() const { return _hwnd && _d3dDevice; }
    bool BeginFrame();
    void EndFrame();

    bool CreateWnd();
    void CleanupWnd();

    bool CreateDevice();
    void CleanupDevice();
    bool ResetDevice();
    void Render();

    void StartDrag();
    void StopDrag() { _dragging = false; }
    void UpdateDrag();

    void StartSwipe(Swipe swipe);
    void StepSwipe(float dt);
    void ApplySwipeFrame();
    void FinishSwipe();
    void JumpSwipeToEnd();
    void SetLayered(bool layered);

    enum ScaleAnchor {
        ScaleAnchor_Center,
        ScaleAnchor_StoredPosition
    };
    float DpiScale() const { return _scaleFactor * (float)_dpi / 96.f; }
    void RequestSizeAndScale();
    void ApplySizeAndScale(ScaleAnchor anchor);
    Vec2i ScaledSize() const;

    HICON TrayIconImage() const { return _trayIconOverride ? _trayIconOverride : _icon; }
    void ApplyWndIcon();
    void MoveToStoredRect();

    static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};
