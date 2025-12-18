/* StartUp */
/*
* DLLのエントリーポイント
*/

#include <windowsx.h>
#include "System.h"
#include "Input.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "StartUp.h"
#include "MainFrame.h"

int screenWidth = 1920;
int screenHeight = 1080;
bool isInit = false;
bool isRun = false;

extern "C" {
	// versionを取得
	__declspec(dllexport) float SoftVersion() {
		return 0.0f;
	}

	__declspec(dllexport) int SoftInit(const MainFrame::EngineConfig& config) {
		int nResult = 0;
		nResult = MainFrame::GetInstance()->Init(config);
		screenHeight = config.screenHeight;
		screenWidth = config.screenWidth;
		isRun = true;
		isInit = true;
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
		return isRun;
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
			if (isInit && wparam != SIZE_MINIMIZED) {
				UINT width = LOWORD(lparam);
				UINT height = HIWORD(lparam);
				screenHeight = height;
				screenWidth = width;
				DirectX11::GetInstance()->OnResize(width, height);
			}
			break;
		}
	}
}

void SetRun(bool run) {
	isRun = run;
}

int StartUp::GetNowWindowSizeWidth()
{
	return screenWidth;
}

int StartUp::GetNowWindowSizeHeight()
{
	return screenHeight;
}