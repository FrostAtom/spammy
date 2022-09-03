

/*

LRESULT CALLBACK App::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	App& app = App::instance();
	if (nCode >= 0) {
		LPMSLLHOOKSTRUCT data = (LPMSLLHOOKSTRUCT)lParam;
		switch (wParam) {
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_XBUTTONDOWN: {

			break;
		}
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		case WM_XBUTTONUP: {

			break;
		}
		}
	}
	return CallNextHookEx(app._hkMouse, nCode, wParam, lParam);
}

*/