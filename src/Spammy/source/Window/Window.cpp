#include "Window/Window.h"
#include "ImGui.h"

const char* Window::_errorCodeNames[] = {
    "OK", "Invalid call", "Windows error", "Renderer error", "ImGui errror",
};

const char* Window::FormatError(ErrorCode code)
{
    return _errorCodeNames[code];
}

static const DWORD ImGuiWindowDisableScrollMask = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
static const DWORD ImGuiWindowMenuBarMask = ImGuiWindowFlags_MenuBar;
static const DWORD ImGuiWindowNoTitleBarMask = ImGuiWindowFlags_NoTitleBar;

Window::Window(const wchar_t* className, const wchar_t* wndName)
    : _icon(NULL),
      _atom(NULL),
      _hwnd(NULL),
      _imCtx(NULL),
      _wantQuit(false),
      _mustQuit(false),
      _imWndFlags(ImGuiWindowDisableScrollMask),
      _movable(false),
      _moving(false),
      _dpi(96),
      _scaleFactor(1.f),
      _scale(1.f),
      _scalePending(false),
      _inFrame(false),
      _d3d(NULL),
      _d3dDevice(NULL),
      _trayIcon(NULL),
      _position(CW_USEDEFAULT, CW_USEDEFAULT),
      _size(512, 512)
{
    if (!wndName) wndName = className;
    wcsncpy_s(_className, className, std::size(_className));
    SetName(wndName);

    memset(&_d3dParams, NULL, sizeof(_d3dParams));
    _d3dParams.Windowed = TRUE;
    _d3dParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
    _d3dParams.BackBufferFormat =
        D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    _d3dParams.EnableAutoDepthStencil = TRUE;
    _d3dParams.AutoDepthStencilFormat = D3DFMT_D16;
    _d3dParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; // Present without vsync
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

    if (_trayIcon) _trayIcon->Create(_hwnd, _icon);

    return ErrorCode_OK;
}

void Window::SetTrayIcon(TrayIcon* icon)
{
    if (_trayIcon) {
        delete _trayIcon;
    }
    _trayIcon = icon;
    if (_hwnd) _trayIcon->Create(_hwnd, _icon);
}

void Window::Cleanup()
{
    if (_trayIcon) {
        delete _trayIcon;
        _trayIcon = NULL;
    }
    CleanupDevice();
    CleanupWnd();
    if (_imCtx) {
        ImGui::DestroyContext(_imCtx);
        _imCtx = NULL;
    }
}

void Window::Close()
{
    _mustQuit = true;
}

void Window::SetIcon(HICON icon)
{
    _icon = icon;
    if (_hwnd) {
        SendMessageW(_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)_icon);
        SendMessageW(_hwnd, WM_SETICON, ICON_BIG, (LPARAM)_icon);
        if (_trayIcon) _trayIcon->UpdateIcon(_icon);
    }
}

void Window::SetIcon(unsigned id)
{
    HICON icon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(id));
    SetIcon(icon);
}

bool Window::MustQuit()
{
    return _mustQuit;
}

bool Window::WantQuit()
{
    if (_wantQuit) {
        _wantQuit = false;
        return true;
    }
    return false;
}

void Window::Focus()
{
    if (_hwnd) SetForegroundWindow(_hwnd);
}

static int GetShowCmd(HWND hwnd)
{
    WINDOWPLACEMENT wPos = {sizeof(wPos)};
    GetWindowPlacement(hwnd, &wPos);
    return wPos.showCmd;
}

bool Window::IsWndMaximized()
{
    if (!_hwnd) return false;
    int cmd = GetShowCmd(_hwnd);
    return cmd == SW_MAXIMIZE;
}

bool Window::IsWndNormalized()
{
    if (!_hwnd) return false;
    int cmd = GetShowCmd(_hwnd);
    return cmd == SW_NORMAL;
}

void Window::Show()
{
    if (_hwnd) ShowWindow(_hwnd, SW_SHOWNORMAL);
}

void Window::Hide()
{
    if (_hwnd) ShowWindow(_hwnd, SW_HIDE);
}

bool Window::IsShown()
{
    if (!_hwnd) return false;
    return (bool)(GetWindowStyle(_hwnd) & WS_VISIBLE);
}

void Window::SetName(const wchar_t* name)
{
    wcsncpy_s(_wndName, name, std::size(_wndName));
    _u8wndName[0] = '\0';
    WideCharToMultiByte(CP_UTF8, 0, name, -1, _u8wndName, (int)std::size(_u8wndName), NULL, NULL);
    if (_hwnd) SetWindowTextW(_hwnd, name);
}

Window::Vec2D<int> Window::GetSize()
{
    return _size;
}

void Window::SetSize(const Vec2D<int>& size)
{
    _size = size;
    if (_hwnd) {
        Vec2D<int> scaled = ScaledSize();
        MoveWindow(_hwnd, _position.x, _position.y, scaled.x, scaled.y, FALSE);
    }
}

void Window::SetPosition(const Vec2D<int>& position)
{
    _position = position;
    if (_hwnd) {
        Vec2D<int> scaled = ScaledSize();
        MoveWindow(_hwnd, _position.x, _position.y, scaled.x, scaled.y, FALSE);
    }
}

void Window::ResetPosition()
{
    Vec2D<int> pos = GetScreenSize();
    Vec2D<int> size = ScaledSize();
    pos.x = (pos.x - size.x) / 2;
    pos.y = (pos.y - size.y) / 2;
    SetPosition(pos);
}

Window::Vec2D<int> Window::ScaledSize()
{
    return {(int)lroundf(_size.x * _scale), (int)lroundf(_size.y * _scale)};
}

void Window::SetScaleFactor(float factor)
{
    _scaleFactor = factor;
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
    if (hasMonitor && _size.x > 0 && _size.y > 0) {
        float fitW = (float)(monitor.rcWork.right - monitor.rcWork.left) / (float)_size.x;
        float fitH = (float)(monitor.rcWork.bottom - monitor.rcWork.top) / (float)_size.y;
        scale = ImMin(scale, ImMin(fitW, fitH));
    }
    _scale = ImMax(scale, 0.5f);

    if (_imCtx) {
        _imCtx->Style.CircleTessellationMaxError = 0.3f / _scale;
        _imCtx->Style.CurveTessellationTol = 1.25f / _scale;
        _imCtx->DrawListSharedData.InitialFringeScale = 1.f / _scale;
    }

    if (!_hwnd) return;
    RECT rect;
    GetWindowRect(_hwnd, &rect);
    Vec2D<int> size = ScaledSize();
    Vec2D<int> pos = _position;
    if (keepCenter)
        pos = {((int)rect.left + (int)rect.right - size.x) / 2, ((int)rect.top + (int)rect.bottom - size.y) / 2};
    if (hasMonitor) {
        pos.x = ImClamp(pos.x, (int)monitor.rcWork.left,
                        ImMax((int)monitor.rcWork.left, (int)monitor.rcWork.right - size.x));
        pos.y = ImClamp(pos.y, (int)monitor.rcWork.top,
                        ImMax((int)monitor.rcWork.top, (int)monitor.rcWork.bottom - size.y));
    }
    MoveWindow(_hwnd, pos.x, pos.y, size.x, size.y, TRUE);
}

Window::Vec2D<int> Window::GetScreenSize()
{
    RECT rect;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rect, 0);
    return {rect.right - rect.left, rect.bottom - rect.top};
}

void Window::EnableMoving(bool state)
{
    _movable = state;
}

void Window::Update()
{
    if (_scalePending) {
        _scalePending = false;
        ApplyScale(true);
    }
    if (IsReady()) {
        if (Window::BeginFrame()) Draw();
        Window::EndFrame();
    }
}

void Window::EnableMenuBar(bool state)
{
    if (state)
        _imWndFlags |= ImGuiWindowMenuBarMask;
    else
        _imWndFlags &= ~ImGuiWindowMenuBarMask;
}

void Window::EnableTitleBar(bool state)
{
    if (state)
        _imWndFlags &= ~ImGuiWindowNoTitleBarMask;
    else
        _imWndFlags |= ImGuiWindowNoTitleBarMask;
}

HWND Window::Native()
{
    return _hwnd;
}

bool Window::IsReady()
{
    return _hwnd && _d3dDevice;
}

bool Window::BeginFrame()
{
    _inFrame = true;
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplayFramebufferScale = ImVec2(_scale, _scale);
    if (_scale != 1.f) {
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
    if (!isShown) _wantQuit = true;

    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (ctx->CurrentWindow && ctx->MovingWindow == ctx->CurrentWindow)
        StartMove();
    else
        StopMove();

    if (!isShown) PostMessageW(_hwnd, WM_QUIT, NULL, NULL);
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

    DWORD dwStyle = WS_POPUP | WS_THICKFRAME;
    _hwnd = CreateWindowExW(0, wc.lpszClassName, _wndName, dwStyle, _position.x, _position.y, _size.x, _size.y, NULL,
                            NULL, wc.hInstance, this);
    if (!_hwnd) {
        CleanupWnd();
        return false;
    }
    if (_icon) {
        SendMessageW(_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)_icon);
        SendMessageW(_hwnd, WM_SETICON, ICON_BIG, (LPARAM)_icon);
    }

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
        UnregisterClassW((LPCWSTR)(_atom & 0xFFFF), NULL);
        _atom = NULL;
    }
}

bool Window::CreateDevice()
{
    _d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!_d3d) return false;

    HRESULT hRes = _d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, _hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                      &_d3dParams, &_d3dDevice);
    if (hRes < 0) {
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
    D3DCOLOR clearCol = D3DCOLOR_RGBA(10, 13, 19, 255);
    _d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clearCol, 1.0f, 0);
    if (_d3dDevice->BeginScene() >= 0) {
        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        if (_scale != 1.f) {
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
    HRESULT result = _d3dDevice->Present(NULL, NULL, NULL, NULL);
    if (result == D3DERR_DEVICELOST && _d3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) ResetDevice();
}

void Window::StartMove()
{
    if (!_movable || _moving) return;
    POINT point;
    GetCursorPos(&point);
    RECT rect;
    GetWindowRect(_hwnd, &rect);
    _movePos = {point.x - rect.left, point.y - rect.top};
    _moving = true;
}

void Window::StopMove()
{
    if (!_moving) return;
    _moving = false;
}

void Window::UpdateMove()
{
    if (!_moving) return;
    if (!_movable) return StopMove();
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
bool Window::HandleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
    *result = 0;
    if (*result = ImGui_ImplWin32_WndProcHandler(_hwnd, msg, wParam, lParam)) return true;
    if (_trayIcon && _trayIcon->HandleMessage(msg, wParam, lParam)) return true;

    switch (msg) {
    case WM_CLOSE: _wantQuit = true; return true;
    case WM_ENDSESSION: _mustQuit = true; return true;
    case WM_SIZING:
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
        RECT* suggested = (RECT*)lParam;
        _position = {(int)suggested->left, (int)suggested->top};
        ApplyScale(false);
        return true;
    }
    case WM_MOUSEMOVE: {
        if (wParam & MK_LBUTTON) UpdateMove();
        return true;
    }
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return true;
        break;
    case WM_NCACTIVATE: *result = DefWindowProcW(_hwnd, msg, wParam, -1); return true;
    case WM_NCCALCSIZE:
        if (wParam && !IsWndMaximized()) return true;
        break;
    case WM_NCHITTEST: *result = HTCLIENT; return true;
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
        self = (Window*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }

    LRESULT result;
    if (self && self->HandleWndProc(msg, wParam, lParam, &result)) return result;
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
