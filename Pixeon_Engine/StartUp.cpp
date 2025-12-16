/*
* Engine StartUp
* §ìÒ: ƒAƒLƒm
*/

#include <windowsx.h>
#include "System.h"
#include "Input.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "StartUp.h"
#include "MainFrame.h"

int g_nScreenWidth = 1920;
int g_nScreenHeight = 1080;
bool g_bInit = false;
bool g_bRun = false;

extern "C" {
	// version‚ğæ“¾
	__declspec(dllexport) float SoftVersion() {
		return 0.0f;
	}

	__declspec(dllexport) int SoftInit(const MainFrame::EngineConfig& config) {
		int nResult = 0;
		nResult = MainFrame::GetInstance()->Init(config);
		g_nScreenHeight = config.screenHeight;
		g_nScreenWidth = config.screenWidth;
		g_bRun = true;
		g_bInit = true;
		return nResult;
	}

	__declspec(dllexport) void SoftUpdate(HWND hwnd) {
		MainFrame::GetInstance()->Update();
	}

	__declspec(dllexport) void SoftDraw() {
		MainFrame::GetInstance()->Draw();
	}

	__declspec(dllexport) void SoftShutDown() {
		MainFrame::GetInstance()->UnInit();
		MainFrame::DeleteInstance();
	}

	__declspec(dllexport) bool IsEngineRunning() {
		return g_bRun;
	}

	__declspec(dllexport) void EngineProc(HWND wnd, UINT uint, WPARAM wparam, LPARAM lparam) {
		ImGui_ImplWin32_WndProcHandler(wnd, uint, wparam, lparam);
		switch (uint) {
		case WM_MOUSEWHEEL:
			OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wparam));
			break;
		case WM_MOUSEHWHEEL:
			OnMouseHWheel(GET_WHEEL_DELTA_WPARAM(wparam));
			break;
		case WM_SIZE:
			if (g_bInit && wparam != SIZE_MINIMIZED) {
				UINT width = LOWORD(lparam);
				UINT height = HIWORD(lparam);
				g_nScreenHeight = height;
				g_nScreenWidth = width;
				DirectX11::GetInstance()->OnResize(width, height);
			}
			break;
		}
	}
}

void SetRun(bool run) {
	g_bRun = run;
}