/* MainFrame */
/*
* ソフトウェアのメインフレームクラス
* エンジン全体の初期化、更新、描画、終了処理
* Render管理、GUI管理、各種マネージャーの初期化などを担当
* シングルトンパターンで実装
*/

#include "MainFrame.h"
#include "System.h"
#include "GameRenderTarget.h"
#include "GUI.h"
#include "AssetManager.h"
#include "ModelManager.h"
#include "TextureManager.h"
#include "SettingManager.h"
#include "ShaderManager.h"
#include "SoundManager.h"
#include "ComponentManager.h"
#include "ScriptManager.h"
#include "ResourceService.h"
#include "Animator2DManager.h"
#include "Input.h"
#include "EngineFrame.h"
#include "Animator2DFrame.h"
#include "ImageUtils.h"

#include <crtdbg.h>

MainFrame* MainFrame::instance_ = nullptr;

MainFrame* MainFrame::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new MainFrame();
	}
	return instance_;
}

void MainFrame::DeleteInstance() {
	if (instance_ != nullptr) {
		delete instance_;
		instance_ = nullptr;
	}
}

int MainFrame::Init(const EngineConfig& InPut)
{
	_targetFrameTime = 1000.0f / 70.0f;
	_lastUpdateTime = timeGetTime();
	_wnd = InPut.wnd;
	_updateDraw = false;;
	_engineConfig = InPut;

	SettingManager::GetInstance()->LoadConfig();
	/* COM の初期化 */
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr)) return -1;
	/* DirectX11 初期化 */
	hr = DirectX11::GetInstance()->Init(InPut.wnd, InPut.screenWidth, InPut.screenHeight, InPut.fullscreen);
	if (FAILED(hr)) {
		CoUninitialize();
		return -1;
	}
	DirectX11::GetInstance()->InitializeHDRPipeline(InPut.screenWidth, InPut.screenHeight);
	// AssetManager のルートパス設定
	AssetManager::Instance()->SetRoot(SettingManager::GetInstance()->GetAssetsFilePath());
	// AssetManager のロードモード設定
	AssetManager::Instance()->SetLoadMode(AssetManager::LoadMode::FromSource);
	// AssetManager の自動同期開始
	AssetManager::Instance()->StartAutoSync(std::chrono::milliseconds(1000), true);
	// レイヤーレンダーテクスチャ初期化
	for (int layer = 0; layer < MAX_LAYER_COUNT; layer++)
	{
		GameRenderTarget* Layer = new GameRenderTarget();
		Layer->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);
		Layer->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());
		_layerRenderTargets.push_back(Layer);
	}
	// 最終レンダーテクスチャ初期化
	_finalRenderTarget = new GameRenderTarget();
	_finalRenderTarget->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);
	_finalRenderTarget->SetRenderZBuffer(false);
	/* GUIの初期化 */
	GUI::GetInstance()->Init();
	/* シェーダーマネージャーの初期化 */
	ShaderManager::GetInstance()->Initialize(DirectX11::GetInstance()->GetDevice());
	/* コンポーネントマネージャーの初期化 */
	ComponentManager::GetInstance()->Init();
	/* スクリプトマネージャーの初期化 */
	ScriptManager::Instance().RegisterAllScripts();
	/* 入力初期化 */
	InitInput();
	/* 各フレーム初期化 */
	EngineFrame::GetInstance()->Init();
	Animator2DFrame::GetInstance()->Init();
	return 0;
}

void MainFrame::Update()
{
	// フレーム制御
	DWORD currentTime = timeGetTime();
	float deltaTime = static_cast<float>(currentTime - _lastUpdateTime);

	if (deltaTime >= _targetFrameTime) {
		// deltaTime を秒単位に変換
		_deltaTime = deltaTime * 0.001f; // ms -> s
		// 入力更新
		UpdateInput(GetWindowHandle());
		// ソフトウェアモードごとの更新処理
		switch (_softwareMode)
		{
		case SoftWareMode::ENGINE:
			EngineFrame::GetInstance()->Update();
			break;
		case SoftWareMode::ANIMTOR2D:
			Animator2DFrame::GetInstance()->Update();
			break;
		default:
			break;
		}

		// 更新時間記録
		_lastUpdateTime = currentTime;
		_updateDraw = true;
	}
}

void MainFrame::Draw()
{
	if (_updateDraw) {
		auto* dx11 = DirectX11::GetInstance();

		ID3D11RenderTargetView* hdrRTV = dx11->GetHDRRTV();
		ID3D11DepthStencilView* dsv = dx11->GetDefaultDSV()->GetView();

		float clearColor[4] = { 0.0f,0.0f,0.0f,0.0f };
		dx11->GetContext()->ClearRenderTargetView(hdrRTV, clearColor);
		dx11->GetContext()->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		dx11->GetContext()->OMSetRenderTargets(1, &hdrRTV, dsv);

		int LayerIndex = 0;
		for (auto layerRT : _layerRenderTargets) {
			layerRT->SetBlend(true);
			layerRT->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());
			layerRT->Begin(dx11->GetContext());
			switch (_softwareMode)
			{
			case SoftWareMode::ENGINE:
				EngineFrame::GetInstance()->Draw(LayerIndex);
				break;
			case SoftWareMode::ANIMTOR2D:
				Animator2DFrame::GetInstance()->Draw();
				break;
			default:
				break;
			}
			layerRT->End();

			dx11->GetContext()->OMSetRenderTargets(1, &hdrRTV, dsv);
			ImageUtils::DrawSRV(
				layerRT->GetShaderResourceView(),
				0.0f, 0.0f,
				(float)_engineConfig.screenWidth,
				(float)_engineConfig.screenHeight
			);
			LayerIndex++;
		}

		_finalRenderTarget->SetRenderZBuffer(false);
		_finalRenderTarget->Begin(dx11->GetContext());
		dx11->ApplyToneMappingPass();
		for (auto layerRT : _layerRenderTargets) {
			ImageUtils::DrawSRV(layerRT->GetShaderResourceView(), 0.0f, 0.0, (float)_engineConfig.screenWidth, (float)_engineConfig.screenHeight);
		}
		_finalRenderTarget->End();

		// メイン描画
		DirectX11::GetInstance()->BeginDraw();
		GUI::GetInstance()->BeginDraw();
		switch (_softwareMode)
		{
		case SoftWareMode::ENGINE:
			EngineFrame::GetInstance()->DrawGUI();
			break;
		case SoftWareMode::ANIMTOR2D:
			Animator2DFrame::GetInstance()->DrawGUI();
			break;
		default:
			break;
		}
		GUI::GetInstance()->EndDraw();
		DirectX11::GetInstance()->EndDraw();

		_updateDraw = false;
	}
}

void MainFrame::UnInit() {
	// レンダーテクスチャ解放
	for (auto layerRT : _layerRenderTargets) {
		delete layerRT;
	}
	_layerRenderTargets.clear();
	if (_finalRenderTarget)
	{
		delete _finalRenderTarget;
		_finalRenderTarget = nullptr;
	}
	Animator2DFrame::GetInstance()->UnInit();
	Animator2DFrame::DestroyInstance();
	EngineFrame::GetInstance()->UnInit();
	EngineFrame::DestroyInstance();
	UninitInput();
	AssetManager::Instance()->StopAutoSync();
	SettingManager::GetInstance()->SaveConfig();
	GUI::DestroyInstance();
	ComponentManager::DestroyInstance();
	SettingManager::DestroyInstance();
	ScriptManager::Release();
	ShaderManager::DestroyInstance();
	Animator2DManager::DestroyInstance();
	AssetManager::DeleteInstance();
	ModelManager::DeleteInstance();
	TextureManager::DeleteInstance();
	SoundManager::DeleteInstance();
	ResourceService::DeleteInstance();
	DirectX11::DestroyInstance();
	CoUninitialize();
}

ID3D11ShaderResourceView* MainFrame::GetFinalRenderTargetSRV()
{
	if (_finalRenderTarget) {
		return _finalRenderTarget->GetShaderResourceView();
	}
	return nullptr;
}