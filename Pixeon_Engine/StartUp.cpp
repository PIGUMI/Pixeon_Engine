/* StartUp */
/*
* DLLのエントリーポイント
*/

/* コーディング基底 */
/*
* 2025/12/13 再定義
* 　変数名定義
* 　　型名：int i_、float f_、bool b_、std::string str_、クラス：cls_
* 　　メンバー変数：g_（グローバル）m_（メンバー）
* 　　ローカル変数：なし
* 　関数名定義
* 　　動詞＋名詞、キャメルケース
* 　ファイル名定義
*/

#include <windowsx.h>
#include "System.h"
#include "Input.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "StartUp.h"
#include "MainFrame.h"

int g_iScreenWidth = 1920;
int g_iScreenHeight = 1080;
bool g_bInit = false;
bool g_bRun = false;

extern "C" {
	// versionを取得
	__declspec(dllexport) float SoftVersion() {
		return 230.0f;
	}

	__declspec(dllexport) int SoftInit(const MainFrame::EngineConfig& config) {
		int nResult = 0;
		nResult = MainFrame::GetInstance()->Init(config);
		g_iScreenHeight = config.screenHeight;
		g_iScreenWidth = config.screenWidth;
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
				g_iScreenHeight = height;
				g_iScreenWidth = width;
				DirectX11::GetInstance()->OnResize(width, height);
			}
			break;
		}
	}
}

void SetRun(bool run) {
	g_bRun = run;
}

int StartUp::GetNowWindowSizeX()
{
	return g_iScreenWidth;
}

int StartUp::GetNowWindowSizeY()
{
	return g_iScreenWidth;
}
