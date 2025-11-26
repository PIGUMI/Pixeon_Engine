// GameLauncher/main.cpp
#include <Windows.h>
#include <iostream>
#include <crtdbg.h>

// Engine.dllからの関数をインポート
struct EngineConfig {
    HWND hWnd;
    int screenWidth;
    int screenHeight;
    const char* windowTitle;
    bool fullscreen;
    float targetFPS;
    const char* startScene;
};

typedef int (*EngineInitFunc)(const EngineConfig& config);
typedef void (*EngineUpdateFunc)();
typedef void (*EngineDrawFunc)();
typedef void (*EngineShutdownFunc)();
typedef bool (*EngineIsRunningFunc)();
typedef void (*EngineProcessWindowMessageFunc)(HWND, UINT, WPARAM, LPARAM);


// グローバル変数
HWND g_hWnd;
HMODULE g_engineDLL = nullptr;
EngineUpdateFunc EngineUpdate;
EngineDrawFunc EngineDraw;
EngineIsRunningFunc EngineIsRunning;
EngineProcessWindowMessageFunc EngineProcessWindowMessage;


#define APP_TITLE "Pixeon"
#define SCREEN_WIDTH (1920)
#define SCREEN_HEIGHT (1080)
// ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // エンジンにメッセージを転送
    if (EngineProcessWindowMessage) {
        EngineProcessWindowMessage(hWnd, message, wParam, lParam);
    }

    const float ASPECT_RATIO = 16.0f / 9.0f;
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_SIZING:
    {
        RECT* rect = (RECT*)lParam;
        int width = rect->right - rect->left;
        int height = rect->bottom - rect->top;

        // アスペクト比調整
        switch (wParam)
        {
        case WMSZ_LEFT:
        case WMSZ_RIGHT:
            height = static_cast<int>(width / ASPECT_RATIO);
            rect->bottom = rect->top + height;
            break;
        case WMSZ_TOP:
        case WMSZ_BOTTOM:
            width = static_cast<int>(height * ASPECT_RATIO);
            rect->right = rect->left + width;
            break;
            // 他のケースも同様...
        }
    }
    break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Engine.dllの読み込み
    g_engineDLL = LoadLibrary("Pixeon_Engine.dll");
    if (!g_engineDLL) {
        DWORD error = GetLastError();
        char msg[256];
        sprintf_s(msg, "Failed to load Pixeon.dll\nError code: %lu", error);
        MessageBoxA(NULL, msg, "Error", MB_OK);
        return -1;
    }

    // エンジン関数の取得
    EngineInitFunc EngineInit = (EngineInitFunc)GetProcAddress(g_engineDLL, "SoftInit");
    EngineUpdate = (EngineUpdateFunc)GetProcAddress(g_engineDLL, "SoftUpdate");
	EngineDraw = (EngineDrawFunc)GetProcAddress(g_engineDLL, "SoftDraw");
    EngineShutdownFunc EngineShutdown = (EngineShutdownFunc)GetProcAddress(g_engineDLL, "SoftShutDown");
    EngineIsRunning = (EngineIsRunningFunc)GetProcAddress(g_engineDLL, "IsEngineRunning");
    EngineProcessWindowMessage = (EngineProcessWindowMessageFunc)GetProcAddress(g_engineDLL, "EngineProc");


    // 関数ポインタのチェック　
    if (!EngineInit)
    {
        MessageBox(NULL, "EngineInit function not found", "Error", MB_OK);
        FreeLibrary(g_engineDLL);
        return -1;
    }
    if (!EngineUpdate)
    {
        MessageBox(NULL, "EngineUpdate function not found", "Error", MB_OK);
        FreeLibrary(g_engineDLL);
        return -1;
    }
    if (!EngineShutdown)
    {
        MessageBox(NULL, "EngineShutdown function not found", "Error", MB_OK);
        FreeLibrary(g_engineDLL);
        return -1;
    }



    // ウィンドウクラスの登録
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.hInstance = hInstance;
    wcex.lpszClassName = "PixeonEngine";
    wcex.lpfnWndProc = WndProc;
    wcex.style = CS_CLASSDC | CS_DBLCLKS;
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hIconSm = wcex.hIcon;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, "Failed to RegisterClassEx", "Error", MB_OK);
        FreeLibrary(g_engineDLL);
        return -1;
    }

	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // ウィンドウの作成
    RECT rect = { 0, 0, screenWidth, screenHeight };
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exStyle = WS_EX_OVERLAPPEDWINDOW;
    AdjustWindowRectEx(&rect, style, false, exStyle);

    g_hWnd = CreateWindowEx(
        exStyle, wcex.lpszClassName,
        APP_TITLE, style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        HWND_DESKTOP, NULL, hInstance, NULL
    );

    if (!g_hWnd) {
        MessageBox(NULL, "Failed to create window", "Error", MB_OK);
        UnregisterClass(wcex.lpszClassName, hInstance);
        FreeLibrary(g_engineDLL);
        return -1;
    }

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // エンジンの初期化
    EngineConfig config = {};
    config.hWnd = g_hWnd;
    config.screenWidth = screenWidth;
    config.screenHeight = screenHeight;
    config.windowTitle = APP_TITLE;
    config.fullscreen = false;
    config.targetFPS = 60.0f;
    config.startScene = "MainScene";

    if (EngineInit(config) != 0) {
        MessageBox(NULL, "Engine initialization failed", "Error", MB_OK);
        DestroyWindow(g_hWnd);
        UnregisterClass(wcex.lpszClassName, hInstance);
        FreeLibrary(g_engineDLL);
        return -1;
    }

    // メインループ
    MSG msg = {};
    while (EngineIsRunning && EngineIsRunning())
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
        {
            if (!GetMessage(&msg, NULL, 0, 0))
            {
                break;
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            EngineUpdate();
			EngineDraw();
        }
    }

    // 終了処理
    EngineShutdown();
    DestroyWindow(g_hWnd);
    UnregisterClass(wcex.lpszClassName, hInstance);
    FreeLibrary(g_engineDLL);

    return 0;
}

