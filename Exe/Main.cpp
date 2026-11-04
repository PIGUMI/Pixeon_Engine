/*
* ファイル名　Main (Exe)
* 説　　　明　エンジン DLL を読み込んで起動するランチャー
*
*   exe と同じフォルダに game.json がある → ビルド済みゲームとして起動 (Game モード)
*   無い                                  → エディタとして起動 (Editor モード)
*
* コマンドライン
*   --editor            game.json があってもエディタで起動する
*   --backend <name>    描画バックエンドを上書き (dx11 / dx12 / vulkan)
*   --windowed / --fullscreen
*/
#include <Windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include "../Common/EngineApi.h"
#include "../Common/GameManifest.h"

namespace {
	constexpr const char* WINDOW_CLASS_NAME = "PixeonEngine";
	constexpr const char* EDITOR_TITLE = "Pixeon";

	struct EngineFunctions {
		PixeonSoftInitFunc        init = nullptr;
		PixeonSoftUpdateFunc      update = nullptr;
		PixeonSoftDrawFunc        draw = nullptr;
		PixeonSoftShutDownFunc    shutdown = nullptr;
		PixeonIsEngineRunningFunc isRunning = nullptr;
		PixeonEngineProcFunc      proc = nullptr;
	};

	struct LaunchOptions {
		bool b_ForceEditor = false;
		bool b_HasBackend = false;
		GraphicsBackend backend = GraphicsBackend::D3D11;
		int  i_Fullscreen = -1;   // -1: 指定なし 0: ウィンドウ 1: フルスクリーン
	};

	HMODULE         g_cls_EngineDll = nullptr;
	EngineFunctions g_str_Engine;

	/*
	* 関数名　ExeDirectoryGet
	* 引　数　なし
	* 戻り値　exe のあるディレクトリ（末尾 '\' 付き）
	* 説　明　作業ディレクトリに依存せず game.json / data.pak を探すために使う
	*/
	std::string ExeDirectoryGet()
	{
		char path[MAX_PATH]{};
		GetModuleFileNameA(nullptr, path, MAX_PATH);
		std::string dir = path;
		const size_t pos = dir.find_last_of("\\/");
		return (pos == std::string::npos) ? std::string() : dir.substr(0, pos + 1);
	}

	/*
	* 関数名　OptionsParse
	* 引　数　なし
	* 戻り値　起動オプション
	* 説　明　コマンドラインを解釈する
	*/
	LaunchOptions OptionsParse()
	{
		LaunchOptions opt;
		int argc = 0;
		LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
		for (int i = 1; argv && i < argc; ++i) {
			const std::wstring a = argv[i];
			if (a == L"--editor") opt.b_ForceEditor = true;
			else if (a == L"--windowed") opt.i_Fullscreen = 0;
			else if (a == L"--fullscreen") opt.i_Fullscreen = 1;
			else if (a == L"--backend" && i + 1 < argc) {
				const std::wstring w = argv[++i];
				opt.b_HasBackend = GraphicsBackendUtil::NameParse(std::string(w.begin(), w.end()), opt.backend);
			}
		}
		if (argv) LocalFree(argv);
		return opt;
	}

	/*
	* 関数名　EngineLoad
	* 引　数　なし
	* 戻り値　成功なら true
	* 説　明　エンジン DLL を読み込み、エクスポート関数を取得する
	*/
	bool EngineLoad()
	{
		g_cls_EngineDll = LoadLibraryA("Pixeon_Engine.dll");
		if (!g_cls_EngineDll) {
			char msg[256];
			sprintf_s(msg, "Pixeon_Engine.dll を読み込めません\nError code: %lu", GetLastError());
			MessageBoxA(nullptr, msg, "Error", MB_OK | MB_ICONERROR);
			return false;
		}
		auto& e = g_str_Engine;
		e.init = (PixeonSoftInitFunc)GetProcAddress(g_cls_EngineDll, "SoftInit");
		e.update = (PixeonSoftUpdateFunc)GetProcAddress(g_cls_EngineDll, "SoftUpdate");
		e.draw = (PixeonSoftDrawFunc)GetProcAddress(g_cls_EngineDll, "SoftDraw");
		e.shutdown = (PixeonSoftShutDownFunc)GetProcAddress(g_cls_EngineDll, "SoftShutDown");
		e.isRunning = (PixeonIsEngineRunningFunc)GetProcAddress(g_cls_EngineDll, "IsEngineRunning");
		e.proc = (PixeonEngineProcFunc)GetProcAddress(g_cls_EngineDll, "EngineProc");
		if (!e.init || !e.update || !e.draw || !e.shutdown || !e.isRunning || !e.proc) {
			MessageBoxA(nullptr, "エンジン DLL の関数が見つかりません（DLL のバージョンを確認してください）", "Error", MB_OK | MB_ICONERROR);
			FreeLibrary(g_cls_EngineDll);
			g_cls_EngineDll = nullptr;
			return false;
		}
		return true;
	}

	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (g_str_Engine.proc) {
			const intptr_t r = g_str_Engine.proc(hWnd, message, wParam, lParam);
			if (r != 0) return r;  // エンジン (ImGui) が処理した
		}
		if (message == WM_DESTROY) {
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	/*
	* 関数名　WindowCreate
	* 引　数　instance：インスタンス　config：エンジン設定（サイズ・タイトル）
	* 戻り値　ウィンドウハンドル（失敗時 nullptr）
	* 説　明　クライアント領域が指定サイズになるようにウィンドウを作成する
	*/
	HWND WindowCreate(HINSTANCE instance, const EngineConfig& config)
	{
		WNDCLASSEX wcex = {};
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.hInstance = instance;
		wcex.lpszClassName = WINDOW_CLASS_NAME;
		wcex.lpfnWndProc = WndProc;
		wcex.style = CS_CLASSDC | CS_DBLCLKS;
		wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
		wcex.hIconSm = wcex.hIcon;
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		if (!RegisterClassEx(&wcex)) return nullptr;

		const DWORD style = config.fullscreen ? WS_POPUP : WS_OVERLAPPEDWINDOW;
		RECT rect = { 0, 0, config.screenWidth, config.screenHeight };
		AdjustWindowRectEx(&rect, style, FALSE, 0);
		return CreateWindowEx(0, WINDOW_CLASS_NAME, config.windowTitle, style,
			config.fullscreen ? 0 : CW_USEDEFAULT, config.fullscreen ? 0 : CW_USEDEFAULT,
			rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, nullptr);
	}

	/*
	* 関数名　MainLoop
	* 引　数　hwnd：ウィンドウ
	* 戻り値　なし
	* 説　明　メッセージを全て処理してから 1 フレーム進める
	*/
	void MainLoop(HWND hwnd)
	{
		MSG msg = {};
		while (g_str_Engine.isRunning()) {
			bool quit = false;
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				if (msg.message == WM_QUIT) { quit = true; break; }
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			if (quit) break;
			g_str_Engine.update(hwnd);
			g_str_Engine.draw();
		}
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	const LaunchOptions opt = OptionsParse();
	const std::string exe_dir = ExeDirectoryGet();

	// 起動設定の決定
	GameManifest manifest;
	std::string manifest_error;
	const bool game_mode = !opt.b_ForceEditor && manifest.FileLoad(exe_dir + "game.json", &manifest_error);
	if (!game_mode && !manifest_error.empty() && GetFileAttributesA((exe_dir + "game.json").c_str()) != INVALID_FILE_ATTRIBUTES) {
		MessageBoxA(nullptr, ("game.json を読み込めません\n" + manifest_error).c_str(), "Error", MB_OK | MB_ICONERROR);
		return -1;
	}
	if (game_mode) SetCurrentDirectoryA(exe_dir.c_str());

	const std::string archive_path = exe_dir + manifest.archive;
	EngineConfig config;
	config.runMode = game_mode ? EngineRunMode::Game : EngineRunMode::Editor;
	config.windowTitle = game_mode ? manifest.name.c_str() : EDITOR_TITLE;
	config.screenWidth = game_mode ? manifest.i_Width : GetSystemMetrics(SM_CXSCREEN);
	config.screenHeight = game_mode ? manifest.i_Height : GetSystemMetrics(SM_CYSCREEN);
	config.fullscreen = game_mode ? manifest.b_Fullscreen : false;
	config.vsync = game_mode ? manifest.b_VSync : true;
	config.backend = game_mode ? manifest.backend : GraphicsBackend::D3D11;
	config.startScene = game_mode ? manifest.startScene.c_str() : nullptr;
	config.archivePath = game_mode ? archive_path.c_str() : nullptr;
	if (opt.b_HasBackend) config.backend = opt.backend;
	if (opt.i_Fullscreen >= 0) config.fullscreen = (opt.i_Fullscreen == 1);

	if (!EngineLoad()) return -1;
	HWND hwnd = WindowCreate(hInstance, config);
	if (!hwnd) {
		MessageBoxA(nullptr, "ウィンドウを作成できません", "Error", MB_OK | MB_ICONERROR);
		FreeLibrary(g_cls_EngineDll);
		return -1;
	}
	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	RECT client{};
	GetClientRect(hwnd, &client);
	config.wnd = hwnd;
	config.screenWidth = client.right - client.left;
	config.screenHeight = client.bottom - client.top;
	if (g_str_Engine.init(config) != 0) {
		MessageBoxA(nullptr, "エンジンの初期化に失敗しました", "Error", MB_OK | MB_ICONERROR);
		DestroyWindow(hwnd);
		UnregisterClass(WINDOW_CLASS_NAME, hInstance);
		FreeLibrary(g_cls_EngineDll);
		return -1;
	}

	MainLoop(hwnd);

	g_str_Engine.shutdown();
	DestroyWindow(hwnd);
	UnregisterClass(WINDOW_CLASS_NAME, hInstance);
	FreeLibrary(g_cls_EngineDll);
	return 0;
}
