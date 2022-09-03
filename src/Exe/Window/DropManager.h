#pragma once
#include <oleidl.h>
#include <Shellapi.h>
#include <filesystem>
#include <functional>
#include <vector>

class DropManager : public IDropTarget {
	friend class Window;
	using Callback_t = std::function<void(std::vector<std::filesystem::path>&&)>;
	std::vector<std::filesystem::path> _storage;
	Callback_t _callback;

public:
	DropManager();
	~DropManager();
	void InitWindow(HWND hwnd);
	void UnInitWindow(HWND hwnd);
	void SetCallback(Callback_t&& func);
	void Poll();

	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
	HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
	HRESULT STDMETHODCALLTYPE DragLeave();
	HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
	HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
};
