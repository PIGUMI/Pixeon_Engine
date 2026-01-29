#ifndef MAIN_FRAME_H
#define MAIN_FRAME_H

#define PIXEON_ENGINE_VERSION (400.0f)
#define PIXEON_ENGINE_INEDITOR true

#include <Windows.h>
#include <string>
#include <d3d11.h>
#include <chrono>
#include <list>

class GameRenderTarget;

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
	HWND GetWindowHandle() const { return _wnd; }
	float GetDeltaTime() { return _deltaTime; }
	ID3D11ShaderResourceView* GetFinalRenderTargetSRV();
	void SetSoftwareMode(SoftWareMode mode) { _softwareMode = mode; }
	SoftWareMode GetSoftwareMode() const { return _softwareMode; }
	void fixedMouseCursor(bool fixedCursor) { _fixedMouseCursorFlag = fixedCursor; }
	bool isPixelated() const { return _PixelatedFlag; }
	void setPixelated(bool pixelated) { _PixelatedFlag = pixelated; }
private:
	DWORD _lastUpdateTime;
	bool _updateDraw;
	float _targetFrameTime;
	float _deltaTime;

	HWND _wnd;
	GameRenderTarget* _finalRenderTarget;
	std::list<GameRenderTarget*> _layerRenderTargets;

	SoftWareMode _softwareMode;
	EngineConfig _engineConfig;

	bool _fixedMouseCursorFlag = false;

	bool _PixelatedFlag = false;
private:
	MainFrame() = default;
	~MainFrame() = default;
private:
	static MainFrame* instance_;
};
#endif // !MAIN_FRAME_H