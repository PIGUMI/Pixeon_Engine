/*
* ファイル名　EngineApi
* 説　　　明　エンジン DLL (Pixeon_Engine.dll) とランタイム / エディタ起動 Exe の間の公開インターフェース
*             以前は Exe 側で構造体を手書きで複製しており、定義のずれ（SoftUpdate の引数など）があった。
*             両者がこのヘッダを共有することでずれを防ぐ
*/
#ifndef ENGINE_API_H
#define ENGINE_API_H

#include "GameManifest.h"
#include <cstdint>

// 起動モード
enum class EngineRunMode : int32_t {
	Editor = 0,   // エディタ UI 付きで起動（SceneRoot のディレクトリを直接読む）
	Game = 1,     // ビルド済みゲームとして起動（アーカイブを読み、エディタ UI を出さない）
};

// エンジン初期化設定
struct EngineConfig {
	uint32_t        structSize = sizeof(EngineConfig);   // ABI 確認用
	void* wnd = nullptr;                                 // HWND
	int             screenWidth = 1920;
	int             screenHeight = 1080;
	const char* windowTitle = "Pixeon";
	bool            fullscreen = false;
	float           targetFPS = 60.0f;
	const char* startScene = nullptr;                    // nullptr ならシーン設定ファイルに従う
	EngineRunMode   runMode = EngineRunMode::Editor;
	GraphicsBackend backend = GraphicsBackend::D3D11;
	const char* archivePath = nullptr;                   // Game モードでマウントするアーカイブ
	bool            vsync = true;
};

// DLL がエクスポートする関数の型
extern "C" {
	typedef float   (*PixeonSoftVersionFunc)();
	typedef int     (*PixeonSoftInitFunc)(const EngineConfig& config);
	typedef void    (*PixeonSoftUpdateFunc)(void* wnd);
	typedef void    (*PixeonSoftDrawFunc)();
	typedef void    (*PixeonSoftShutDownFunc)();
	typedef bool    (*PixeonIsEngineRunningFunc)();
	typedef intptr_t(*PixeonEngineProcFunc)(void* wnd, uint32_t msg, uintptr_t wparam, intptr_t lparam);
}

#endif // !ENGINE_API_H
