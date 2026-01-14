/*
* ファイル名　StartUp
* 説　　　明　エントリーポイント用
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
	/*
	* 関数名　SoftVersion
	* 引　数　なし
	* 戻り値　float：エンジンのバージョン
	* 説　明　エンジンのバージョンを取得する関数
	*/
	__declspec(dllexport) float SoftVersion() {
		return PIXEON_ENGINE_VERSION;
	}

	/*
	* 関数名　SoftInit
	* 引　数　エンジン初期化設定構造体への参照　
	* 戻り値　int：初期化成功なら0、失敗なら-1
	* 説　明　ソフトウェアの初期化を行う関数
	*/
	__declspec(dllexport) int SoftInit(const MainFrame::EngineConfig& config) {
		int nResult = 0;
		nResult = MainFrame::GetInstance()->Init(config);
		screenHeight = config.screenHeight;
		screenWidth = config.screenWidth;
		isRun = true;
		isInit = true;
		return nResult;
	}

	/*
	* 関数名　SoftUpdate
	* 引　数　ウインドウハンドル
	* 戻り値　なし
	* 説　明　ソフトウェアの更新処理を行う関数
	*/
	__declspec(dllexport) void SoftUpdate(HWND hwnd) {
		MainFrame::GetInstance()->Update();
	}

	/*
	* 関数名　SoftDraw
	* 引　数　なし
	* 戻り値　なし
	* 説　明　ソフトウェアの描画処理を行う関数
	*/
	__declspec(dllexport) void SoftDraw() {
		MainFrame::GetInstance()->Draw();
	}

	/*
	* 関数名　SoftShutDown
	* 引　数　なし
	* 戻り値　なし
	* 説　明　ソフトウェアの終了処理を行う関数
	*/
	__declspec(dllexport) void SoftShutDown() {
		MainFrame::GetInstance()->UnInit();
		MainFrame::DeleteInstance();
	}

	/*
	* 関数名　IsEngineRunning
	* 引　数　なし
	* 戻り値　bool：エンジンが動作中ならtrue、停止中ならfalse
	* 説　明　エンジンが動作中かどうかを取得する関数　
	*/
	__declspec(dllexport) bool IsEngineRunning() {
		return isRun;
	}

	/*
	* 関数名　EngineProc
	* 引　数　HWND wnd：ウインドウハンドル　UINT uint：ウインドウメッセージ　WPARAM wparam：ウインドウメッセージパラメータ1　LPARAM lparam：ウインドウメッセージパラメータ2
	* 戻り値　なし
	* 説　明　エンジンのウインドウプロシージャ処理を行う関数
	*/
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

/*
* 関数名　SetRun
* 引　数　bool run：エンジンの動作状態
* 戻り値　なし
* 説　明　エンジンの動作状態を設定する関数
*/
void SetRun(bool run) {
	isRun = run;
}

/*
* 関数名　GetNowWindowSizeWidth
* 引　数　なし
* 戻り値　int：現在のウインドウの幅
* 説　明　現在のウインドウの幅を取得する関数
*/
int StartUp::GetNowWindowSizeWidth()
{
	return screenWidth;
}

/*
* 関数名　GetNowWindowSizeHeight
* 引　数　なし
* 戻り値　int：現在のウインドウの高さ
* 説　明　現在のウインドウの高さを取得する関数
*/
int StartUp::GetNowWindowSizeHeight()
{
	return screenHeight;
}