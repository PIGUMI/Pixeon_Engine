#ifndef MAIN_FRAME_H
#define MAIN_FRAME_H

#define PIXEON_ENGINE_VERSION (500.0f)
#define PIXEON_ENGINE_INEDITOR true

#include <Windows.h>
#include <string>
#include <d3d11.h>
#include <chrono>
#include <list>
#include <vector>

class GameRenderTarget;
class GBuffer;
class LightingPass;

enum class SoftWareMode {
	ENGINE,
	ANIMTOR2D,
};

class MainFrame {
private:
	using clock = std::chrono::steady_clock;
public:
	struct EngineConfig {
		HWND        wnd;
		int         screenWidth;
		int         screenHeight;
		const char* windowTitle;
		bool        fullscreen;
		float       targetFPS;
		const char* startScene;
	};
public:
	static MainFrame* GetInstance();
	static void DeleteInstance();
public:
	int  Init(const EngineConfig& InPut);
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

	GBuffer* GetGBuffer(int layerIndex) const;

private:
	DWORD _lastUpdateTime;
	bool  _updateDraw;
	float _targetFrameTime;
	float _deltaTime;

	HWND _wnd;
	GameRenderTarget* _finalRenderTarget = nullptr;
	std::list<GameRenderTarget*>  _layerRenderTargets;
	std::vector<GBuffer*>         _gBuffers;
	std::vector<LightingPass*>    _lightingPasses;

	SoftWareMode _softwareMode;
	EngineConfig _engineConfig;

	bool _fixedMouseCursorFlag = false;
	bool _PixelatedFlag = false;

	void DrawGeometryPass(int layerIndex, GBuffer* gbuffer);

private:
	MainFrame() = default;
	~MainFrame() = default;
private:
	static MainFrame* instance_;
};
#endif // !MAIN_FRAME_H