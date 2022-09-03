#pragma once
#include "Renderer.h"
#include <d3d11.h>
#include <imgui/imgui_impl_dx11.h>

class WindowRenderer_D3d11 : public WindowRenderer {
    HWND _hwnd;
    ID3D11Device* _dev;
    ID3D11DeviceContext* _ctx;
    IDXGISwapChain* _chain;
    ID3D11RenderTargetView* _view;

public:
    WindowRenderer_D3d11();
    ~WindowRenderer_D3d11();

    bool create(HWND hwnd);
    void cleanup();
    void reset();
    void beginFrame();
    void render();
    void setSize(size_t width, size_t height);
    const wchar_t* getName();
    bool isCreated();

private:
    void createTarget();
    void cleanupTarget();
};