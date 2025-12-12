#ifndef MAIN_FRAME_H
#define MAIN_FRAME_H

#define PIXEON_ENGINE_VERSION "1.1.0"
#define PIXEON_ENGINE_INEDITOR true

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
	// Window Handle ?èÔ
	HWND GetWindowHandle() const { return m_hWnd_; }
	float GetDeltaTime() { return deltaTime_; }
	GameRenderTarget* GetGameRenderTarget() const { return m_gameRenderTarget_; }
	ID3D11ShaderResourceView* GetGameRenderTargetSRV();
	ID3D11ShaderResourceView* GetFinalRenderTargetSRV();
	void SetSoftwareMode(SoftWareMode mode) { softwareMode_ = mode; }
	SoftWareMode GetSoftwareMode() const { return softwareMode_; }
private:
	DWORD lastUpdateTime_;
	bool bUpdateDraw;
	float targetFrameTime_;
	float deltaTime_;

	HWND m_hWnd_;
	GameRenderTarget* m_gameRenderTarget_;
	GameRenderTarget* m_uiRenderTarget_ = nullptr;
	GameRenderTarget* m_finalRenderTarget_ = nullptr;

	SoftWareMode softwareMode_;
private:
	// CompositePass Ç MainFrame ÇÃÉÅÉ\ÉbÉhÇ∆ÇµÇƒêÈåæ
	void CompositePass(ID3D11ShaderResourceView* sceneSRV, ID3D11ShaderResourceView* uiSRV, GameRenderTarget* finalRT);

private:
	MainFrame() = default;
	~MainFrame() = default;
private:
	static MainFrame* instance_;
};
#endif // !MAIN_FRAME_H