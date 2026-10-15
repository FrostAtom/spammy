#include "Window/Window.h"
#include "ImGui.h"

static constexpr std::array<const char*, Window::ErrorCode_COUNT> kErrorCodeNames = {
    "OK", "Invalid call", "Windows error", "Renderer error", "ImGui error",
};

static constexpr float kSwipeDistance = 56.f;
static constexpr float kSwipeInSec = 0.22f;
static constexpr float kSwipeOutSec = 0.16f;
static constexpr float kMaxSwipeStepSec = 1.f / 30.f;

static float EaseOutCubic(float t)
{
    const float left = 1.f - t;
    return 1.f - left * left * left;
}

static float EaseInQuad(float t)
{
    return t * t;
}

static void ConvertInputToLogicalUnits(ImGuiIO& io, ImVector<ImGuiInputEvent>& inputEvents, float scale)
{
    io.DisplaySize /= scale;
    for (ImGuiInputEvent& event : inputEvents)
        if (event.Type == ImGuiInputEventType_MousePos && event.MousePos.PosX != -FLT_MAX) {
            event.MousePos.PosX /= scale;
            event.MousePos.PosY /= scale;
        }
}

static void ConvertDrawDataToPhysicalPixels(ImDrawData& drawData, float scale)
{
    drawData.DisplaySize *= scale;
    drawData.FramebufferScale = ImVec2(1.f, 1.f);
    for (ImDrawList* cmdList : drawData.CmdLists) {
        for (ImDrawVert& vertex : cmdList->VtxBuffer)
            vertex.pos *= scale;
        for (ImDrawCmd& cmd : cmdList->CmdBuffer)
            cmd.ClipRect *= scale;
    }
}

const char* Window::FormatError(ErrorCode code)
{
    return kErrorCodeNames[code];
}

Window::Window(const wchar_t* className, const wchar_t* wndName)
{
    wcsncpy_s(_className, className, std::size(_className));
    if (!wndName) wndName = className;
    wcsncpy_s(_wndName, wndName, std::size(_wndName));
    _u8wndName[0] = '\0';
    WideCharToMultiByte(CP_UTF8, 0, wndName, -1, _u8wndName, (int)std::size(_u8wndName), nullptr, nullptr);

    _d3dParams = {
        .BackBufferFormat = D3DFMT_UNKNOWN,
        .SwapEffect = D3DSWAPEFFECT_DISCARD,
        .Windowed = TRUE,
        .EnableAutoDepthStencil = FALSE,
        .PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE,
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

    const auto rollBack = [this](ErrorCode code) {
        Cleanup();
        return code;
    };
    if (!CreateWnd()) return rollBack(ErrorCode_WinError);
    if (!CreateDevice()) return rollBack(ErrorCode_RendererError);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigWindowsResizeFromEdges = false;
    io.ConfigInputTrickleEventQueue = false;
    io.IniFilename = nullptr;

    ImGui::LoadUiFonts();

    if (_trayIcon) _trayIcon->Attach(_hwnd, TrayIconImage());

    return ErrorCode_OK;
}

void Window::SetTrayIcon(std::unique_ptr<TrayIcon> icon)
{
    _trayIcon = std::move(icon);
    if (_hwnd && _trayIcon) _trayIcon->Attach(_hwnd, TrayIconImage());
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
        for (const std::unique_ptr<ImTextureData>& texture : _textures)
            ImGui::UnregisterUserTexture(texture.get());
        _textures.clear();
        ImGui::DestroyContext(_imCtx);
        _imCtx = nullptr;
    }
}

ImTextureRef Window::CreateTexture(int width, int height, std::span<const ImU32> pixels)
{
    IM_ASSERT(pixels.size() == (size_t)width * height);
    ImTextureData& texture = *_textures.emplace_back(std::make_unique<ImTextureData>());
    texture.Create(ImTextureFormat_RGBA32, width, height);
    texture.UseColors = true;
    std::ranges::copy(pixels, (ImU32*)texture.GetPixels());
    ImGui::RegisterUserTexture(&texture);
    return texture.GetTexRef();
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

bool Window::IsOnScreen() const
{
    return _hwnd && IsWindowVisible(_hwnd) && !IsIconic(_hwnd);
}

void Window::Show()
{
    const bool exiting = _quitAfterSwipe;
    if (!_hwnd || exiting) return;
    const bool systemAnimatesRestore = IsIconic(_hwnd);
    if (IsHiddenOrHiding() && !systemAnimatesRestore) StartSwipe(Swipe_In);
    ShowWindow(_hwnd, SW_SHOWNORMAL);
}

void Window::Hide()
{
    if (!_hwnd || _swipe == Swipe_Out) return;
    if (IsOnScreen())
        StartSwipe(Swipe_Out);
    else
        ShowWindow(_hwnd, SW_HIDE);
}

void Window::Close()
{
    if (!IsOnScreen()) {
        _mustQuit = true;
        return;
    }
    _quitAfterSwipe = true;
    if (_swipe != Swipe_Out) StartSwipe(Swipe_Out);
}

bool Window::IsHiddenOrHiding() const
{
    return !_hwnd || !IsWindowVisible(_hwnd) || _swipe == Swipe_Out;
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
        _swipeRestX = rect.left;
        _swipeOffset = swipe == Swipe_In ? -kSwipeDistance : 0.f;
        _swipeAlpha = swipe == Swipe_In ? 0.f : 1.f;
        SetLayered(true); // invisible until ApplySwipeFrame sets its alpha
    }
    _swipe = swipe;
    _swipeProgress = 0.f;
    _swipeStartOffset = _swipeOffset;
    _swipeStartAlpha = _swipeAlpha;
    ApplySwipeFrame();
}

void Window::StepSwipe(float dt)
{
    if (_swipe == Swipe_None) return;
    const bool in = _swipe == Swipe_In;
    _swipeProgress = ImMin(_swipeProgress + dt / (in ? kSwipeInSec : kSwipeOutSec), 1.f);
    const float eased = in ? EaseOutCubic(_swipeProgress) : EaseInQuad(_swipeProgress);
    _swipeOffset = ImLerp(_swipeStartOffset, in ? 0.f : kSwipeDistance, eased);
    _swipeAlpha = ImLerp(_swipeStartAlpha, in ? 1.f : 0.f, in ? eased : _swipeProgress);
    ApplySwipeFrame();
    if (_swipeProgress >= 1.f) FinishSwipe();
}

void Window::ApplySwipeFrame()
{
    SetLayeredWindowAttributes(_hwnd, 0, (BYTE)lroundf(_swipeAlpha * 255.f), LWA_ALPHA);
    if (!_dragging)
        SetWindowPos(_hwnd, nullptr, _swipeRestX + (int)lroundf(_swipeOffset * _scale), _position.y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Window::FinishSwipe()
{
    // a WM_DPICHANGED sent from inside ApplySwipeFrame may have finished it already
    const Swipe finished = std::exchange(_swipe, Swipe_None);
    if (finished == Swipe_None) return;
    if (finished == Swipe_Out) {
        if (std::exchange(_quitAfterSwipe, false)) {
            _mustQuit = true;
            return;
        }
        ShowWindow(_hwnd, SW_HIDE);
        SetWindowPos(_hwnd, nullptr, _swipeRestX, _position.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    SetLayered(false);
}

void Window::JumpSwipeToEnd()
{
    if (_swipe != Swipe_None) StepSwipe(1.f);
}

void Window::MoveToStoredRect()
{
    if (!_hwnd) return;
    JumpSwipeToEnd();
    const Vec2i scaled = ScaledSize();
    MoveWindow(_hwnd, _position.x, _position.y, scaled.x, scaled.y, FALSE);
}

void Window::SetSize(const Vec2i& size)
{
    _size = size;
    RequestSizeAndScale();
}

void Window::CenterOnWorkArea()
{
    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const Vec2i size = ScaledSize();
    _position = {work.left + (work.right - work.left - size.x) / 2, work.top + (work.bottom - work.top - size.y) / 2};
    MoveToStoredRect();
}

Window::Vec2i Window::ScaledSize() const
{
    return {(int)lroundf(_size.x * _scale), (int)lroundf(_size.y * _scale)};
}

void Window::SetScaleFactor(float factor)
{
    _scaleFactor = factor;
    RequestSizeAndScale();
}

void Window::RequestSizeAndScale()
{
    // resizing mid-frame would desync ImGui's display size from the back buffer
    if (_inFrame)
        _sizeAndScalePending = true;
    else
        ApplySizeAndScale(ScaleAnchor_Center);
}

void Window::ApplySizeAndScale(ScaleAnchor anchor)
{
    if (_hwnd) JumpSwipeToEnd();
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
    const Vec2i size = ScaledSize();
    Vec2i pos = _position;
    if (anchor == ScaleAnchor_Center) {
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
    if (std::exchange(_sizeAndScalePending, false)) ApplySizeAndScale(ScaleAnchor_Center);
    if (!IsReady()) return;
    if (BeginFrame()) Draw();
    EndFrame();
    StepSwipe(ImMin(ImGui::GetIO().DeltaTime, kMaxSwipeStepSec));
}

bool Window::BeginFrame()
{
    _inFrame = true;
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplayFramebufferScale = ImVec2(_scale, _scale);
    if (_scale != 1.f) ConvertInputToLogicalUnits(io, _imCtx->InputEventsQueue, _scale);
    ImGui::NewFrame();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    const bool visible = ImGui::Begin(_u8wndName, nullptr, flags);

    if (_imCtx->CurrentWindow && _imCtx->MovingWindow == _imCtx->CurrentWindow)
        StartDrag();
    else
        StopDrag();
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

    const MARGINS dropShadowFrame = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(_hwnd, &dropShadowFrame);

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

    static constexpr DWORD kVertexProcessingFallbacks[] = {
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        D3DCREATE_MIXED_VERTEXPROCESSING,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
    };
    HRESULT hRes = E_FAIL;
    for (const DWORD flags : kVertexProcessingFallbacks)
        if (SUCCEEDED(
                hRes = _d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, _hwnd, flags, &_d3dParams, &_d3dDevice)))
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

bool Window::ResetDevice()
{
    const bool resetAllowed = _d3dDevice->TestCooperativeLevel() != D3DERR_DEVICELOST;
    if (!resetAllowed) return false;
    ImGui_ImplDX9_InvalidateDeviceObjects();
    if (FAILED(_d3dDevice->Reset(&_d3dParams))) return false;
    ImGui_ImplDX9_CreateDeviceObjects();
    _needsReset = false;
    return true;
}

void Window::Render()
{
    if (_needsReset && !ResetDevice()) return;
    const ImVec4& background = _imCtx->Style.Colors[ImGuiCol_WindowBg];
    _d3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_COLORVALUE(background.x, background.y, background.z, 1.f),
                      1.0f, 0);
    if (_d3dDevice->BeginScene() >= 0) {
        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        if (_scale != 1.f) ConvertDrawDataToPhysicalPixels(*drawData, _scale);
        ImGui_ImplDX9_RenderDrawData(drawData);
        _d3dDevice->EndScene();
    }
    if (_d3dDevice->Present(nullptr, nullptr, nullptr, nullptr) == D3DERR_DEVICELOST) _needsReset = true;
}

void Window::StartDrag()
{
    if (_dragging) return;
    POINT cursor;
    GetCursorPos(&cursor);
    RECT rect;
    GetWindowRect(_hwnd, &rect);
    _dragOffset = {cursor.x - rect.left, cursor.y - rect.top};
    _dragging = true;
}

void Window::UpdateDrag()
{
    if (!_dragging) return;
    POINT cursor;
    GetCursorPos(&cursor);
    RECT rect;
    GetWindowRect(_hwnd, &rect);
    MoveWindow(_hwnd, cursor.x - _dragOffset.x, cursor.y - _dragOffset.y, rect.right - rect.left,
               rect.bottom - rect.top, TRUE);
}

// imgui_impl_win32.h keeps this declaration under '#if 0'
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool Window::HandleWndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
    const bool ignoresInput = _swipe == Swipe_Out;
    *result = ignoresInput ? 0 : ImGui_ImplWin32_WndProcHandler(_hwnd, msg, wParam, lParam);
    if (*result) return true;
    if (_trayIcon && _trayIcon->HandleMessage(msg, wParam, lParam)) return true;

    switch (msg) {
    case WM_CLOSE: _wantQuit = true; return true;
    case WM_ENDSESSION: _mustQuit = true; return true;
    case WM_SIZING:
        // the modal size loop blocks the app's message loop
        Update();
        *result = TRUE;
        return true;
    case WM_MOVE: _position = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}; return true;
    case WM_SIZE: {
        const UINT width = LOWORD(lParam), height = HIWORD(lParam);
        const bool backBufferFits = width == _d3dParams.BackBufferWidth && height == _d3dParams.BackBufferHeight;
        if (wParam != SIZE_MINIMIZED && _d3dDevice && !backBufferFits) {
            _d3dParams.BackBufferWidth = width;
            _d3dParams.BackBufferHeight = height;
            _needsReset = true;
        }
        return true;
    }
    case WM_DPICHANGED: {
        _dpi = HIWORD(wParam);
        JumpSwipeToEnd(); // first: its last frame's WM_MOVE would overwrite _position
        const RECT* suggested = (const RECT*)lParam;
        _position = {(int)suggested->left, (int)suggested->top};
        ApplySizeAndScale(ScaleAnchor_StoredPosition);
        return true;
    }
    case WM_MOUSEMOVE:
        if (wParam & MK_LBUTTON) UpdateDrag();
        return true;
    case WM_SYSCOMMAND: {
        const bool altOpensSystemMenu = (wParam & 0xfff0) == SC_KEYMENU;
        if (altOpensSystemMenu) return true;
        break;
    }
    case WM_NCACTIVATE: {
        constexpr LPARAM kSkipNonClientRepaint = -1;
        *result = DefWindowProcW(_hwnd, msg, wParam, kSkipNonClientRepaint);
        return true;
    }
    case WM_NCCALCSIZE: {
        constexpr LRESULT kWholeWindowIsClientArea = 0;
        if (!wParam || IsZoomed(_hwnd)) break;
        *result = kWholeWindowIsClientArea;
        return true;
    }
    case WM_NCHITTEST: *result = HTCLIENT; return true;
    }
    return false;
}

LRESULT WINAPI Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // the first message with the create params; HandleWndProc needs _hwnd before CreateWindowExW returns
    if (msg == WM_NCCREATE) {
        Window* self = (Window*)((const CREATESTRUCTW*)lParam)->lpCreateParams;
        self->_hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    }
    Window* self = (Window*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    LRESULT result;
    if (self && self->HandleWndProc(msg, wParam, lParam, &result)) return result;
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
