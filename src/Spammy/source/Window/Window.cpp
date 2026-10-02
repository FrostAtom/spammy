#include "Window/Window.h"
#include "ImGui.h"

static constexpr std::array<const char*, Window::ErrorCode_COUNT> s_errorCodeNames = {
    "OK", "Invalid call", "Windows error", "Renderer error", "ImGui error",
};

static constexpr float s_swipeDistance = 56.f; // logical px the window travels on show / hide
static constexpr float s_swipeInSec = 0.22f;
static constexpr float s_swipeOutSec = 0.16f;

const char* Window::FormatError(ErrorCode code)
{
    return s_errorCodeNames[code];
}

Window::Window(const wchar_t* className, const wchar_t* wndName)
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
    _lastError = 0;

    _imCtx = ImGui::CreateContext();
    if (!_imCtx) return ErrorCode_ImGuiError;

    // Cleanup() also undoes whatever a failed step managed to create
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
    io.IniFilename = nullptr;

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
        _imCtx = nullptr;
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
    if (!_hwnd) return;
    // a minimized window comes back with the system restore animation instead
    if (!IsIconic(_hwnd) && (!IsWindowVisible(_hwnd) || _swipe == Swipe_Out)) StartSwipe(Swipe_In);
    ShowWindow(_hwnd, SW_SHOWNORMAL);
}

void Window::Hide()
{
    if (!_hwnd || _swipe == Swipe_Out) return;
    // Update() doesn't run for a minimized window, so there'd be nothing to drive the swipe
    if (IsIconic(_hwnd) || !IsWindowVisible(_hwnd))
        ShowWindow(_hwnd, SW_HIDE);
    else
        StartSwipe(Swipe_Out);
}

void Window::Close()
{
    if (!_hwnd || IsIconic(_hwnd) || !IsWindowVisible(_hwnd)) {
        _mustQuit = true;
        return;
    }
    _swipeQuit = true;
    if (_swipe != Swipe_Out) StartSwipe(Swipe_Out);
}

bool Window::IsShown() const
{
    return _hwnd && IsWindowVisible(_hwnd) && _swipe != Swipe_Out;
}

void Window::SetLayered(bool layered)
{
    const LONG_PTR style = GetWindowLongPtrW(_hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(_hwnd, GWL_EXSTYLE, layered ? style | WS_EX_LAYERED : style & ~WS_EX_LAYERED);
}

void Window::StartSwipe(Swipe swipe)
{
    if (_swipe == Swipe_None) {
        RECT rect;
        GetWindowRect(_hwnd, &rect);
        _restX = rect.left;
        // from hidden it comes in from the left, from the screen it leaves from where it stands
        _swipeX = swipe == Swipe_In ? -s_swipeDistance : 0.f;
        _swipeAlpha = swipe == Swipe_In ? 0.f : 1.f;
        SetLayered(true); // the alpha below must follow right away: a layered window without one isn't drawn
    }
    // a swipe that turns around midway continues from wherever the previous one got to
    _swipe = swipe;
    _swipeT = 0.f;
    _swipeFromX = _swipeX;
    _swipeFromAlpha = _swipeAlpha;
    ApplySwipeFrame();
}

void Window::StepSwipe(float dt)
{
    if (_swipe == Swipe_None) return;
    const bool in = _swipe == Swipe_In;
    _swipeT = ImMin(_swipeT + dt / (in ? s_swipeInSec : s_swipeOutSec), 1.f);
    // decelerates into place, accelerates away
    const float left = 1.f - _swipeT;
    const float eased = in ? 1.f - left * left * left : _swipeT * _swipeT;
    _swipeX = ImLerp(_swipeFromX, in ? 0.f : s_swipeDistance, eased);
    _swipeAlpha = ImLerp(_swipeFromAlpha, in ? 1.f : 0.f, in ? eased : _swipeT);
    ApplySwipeFrame();
    if (_swipeT >= 1.f) FinishSwipe();
}

void Window::ApplySwipeFrame()
{
    SetLayeredWindowAttributes(_hwnd, 0, (BYTE)lroundf(_swipeAlpha * 255.f), LWA_ALPHA);
    // the user grabbed the window mid-swipe: it's theirs to place now
    if (!_moving)
        SetWindowPos(_hwnd, nullptr, _restX + (int)lroundf(_swipeX * _scale), _position.y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Window::FinishSwipe()
{
    if (std::exchange(_swipe, Swipe_None) == Swipe_In) {
        SetLayered(false); // a layered window composes slower, keep it only for the fades
        return;
    }
    if (std::exchange(_swipeQuit, false)) {
        _mustQuit = true;
        return;
    }
    ShowWindow(_hwnd, SW_HIDE);
    // back to the rest spot while hidden, so the next swipe in lands where the window was
    SetWindowPos(_hwnd, nullptr, _restX, _position.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    SetLayered(false);
}

void Window::SetName(const wchar_t* name)
{
    wcsncpy_s(_wndName, name, std::size(_wndName));
    _u8wndName[0] = '\0';
    WideCharToMultiByte(CP_UTF8, 0, name, -1, _u8wndName, (int)std::size(_u8wndName), nullptr, nullptr);
    if (_hwnd) SetWindowTextW(_hwnd, name);
}

void Window::MoveToStoredRect()
{
    if (!_hwnd) return;
    const Vec2D<int> scaled = ScaledSize();
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
    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const Vec2D<int> size = ScaledSize();
    SetPosition({(work.right - work.left - size.x) / 2, (work.bottom - work.top - size.y) / 2});
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
    float scale = DpiScale();
    MONITORINFO monitor = {sizeof(monitor)};
    const bool hasMonitor = _hwnd && GetMonitorInfoW(MonitorFromWindow(_hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
    const RECT& work = monitor.rcWork;
    if (hasMonitor && _size.x > 0 && _size.y > 0) {
        const float fitW = (float)(work.right - work.left) / (float)_size.x;
        const float fitH = (float)(work.bottom - work.top) / (float)_size.y;
        scale = ImMin(scale, ImMin(fitW, fitH));
    }
    _scale = ImMax(scale, 0.5f);

    if (_imCtx) {
        _imCtx->Style.CircleTessellationMaxError = 0.3f / _scale;
        _imCtx->Style.CurveTessellationTol = 1.25f / _scale;
        _imCtx->DrawListSharedData.InitialFringeScale = 1.f / _scale;
    }

    if (!_hwnd) return;
    const Vec2D<int> size = ScaledSize();
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

void Window::Update()
{
    if (std::exchange(_scalePending, false)) ApplyScale(true);
    if (!IsReady()) return;
    if (BeginFrame()) Draw();
    EndFrame();
    // capped so a hitch doesn't skip the swipe altogether
    StepSwipe(ImMin(ImGui::GetIO().DeltaTime, 1.f / 30.f));
}

void Window::EnableTitleBar(bool v)
{
    if (v)
        _imWndFlags &= ~ImGuiWindowFlags_NoTitleBar;
    else
        _imWndFlags |= ImGuiWindowFlags_NoTitleBar;
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
        io.DisplaySize /= _scale;
        for (ImGuiInputEvent& event : GImGui->InputEventsQueue)
            if (event.Type == ImGuiInputEventType_MousePos && event.MousePos.PosX != -FLT_MAX) {
                event.MousePos.PosX /= _scale;
                event.MousePos.PosY /= _scale;
            }
    }
    ImGui::NewFrame();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    bool isOpen = true; // cleared by the title bar's close button
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus | _imWndFlags;
    const bool visible = ImGui::Begin(_u8wndName, &isOpen, flags);

    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (ctx->CurrentWindow && ctx->MovingWindow == ctx->CurrentWindow)
        StartMove();
    else
        StopMove();

    if (!isOpen) _wantQuit = true;
    return visible;
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
    const WNDCLASSEXW wc = {
        .cbSize = sizeof(wc),
        .style = CS_CLASSDC,
        .lpfnWndProc = WndProc,
        .hInstance = GetModuleHandleW(nullptr),
        .lpszClassName = _className,
    };
    _atom = RegisterClassExW(&wc);
    if (_atom)
        _hwnd = CreateWindowExW(0, _className, _wndName, WS_POPUP | WS_THICKFRAME, _position.x, _position.y, _size.x,
                                _size.y, nullptr, nullptr, wc.hInstance, this);
    if (!_hwnd) {
        _lastError = HRESULT_FROM_WIN32(GetLastError());
        return false;
    }
    if (_icon) ApplyWndIcon();

    // a 1px DWM frame gives the borderless popup the system drop shadow
    const MARGINS shadowMargins = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(_hwnd, &shadowMargins);

    _dpi = GetDpiForWindow(_hwnd);
    _scale = DpiScale();

    ImGui_ImplWin32_Init(_hwnd);
    return true;
}

void Window::CleanupWnd()
{
    if (_hwnd) {
        ImGui_ImplWin32_Shutdown();
        DestroyWindow(_hwnd);
        _hwnd = nullptr;
    }
    if (_atom) {
        UnregisterClassW(MAKEINTATOM(_atom), nullptr);
        _atom = NULL;
    }
}

bool Window::CreateDevice()
{
    _d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!_d3d) {
        _lastError = HRESULT_FROM_WIN32(GetLastError());
        return false;
    }

    // hardware T&L is missing on basic display adapters, RDP sessions, VMs and some old iGPUs
    static constexpr DWORD s_vertexProcessingFallbacks[] = {
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        D3DCREATE_MIXED_VERTEXPROCESSING,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
    };
    HRESULT hRes = E_FAIL;
    for (const DWORD flags : s_vertexProcessingFallbacks)
        if (SUCCEEDED(hRes = _d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, _hwnd, flags, &_d3dParams,
                                                &_d3dDevice)))
            break;
    if (FAILED(hRes)) {
        _lastError = hRes;
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
        _d3dDevice = nullptr;
    }
    if (_d3d) {
        _d3d->Release();
        _d3d = nullptr;
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
    _d3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(10, 13, 19, 255), 1.0f, 0);
    if (_d3dDevice->BeginScene() >= 0) {
        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        if (_scale != 1.f) {
            // scale the logical-unit draw data back to physical pixels (see BeginFrame)
            drawData->DisplaySize *= _scale;
            drawData->FramebufferScale = ImVec2(1.f, 1.f);
            for (ImDrawList* cmdList : drawData->CmdLists) {
                for (ImDrawVert& vertex : cmdList->VtxBuffer)
                    vertex.pos *= _scale;
                for (ImDrawCmd& cmd : cmdList->CmdBuffer)
                    cmd.ClipRect *= _scale;
            }
        }
        ImGui_ImplDX9_RenderDrawData(drawData);
        _d3dDevice->EndScene();
    }
    if (_d3dDevice->Present(nullptr, nullptr, nullptr, nullptr) == D3DERR_DEVICELOST &&
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
    // a window swiping out is already gone for the user, it must not take clicks meant for what's under it
    *result = _swipe == Swipe_Out ? 0 : ImGui_ImplWin32_WndProcHandler(_hwnd, msg, wParam, lParam);
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

LRESULT WINAPI Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
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
