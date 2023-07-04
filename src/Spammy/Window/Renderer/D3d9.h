#pragma once
#include "../../Headers.h"
#include "Renderer.h"

class WindowRenderer_D3d9 : public WindowRenderer {
    HWND _hwnd;
    LPDIRECT3D9 _lib;
    LPDIRECT3DDEVICE9 _dev;
    D3DPRESENT_PARAMETERS _params;

public:
    WindowRenderer_D3d9();
    ~WindowRenderer_D3d9();
    
    bool create(HWND hwnd);
    void cleanup();
    void reset();
    void beginFrame();
    void render();
    void setSize(size_t width, size_t height);
    const wchar_t* getName();
    bool isCreated();
};