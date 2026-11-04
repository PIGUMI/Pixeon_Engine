#ifndef MAIN_FRAME_H
#define MAIN_FRAME_H

#define PIXEON_ENGINE_VERSION (500.0f)
#define PIXEON_ENGINE_INEDITOR true

#include <Windows.h>
#include "../Common/EngineApi.h"
#include <string>
#include "RHI/RHI.h"
#include <chrono>
#include <list>
#include <vector>

class GameRenderTarget;
class GBuffer;
class LightingPass;
class Layer;
class AbstractScene;

enum class SoftWareMode {
	ENGINE,
	ANIMTOR2D,
};

class MainFrame {
private:
	using clock = std::chrono::steady_clock;
public:
	using EngineConfig = ::EngineConfig;
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
	bool IsEditorMode() const { return _engineConfig.runMode == EngineRunMode::Editor; }
	const EngineConfig& GetEngineConfig() const { return _engineConfig; }
	// 起動引数で指定された開始シーン（空ならシーン設定ファイルに従う）
	const std::string& GetStartSceneOverride() const { return _startSceneName; }
	float GetDeltaTime() { return _deltaTime; }
	RHI::Texture* GetFinalRenderTarget();
	void SetSoftwareMode(SoftWareMode mode) { _softwareMode = mode; }
	SoftWareMode GetSoftwareMode() const { return _softwareMode; }
	void fixedMouseCursor(bool fixedCursor) { _fixedMouseCursorFlag = fixedCursor; }

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

	void DrawGeometryPass(int layerIndex, GBuffer* gbuffer);
	Layer* LayerSettingsGet(int layerIndex) const;
	void LayerSceneDraw(int layerIndex, GameRenderTarget* layerRT, Layer* layerSettings);
	RHI::Texture* DeferredSSAOGenerate(AbstractScene* scene, Layer* layerSettings, GBuffer* gb);
	void LayerAnimator2DDraw(int layerIndex, GameRenderTarget* layerRT);
	void LayerComposite(int layerIndex, GameRenderTarget* layerRT, Layer* layerSettings);
	bool AssetsMount();
	void AssetHotReloadRegister();
	std::string _startSceneName;
	std::string _archivePath;
	int _assetListenerId = 0;

private:
	MainFrame() = default;
	~MainFrame() = default;
private:
	static MainFrame* instance_;
};
#endif // !MAIN_FRAME_H
