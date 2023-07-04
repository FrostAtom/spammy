#include "D3d9.h"

WindowRenderer_D3d9::WindowRenderer_D3d9()
	: _hwnd(NULL), _lib(NULL), _dev(NULL)
{
	memset(&_params, NULL, sizeof(_params));
	_params.Windowed = TRUE;
	_params.SwapEffect = D3DSWAPEFFECT_DISCARD;
	_params.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
	_params.EnableAutoDepthStencil = TRUE;
	_params.AutoDepthStencilFormat = D3DFMT_D16;
	_params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;           // Present without vsync
}

WindowRenderer_D3d9::~WindowRenderer_D3d9() { cleanup(); }

bool WindowRenderer_D3d9::create(HWND hwnd)
{
    IM_ASSERT(!_hwnd && "Device already created!");
	_lib = Direct3DCreate9(D3D_SDK_VERSION);
	if (!_lib) return false;

	HRESULT hRes = _lib->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &_params, &_dev);
	if (hRes < 0) {
		_lib->Release();
		_lib = NULL;
		return false;
	}
	ImGui_ImplDX9_Init(_dev);
	_hwnd = hwnd;
	return true;
}

void WindowRenderer_D3d9::cleanup()
{
	if (ImGui::GetIO().BackendRendererUserData) ImGui_ImplDX9_Shutdown();
	if (_dev) { _dev->Release(); _dev = NULL; }
	if (_lib) { _lib->Release(); _lib = NULL; }
	_hwnd = NULL;
}

void WindowRenderer_D3d9::reset()
{
	if (!_dev) return;
	ImGui_ImplDX9_InvalidateDeviceObjects();
	_dev->Reset(&_params);
	ImGui_ImplDX9_CreateDeviceObjects();
}

void WindowRenderer_D3d9::beginFrame()
{
	ImGui_ImplDX9_NewFrame();
}

void WindowRenderer_D3d9::render()
{
	_dev->SetRenderState(D3DRS_ZENABLE, FALSE);
	_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	D3DCOLOR clearCol = D3DCOLOR_RGBA(255, 255, 255, 255);
	_dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clearCol, 1.0f, 0);
	if (_dev->BeginScene() >= 0) {
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		_dev->EndScene();
	}
	HRESULT result = _dev->Present(NULL, NULL, NULL, NULL);
	if (result == D3DERR_DEVICELOST && _dev->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		reset();
}

void WindowRenderer_D3d9::setSize(size_t width, size_t height)
{
	_params.BackBufferWidth = width;
	_params.BackBufferHeight = height;
	reset();
}

const wchar_t* WindowRenderer_D3d9::getName()
{
	return L"DirectX 9";
}

bool WindowRenderer_D3d9::isCreated()
{
	return !!_dev;
}