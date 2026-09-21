#include "Window/Window.h"
#include "ImGui.h"

static constexpr std::array<const char*, Window::ErrorCode_COUNT> s_errorCodeNames = {
    "OK", "Invalid call", "Windows error", "Renderer error", "ImGui error",
};

static constexpr DWORD ImGuiWindowDisableScrollMask = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
static constexpr DWORD ImGuiWindowNoTitleBarMask = ImGuiWindowFlags_NoTitleBar;

const char* Window::FormatError(ErrorCode code)
{
    return s_errorCodeNames[code];
}

Window::Window(const wchar_t* className, const wchar_t* wndName) : _imWndFlags(ImGuiWindowDisableScrollMask)
{
    wcsncpy_s(_className, className, std::size(_className));
    SetName(wndName ? wndName : className);

    _d3dParams = {
        // D3DFMT_UNKNOWN: an explicit format with alpha is needed only for per-pixel alpha composition
        .BackBufferFormat = D3DFMT_UNKNOWN,
        .SwapEffect = D3DSWAPEFFECT_DISCARD,
        .Windowed = TRUE,
        .EnableAutoDepthStencil = TRUE,
        .AutoDepthStencilFormat = D3DFMT_D16,
        .PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE, // no vsync
    };
}

Window::~Window()
{
    Cleanup();
}

Window::ErrorCode Window::Initialize()
{
    if (_hwnd) return ErrorCode_InvalidCall;

    _imCtx = ImGui::CreateContext();
    if (!_imCtx) return ErrorCode_ImGuiError;

    if (!CreateWnd()) {
        _lastError = HRESULT_FROM_WIN32(GetLastError());
        Cleanup();
        return ErrorCode_WinError;
    }

    if (!CreateDevice()) {
        Cleanup();
        return ErrorCode_RendererError;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigWindowsResizeFromEdges = false;
    io.ConfigInputTrickleEventQueue = false;
    io.IniFilename = NULL;

    ImGui::LoadUiFonts();

    if (_trayIcon) _trayIcon->Create(_hwnd, TrayIconImage());

    return ErrorCode_OK;
}

void Window::SetTrayIcon(std::unique_ptr<TrayIcon> icon)
{
    _trayIcon = std::move(icon);
    if (_hwnd && _trayIcon) _trayIcon->Create(_hwnd, TrayIconImage());
}

void Window::SetTrayIconOverride(HICON icon)
{
    if (_trayIconOverride == icon) return;
    _trayIconOverride = icon;
    if (_trayIcon) _trayIcon->UpdateIcon(TrayIconImage());
}

void Window::Cleanup()
{
    _trayIcon.reset();
    CleanupDevice();
    CleanupWnd();
    if (_imCtx) {
        ImGui::DestroyContext(_imCtx);
        _imCtx = NULL;
    }
}

void Window::ApplyWndIcon()
{
    SendMessageW(_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)_icon);
    SendMessageW(_hwnd, WM_SETICON, ICON_BIG, (LPARAM)_icon);
}

void Window::SetIcon(HICON icon)
{
    _icon = icon;
    if (!_hwnd) return;
    ApplyWndIcon();
    if (_trayIcon) _trayIcon->UpdateIcon(TrayIconImage());
}

void Window::SetIcon(unsigned id)
{
    SetIcon(LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(id)));
}

void Window::Focus()
{
    if (_hwnd) SetForegroundWindow(_hwnd);
}

int Window::ShowCmd() const
{
    if (!_hwnd) return SW_HIDE;
    WINDOWPLACEMENT placement = {sizeof(placement)};
    GetWindowPlacement(_hwnd, &placement);
    return placement.showCmd;
}

void Window::Show()
{
    if (_hwnd) ShowWindow(_hwnd, SW_SHOWNORMAL);
}

void Window::Hide()
{
    if (_hwnd) ShowWindow(_hwnd, SW_HIDE);
}

bool Window::IsShown() const
{
    return _hwnd && IsWindowVisible(_hwnd);
}

void Window::SetName(const wchar_t* name)
{
    wcsncpy_s(_wndName, name, std::size(_wndName));
    _u8wndName[0] = '\0';
    WideCharToMultiByte(CP_UTF8, 0, name, -1, _u8wndName, (int)std::size(_u8wndName), NULL, NULL);
    if (_hwnd) SetWindowTextW(_hwnd, name);
}

void Window::MoveToStoredRect()
{
    if (!_hwnd) return;
    Vec2D<int> scaled = ScaledSize();
    MoveWindow(_hwnd, _position.x, _position.y, scaled.x, scaled.y, FALSE);
}

void Window::SetSize(const Vec2D<int>& size)
{
    _size = size;
    MoveToStoredRect();
}

void Window::SetPosition(const Vec2D<int>& position)
{
    _position = position;
    MoveToStoredRect();
}

void Window::ResetPosition()
{
    Vec2D<int> screen = GetScreenSize();
    Vec2D<int> size = ScaledSize();
    SetPosition({(screen.x - size.x) / 2, (screen.y - size.y) / 2});
}

Window::Vec2D<int> Window::ScaledSize() const
{
    return {(int)lroundf(_size.x * _scale), (int)lroundf(_size.y * _scale)};
}

void Window::SetScaleFactor(float factor)
{
    _scaleFactor = factor;
    // resizing mid-frame would desync ImGui's display size from the backbuffer
    if (_inFrame)
        _scalePending = true;
    else
        ApplyScale(true);
}

void Window::ApplyScale(bool keepCenter)
{
    float scale = _scaleFactor * (float)_dpi / 96.f;
    MONITORINFO monitor = {sizeof(monitor)};
    bool hasMonitor = _hwnd && GetMonitorInfoW(MonitorFromWindow(_hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
    const RECT& work = monitor.rcWork;
    if (hasMonitor && _size.x > 0 && _size.y > 0) {
        float fitW = (float)(work.right - work.left) / (float)_size.x;
        float fitH = (float)(work.bottom - work.top) / (float)_size.y;
        scale = ImMin(scale, ImMin(fitW, fitH));
    }
    _scale = ImMax(scale, 0.5f);

    if (_imCtx) {
        _imCtx->Style.CircleTessellationMaxError = 0.3f / _scale;
        _imCtx->Style.CurveTessellationTol = 1.25f / _scale;
        _imCtx->DrawListSharedData.InitialFringeScale = 1.f / _scale;
    }

    if (!_hwnd) return;
    Vec2D<int> size = ScaledSize();
    Vec2D<int> pos = _position;
    if (keepCenter) {
        RECT rect;
        GetWindowRect(_hwnd, &rect);
        pos = {((int)rect.left + (int)rect.right - size.x) / 2, ((int)rect.top + (int)rect.bottom - size.y) / 2};
    }
    if (hasMonitor) {
        pos.x = ImClamp(pos.x, (int)work.left, ImMax((int)work.left, (int)work.right - size.x));
        pos.y = ImClamp(pos.y, (int)work.top, ImMax((int)work.top, (int)work.bottom - size.y));
    }
    MoveWindow(_hwnd, pos.x, pos.y, size.x, size.y, TRUE);
}

Window::Vec2D<int> Window::GetScreenSize()
{
    RECT rect;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rect, 0);
    return {rect.right - rect.left, rect.bottom - rect.top};
}

void Window::Update()
{
    if (std::exchange(_scalePending, false)) ApplyScale(true);
    if (!IsReady()) return;
    if (BeginFrame()) Draw();
    EndFrame();
}

void Window::EnableTitleBar(bool state)
{
    if (state)
        _imWndFlags &= ~ImGuiWindowNoTitleBarMask;
    else
        _imWndFlags |= ImGuiWindowNoTitleBarMask;
}

bool Window::BeginFrame()
{
    _inFrame = true;
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplayFramebufferScale = ImVec2(_scale, _scale);
    if (_scale != 1.f) {
        // ImGui runs in unscaled logical units; the backend reports physical pixels
        io.DisplaySize = ImVec2(io.DisplaySize.x / _scale, io.DisplaySize.y / _scale);
        for (ImGuiInputEvent& event : GImGui->InputEventsQueue)
            if (event.Type == ImGuiInputEventType_MousePos && event.MousePos.PosX != -FLT_MAX) {
                event.MousePos.PosX /= _scale;
                event.MousePos.PosY /= _scale;
            }
    }
    ImGui::NewFrame();
    bool isShown = true;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    DWORD flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus;
    bool result = ImGui::Begin(_u8wndName, &isShown, flags | _imWndFlags);

    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (ctx->CurrentWindow && ctx->MovingWindow == ctx->CurrentWindow)
        StartMove();
    else
        StopMove();

    if (!isShown) {
        _wantQuit = true;
        PostMessageW(_hwnd, WM_QUIT, NULL, NULL);
    }
    return result;
}

void Window::EndFrame()
{
    ImGui::End();
    ImGui::EndFrame();
    Render();
    _inFrame = false;
}

bool Window::CreateWnd()
{
    WNDCLASSEXW wc = {sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandleW(NULL), NULL, NULL, NULL, NULL,
                      _className,          NULL};
    _atom = RegisterClassExW(&wc);
    if (!_atom) return false;

    DWORD style = WS_POPUP | WS_THICKFRAME;
    _hwnd = CreateWindowExW(0, wc.lpszClassName, _wndName, style, _position.x, _position.y, _size.x, _size.y, NULL,
                            NULL, wc.hInstance, this);
    if (!_hwnd) {
        CleanupWnd();
        return false;
    }
    if (_icon) ApplyWndIcon();

    // a 1px DWM frame gives the borderless popup the system drop shadow
    MARGINS shadowMargins = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(_hwnd, &shadowMargins);

    _dpi = GetDpiForWindow(_hwnd);
    _scale = _scaleFactor * (float)_dpi / 96.f;

    ImGui_ImplWin32_Init(_hwnd);
    return true;
}

void Window::CleanupWnd()
{
    if (_hwnd) {
        ImGui_ImplWin32_Shutdown();
        DestroyWindow(_hwnd);
        _hwnd = NULL;
    }
    if (_atom) {
        UnregisterClassW(MAKEINTATOM(_atom), NULL);
        _atom = NULL;
    }
}

bool Window::CreateDevice()
{
    _lastError = 0;
    _d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!_d3d) {
        _lastError = HRESULT_FROM_WIN32(GetLastError());
        return false;
    }

    // hardware T&L is missing on basic display adapters, RDP sessions, VMs and some old iGPUs — fall back gracefully
    static constexpr DWORD s_behaviorFlags[] = {
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        D3DCREATE_MIXED_VERTEXPROCESSING,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
    };
    HRESULT hRes = E_FAIL;
    for (DWORD flags : s_behaviorFlags) {
        hRes = _d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, _hwnd, flags, &_d3dParams, &_d3dDevice);
        if (SUCCEEDED(hRes)) break;
    }
    if (FAILED(hRes)) {
        _lastError = hRes;
        _d3d->Release();
        _d3d = NULL;
        return false;
    }
    ImGui_ImplDX9_Init(_d3dDevice);
    return true;
}

void Window::CleanupDevice()
{
    if (_d3dDevice) {
        ImGui_ImplDX9_Shutdown();
        _d3dDevice->Release();
        _d3dDevice = NULL;
    }
    if (_d3d) {
        _d3d->Release();
        _d3d = NULL;
    }
}

void Window::ResetDevice()
{
    if (!_d3dDevice) return;
    ImGui_ImplDX9_InvalidateDeviceObjects();
    _d3dDevice->Reset(&_d3dParams);
    ImGui_ImplDX9_CreateDeviceObjects();
}

void Window::Render()
{
    _d3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    _d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    _d3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    _d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(10, 13, 19, 255), 1.0f, 0);
    if (_d3dDevice->BeginScene() >= 0) {
        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        if (_scale != 1.f) {
            // scale the logical-unit draw data back to physical pixels (see BeginFrame)
            drawData->DisplaySize = ImVec2(drawData->DisplaySize.x * _scale, drawData->DisplaySize.y * _scale);
            drawData->FramebufferScale = ImVec2(1.f, 1.f);
            for (ImDrawList* cmdList : drawData->CmdLists) {
                for (ImDrawVert& vertex : cmdList->VtxBuffer) {
                    vertex.pos.x *= _scale;
                    vertex.pos.y *= _scale;
                }
                for (ImDrawCmd& cmd : cmdList->CmdBuffer) {
                    cmd.ClipRect.x *= _scale;
                    cmd.ClipRect.y *= _scale;
                    cmd.ClipRect.z *= _scale;
                    cmd.ClipRect.w *= _scale;
                }
            }
        }
        ImGui_ImplDX9_RenderDrawData(drawData);
        _d3dDevice->EndScene();
    }
    if (_d3dDevice->Present(NULL, NULL, NULL, NULL) == D3DERR_DEVICELOST &&
        _d3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
        ResetDevice();
}

void Window::StartMove()
{
    if (!_movable || _moving) return;
    POINT cursor;
    GetCursorPos(&cursor);
    RECT rect;
    GetWindowRect(_hwnd, &rect);
    _movePos = {cursor.x - rect.left, cursor.y - rect.top};
    _moving = true;
}

void Window::UpdateMove()
{
    if (!_moving) return;
    if (!_movable) return StopMove();
    POINT cursor;
    GetCursorPos(&cursor);
    RECT rect;
    GetWindowRect(_hwnd, &rect);
    MoveWindow(_hwnd, cursor.x - _movePos.x, cursor.y - _movePos.y, rect.right - rect.left, rect.bottom - rect.top,
               TRUE);
}

// imgui_impl_win32.h keeps this declaration in '#if 0' to avoid including <windows.h>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool Window::HandleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
    *result = ImGui_ImplWin32_WndProcHandler(_hwnd, msg, wParam, lParam);
    if (*result) return true;
    if (_trayIcon && _trayIcon->HandleMessage(msg, wParam, lParam)) return true;

    switch (msg) {
    case WM_CLOSE: _wantQuit = true; return true;
    case WM_ENDSESSION: _mustQuit = true; return true;
    case WM_SIZING:
        // keep rendering while the modal size loop blocks the message pump
        Update();
        *result = TRUE;
        return true;
    case WM_MOVE: _position = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}; return true;
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED && _d3dDevice) {
            _d3dParams.BackBufferWidth = LOWORD(lParam);
            _d3dParams.BackBufferHeight = HIWORD(lParam);
            ResetDevice();
        }
        return true;
    case WM_DPICHANGED: {
        _dpi = HIWORD(wParam);
        const RECT* suggested = (const RECT*)lParam;
        _position = {(int)suggested->left, (int)suggested->top};
        ApplyScale(false);
        return true;
    }
    case WM_MOUSEMOVE:
        if (wParam & MK_LBUTTON) UpdateMove();
        return true;
    case WM_SYSCOMMAND:
        // Alt alone would otherwise open the (non-existent) system menu and steal focus
        if ((wParam & 0xfff0) == SC_KEYMENU) return true;
        break;
    case WM_NCACTIVATE:
        // lParam=-1 tells DefWindowProc not to repaint the non-client area, which we don't have
        *result = DefWindowProcW(_hwnd, msg, wParam, -1);
        return true;
    case WM_NCCALCSIZE:
        // returning 0 keeps the whole WS_THICKFRAME window as client area (no visible frame)
        if (wParam && !IsWndMaximized()) return true;
        break;
    case WM_NCHITTEST: *result = HTCLIENT; return true;
    }
    return false;
}

LRESULT __stdcall Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_CREATE) {
        Window* self = (Window*)((const CREATESTRUCTW*)lParam)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        return 0;
    }
    Window* self = (Window*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    LRESULT result;
    if (self && self->HandleWndProc(msg, wParam, lParam, &result)) return result;
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
