#include "DropManager.h"

DropManager::DropManager()
{
    OleInitialize(NULL);
}

DropManager::~DropManager()
{
    OleUninitialize();
}

void DropManager::InitWindow(HWND hwnd)
{
    RegisterDragDrop(hwnd, this);
}

void DropManager::UnInitWindow(HWND hwnd)
{
    RevokeDragDrop(hwnd);
    OleUninitialize();
}

void DropManager::SetCallback(Callback_t&& func)
{
    _callback = std::forward<Callback_t>(func);
}

void DropManager::Poll()
{
    if (_storage.empty()) return;
    _callback(std::move(_storage));
    _storage.clear();
}

ULONG DropManager::AddRef() { return 1; }
ULONG DropManager::Release() { return 0; }

HRESULT DropManager::QueryInterface(REFIID riid, void** ppvObject)
{
    if (riid == IID_IDropTarget) {
        *ppvObject = this;
        return S_OK;
    }
    return E_NOINTERFACE;
}

HRESULT DropManager::DragEnter(IDataObject*, DWORD, POINTL, DWORD* pdwEffect)
{
    *pdwEffect &= DROPEFFECT_COPY;
    return S_OK;
}

HRESULT DropManager::DragLeave() { return S_OK; }

HRESULT DropManager::DragOver(DWORD, POINTL, DWORD* pdwEffect)
{
    *pdwEffect &= DROPEFFECT_COPY;
    return S_OK;
}

HRESULT DropManager::Drop(IDataObject* pDataObj, DWORD, POINTL, DWORD*)
{
    FORMATETC fmte = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stgm;
	if (SUCCEEDED(pDataObj->GetData(&fmte, &stgm))) {
		HDROP hdrop = (HDROP)stgm.hGlobal;
		UINT file_count = DragQueryFileW(hdrop, 0xFFFFFFFF, NULL, 0);
		for (UINT i = 0; i < file_count; i++) {
			wchar_t szFile[MAX_PATH];
			UINT cch = DragQueryFileW(hdrop, i, szFile, MAX_PATH);
			if (cch > 0 && cch < MAX_PATH)
				_storage.push_back(szFile);
		}
		ReleaseStgMedium(&stgm);
	}
    return S_OK;
}
