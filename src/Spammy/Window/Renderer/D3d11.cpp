#include "D3d11.h"
#pragma comment(lib, "D3D11")

WindowRenderer_D3d11::WindowRenderer_D3d11()
    : _hwnd(NULL), _dev(NULL), _ctx(NULL), _chain(NULL), _view(NULL)
{}

WindowRenderer_D3d11::~WindowRenderer_D3d11() { cleanup(); }

bool WindowRenderer_D3d11::create(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT hRes = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &_chain, &_dev, &featureLevel, &_ctx);
    if (hRes != S_OK)
        return false;

    createTarget();
    ImGui_ImplDX11_Init(_dev, _ctx);
    _hwnd = hwnd;
    return true;
}

void WindowRenderer_D3d11::createTarget()
{
    ID3D11Texture2D* pBackBuffer;
    _chain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    _dev->CreateRenderTargetView(pBackBuffer, NULL, &_view);
    pBackBuffer->Release();
}

void WindowRenderer_D3d11::cleanup()
{
    if (ImGui::GetIO().BackendRendererUserData) ImGui_ImplDX11_Shutdown();
    cleanupTarget();
    if (_chain) { _chain->Release(); _chain = NULL; }
    if (_ctx) { _ctx->Release(); _ctx = NULL; }
    if (_dev) { _dev->Release(); _dev = NULL; }
}

void WindowRenderer_D3d11::cleanupTarget()
{
    if (_view) { _view->Release(); _view = NULL; }
}

void WindowRenderer_D3d11::reset() {}

void WindowRenderer_D3d11::beginFrame()
{
    ImGui_ImplDX11_NewFrame();
}

void WindowRenderer_D3d11::render()
{
    ImGui::Render();
    const float clear_color_with_alpha[4] = { 0.45f, 0.55f, 0.60f, 1.00f };
    _ctx->OMSetRenderTargets(1, &_view, NULL);
    _ctx->ClearRenderTargetView(_view, clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    _chain->Present(1, 0); // Present with vsync
    //g_pSwapChain->Present(0, 0); // Present without vsync
}

void WindowRenderer_D3d11::setSize(size_t width, size_t height)
{
    cleanupTarget();
    _chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    createTarget();
}

const wchar_t* WindowRenderer_D3d11::getName()
{
    return L"DirectX 11";
}

bool WindowRenderer_D3d11::isCreated()
{
    return !!_dev;
}