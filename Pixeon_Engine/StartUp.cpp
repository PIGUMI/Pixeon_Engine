/*
* ファイル名　StartUp
* 説　　　明　エントリーポイント用
*/

#include <windowsx.h>
#include "System.h"
#include "Input.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui.h"
#include "StartUp.h"
#include "MainFrame.h"
#include <imm.h>
#pragma comment(lib, "imm32.lib")

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
	* 説　明　
	*/
    __declspec(dllexport) LRESULT EngineProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        // カスタムIME処理（ImGuiの前に処理）
        switch (msg)
        {
        case WM_IME_SETCONTEXT:
        {
            // IMEの変換ウィンドウを無効化
            lparam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
            return DefWindowProc(wnd, msg, wparam, lparam);
        }

        case WM_IME_COMPOSITION:
        {
            HIMC hIMC = ImmGetContext(wnd);
            if (hIMC)
            {
                if (lparam & GCS_RESULTSTR)
                {
                    // 確定文字列を取得（Unicode）
                    LONG len = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, NULL, 0);
                    if (len > 0)
                    {
                        std::wstring wstr(len / sizeof(wchar_t), 0);
                        ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, &wstr[0], len);

                        // UTF-16からUTF-8に変換
                        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
                        if (utf8Len > 0)
                        {
                            std::string utf8str(utf8Len - 1, 0);
                            WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8str[0], utf8Len, NULL, NULL);

                            // ImGuiの入力バッファに追加
                            ImGuiIO& io = ImGui::GetIO();
                            io.AddInputCharactersUTF8(utf8str.c_str());
                        }
                    }
                }
                ImmReleaseContext(wnd, hIMC);
            }
            // WM_IME_COMPOSITIONを処理したのでImGuiには渡さない
            return 0;
        }

        case WM_CHAR:
        {
            // IME経由の文字は無視（WM_IME_COMPOSITIONで処理済み）
            // 0x80以上の文字コードはIME経由なので無視
            if (wparam >= 0x80)
            {
                return 0;
            }
            // 半角英数のみImGuiに渡す
            break;
        }

        case WM_SYSCHAR:
            // システム文字も無視
            return 0;
        }

        // ImGuiのWndProcHandler（半角英数などの処理）
        if (ImGui_ImplWin32_WndProcHandler(wnd, msg, wparam, lparam))
            return 1;

        // その他のメッセージ処理
        switch (msg)
        {
        case WM_MOUSEWHEEL:
            OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wparam));
            return 0;

        case WM_MOUSEHWHEEL:
            OnMouseHWheel(GET_WHEEL_DELTA_WPARAM(wparam));
            return 0;

        case WM_SIZE:
            if (isInit && wparam != SIZE_MINIMIZED) {
                UINT width = LOWORD(lparam);
                UINT height = HIWORD(lparam);
                screenHeight = height;
                screenWidth = width;
                DirectX11::GetInstance()->OnResize(width, height);
            }
            return 0;
        }

        return 0; // 未処理。exe側で DefWindowProc に回す
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