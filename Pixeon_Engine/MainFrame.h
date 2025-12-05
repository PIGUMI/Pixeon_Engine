#ifndef MAIN_FRAME_H
#define MAIN_FRAME_H

#define PIXEON_ENGINE_VERSION "1.1.0"
#define PIXEON_ENGINE_INEDITOR true

// ソフトウェア全体の管理を行うクラス
// 全体管理を行う
// シングルトン
/*
* Log
* 2025/11/25 リファクタリング
* 2025/12/01 リファクタリング
* 2025/12/05 EngineManager から MainFrame に改名
*/

#include <Windows.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include <chrono>

class GameRenderTarget;
class Object;

enum class SoftWareMode {
	ENGINE,
	ANIMTOR2D,
};

class MainFrame {
private:
	using clock = std::chrono::steady_clock;
public:
	struct EngineConfig {
		HWND		wnd;
		int			screenWidth;
		int			screenHeight;
		const char* windowTitle;
		bool		fullscreen;
		float		targetFPS;
		const char* startScene;
	};
public:
	static MainFrame* GetInstance();
	static void DeleteInstance();
public:
	int Init(const EngineConfig& InPut);
	void Update();
	void Draw();
	void UnInit();
public:
	// Window Handle 取得
	HWND GetWindowHandle() const { return m_hWnd_; }
	float GetDeltaTime() { return deltaTime_; }
	GameRenderTarget* GetGameRenderTarget() const { return m_gameRenderTarget_; }
	void SetSoftwareMode(SoftWareMode mode) { softwareMode_ = mode; }
	SoftWareMode GetSoftwareMode() const { return softwareMode_; }
private:
	DWORD lastUpdateTime_;
	bool bUpdateDraw;
	float targetFrameTime_;
	float deltaTime_;

	HWND m_hWnd_;
	GameRenderTarget* m_gameRenderTarget_;

	SoftWareMode softwareMode_;
private:
	MainFrame() = default;
	~MainFrame() = default;
private:
	static MainFrame* instance_;
};
#endif // !MAIN_FRAME_H
