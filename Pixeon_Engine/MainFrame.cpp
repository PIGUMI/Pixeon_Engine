/*
* ファイル名　MainFrame
* 説　　　明　ソフトウェアのメインフレームクラス
*		　　　エンジン全体の初期化、更新、描画、終了処理
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
#include "EffectManager.h"
#include "ComponentManager.h"
#include "ScriptManager.h"
#include "ResourceService.h"
#include "Animator2DManager.h"
#include "SceneManger.h"
#include "Input.h"
#include "EngineFrame.h"
#include "Animator2DFrame.h"
#include "ImageUtils.h"
#include "LayerSettings.h"
#include "IZANAGI.h"
#include <crtdbg.h>

MainFrame* MainFrame::instance_ = nullptr;

/*
* 関数名　GetInstance
* 引　数　なし
* 戻り値　MainFrame*：MainFrameのインスタンスへのポインタ
* 説　明　MainFrameのシングルトンインスタンスを取得する関数
*/
MainFrame* MainFrame::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new MainFrame();
	}
	return instance_;
}

/*
* 関数名　DeleteInstance
* 引　数　なし
* 戻り値　なし
* 説　明　MainFrameのシングルトンインスタンスを削除する関数
*/
void MainFrame::DeleteInstance() {
	if (instance_ != nullptr) {
		delete instance_;
		instance_ = nullptr;
	}
}

/*
* 関数名　Init
* 引　数　const EngineConfig& InPut：エンジン初期化設定構造体への参照
* 戻り値　int：初期化成功なら0、失敗なら-1
* 説　明　MainFrameの初期化を行う関数
*/
int MainFrame::Init(const EngineConfig& InPut)
{
	_targetFrameTime = 1000.0f / 70.0f;
	_lastUpdateTime = timeGetTime();
	_wnd = InPut.wnd;
	_updateDraw = false;;
	_engineConfig = InPut;

	if (!IZANAGI::GetInstance()->Initialize("SceneRoot/Tool/IZANAGI/qwen2.5-7b-instruct-q3_k_m.gguf", 8192, 4, 2)) {
		MessageBox(nullptr, "IZANAGIエンジンの初期化に失敗しました。モデルファイルを確認してください。", "エラー", MB_OK | MB_ICONERROR);
	}

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
	/* EffectManagerの初期化*/
	EffectManager::Instance()->Init(DirectX11::GetInstance()->GetDevice(), DirectX11::GetInstance()->GetContext());

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

/*
* 関数名　Update
* 引　数　なし
* 戻り値　なし
* 説　明　MainFrameの更新処理を行う関数
*/
void MainFrame::Update()
{
	// フレーム制御
	DWORD current_Time = timeGetTime();
	float delta_Time = static_cast<float>(current_Time - _lastUpdateTime);

	if (delta_Time >= _targetFrameTime) {
		// deltaTime を秒単位に変換
		_deltaTime = delta_Time * 0.001f; // ms -> s
		// 入力更新
		UpdateInput(GetWindowHandle());

		// ソフトウェアモードごとの更新処理
		switch (_softwareMode)
		{
		case SoftWareMode::ENGINE:
			EngineFrame::GetInstance()->Update();
			if (SceneManger::GetInstance())
			{
				if (auto scene = SceneManger::GetInstance()->GetCurrentScene())
				{
					scene->UpdateEffects(_deltaTime);
				}
			}
			break;
		case SoftWareMode::ANIMTOR2D:
			Animator2DFrame::GetInstance()->Update();
			break;
		default:
			break;
		}
		SetMouseFreeze(_fixedMouseCursorFlag);

		// 更新時間記録
		_lastUpdateTime = current_Time;
		_updateDraw = true;
	}
}

/*
* 関数名　Draw
* 引　数　なし
* 戻り値　なし
* 説　明　MainFrameの描画処理を行う関数
*/
void MainFrame::Draw()
{
	if (_updateDraw) {
		auto* dx11 = DirectX11::GetInstance();
		ID3D11DeviceContext* ctx = dx11->GetContext();

		ID3D11RenderTargetView* hdrRTV = dx11->GetHDRRTV();
		ID3D11DepthStencilView* dsv = dx11->GetDefaultDSV()->GetView();

		// HDRバッファクリア
		float clear_Color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		ctx->ClearRenderTargetView(hdrRTV, clear_Color);
		ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		// レイヤーごとの描画（エフェクトなし）
		int Layer_Index = 0;
		auto layerRTIt = _layerRenderTargets.begin();

		for (; layerRTIt != _layerRenderTargets.end() && Layer_Index < MAX_LAYER_COUNT; ++layerRTIt, ++Layer_Index) {
			auto layerRT = *layerRTIt;

			Layer* layerSettings = nullptr;
			if (_softwareMode == SoftWareMode::ENGINE) {
				if (SceneManger::GetInstance())
				{
					if (auto scene = SceneManger::GetInstance()->GetCurrentScene())
					{
						layerSettings = scene->GetLayer(Layer_Index);
					}
				}
			}

			if (layerSettings && !layerSettings->visible) {
				continue;
			}

			// 通常描画
			layerRT->SetBlend(true);
			layerRT->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());
			layerRT->Begin(ctx);

			switch (_softwareMode)
			{
			case SoftWareMode::ENGINE:
				EngineFrame::GetInstance()->Draw(Layer_Index);
				DirectX11::GetInstance()->SetBlendMode(BLEND_ALPHA);
				if (SceneManger::GetInstance())
				{
					if (auto scene = SceneManger::GetInstance()->GetCurrentScene())
					{
						if (auto camera = scene->GetMainCamera())
						{
							scene->DrawEffects(Layer_Index, camera);
						}
					}
				}
				break;
			case SoftWareMode::ANIMTOR2D:
				if (Layer_Index == 0)Animator2DFrame::GetInstance()->Draw();
				break;
			}

			layerRT->End();
		}

		// 最終合成バッファへ描画
		_finalRenderTarget->SetRenderZBuffer(false);
		_finalRenderTarget->Begin(ctx);
		dx11->ApplyToneMappingPass();

		// レイヤーを順番に合成
		Layer_Index = 0;
		layerRTIt = _layerRenderTargets.begin();

		for (; layerRTIt != _layerRenderTargets.end() && Layer_Index < MAX_LAYER_COUNT; ++layerRTIt, ++Layer_Index) {
			auto layerRT = *layerRTIt;

			Layer* layerSettings = nullptr;
			if (_softwareMode == SoftWareMode::ENGINE) {
				if (SceneManger::GetInstance())
				{
					if (auto scene = SceneManger::GetInstance()->GetCurrentScene())
					{
						layerSettings = scene->GetLayer(Layer_Index);
					}
				}
			}

			if (layerSettings && !layerSettings->visible) {
				continue;
			}

			float opacity = layerSettings ? layerSettings->opacity : 1.0f;

			// ポストエフェクト適用判定
			if (layerSettings && !layerSettings->postEffects.empty()) {
				// ポストエフェクトを最終出力に直接適用
				layerSettings->ApplyPostEffectsToScreen(
					layerRT->GetShaderResourceView(),
					_engineConfig.screenWidth,
					_engineConfig.screenHeight,
					opacity
				);
			}
			else {
				// エフェクトなし:  通常描画
				ImageUtils::DrawSRV(
					layerRT->GetShaderResourceView(),
					0.0f, 0.0f,
					(float)_engineConfig.screenWidth,
					(float)_engineConfig.screenHeight,
					DirectX::XMFLOAT4(1, 1, 1, 1),
					DirectX::XMFLOAT4(0, 0, 1, 1),
					true,
					opacity
				);
			}
		}

		_finalRenderTarget->End();

		// GUI描画
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
		}

		GUI::GetInstance()->EndDraw();
		DirectX11::GetInstance()->EndDraw();

		_updateDraw = false;
	}
}

/*
* 関数名　UnInit
* 引　数　なし
* 戻り値　なし
* 説　明　MainFrameの終了処理を行う関数
*/
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
	IZANAGI::GetInstance()->Shutdown();
	IZANAGI::DestroyInstance();
	CoUninitialize();
}

/*
* 関数名　GetFinalRenderTargetSRV
* 引　数　なし
* 戻り値　ID3D11ShaderResourceView*：最終レンダーテクスチャのシェーダーリソースビューへのポインタ
* 説　明　最終レンダーテクスチャのシェーダーリソースビューを取得する関数
*/
ID3D11ShaderResourceView* MainFrame::GetFinalRenderTargetSRV()
{
	if (_finalRenderTarget) {
		return _finalRenderTarget->GetShaderResourceView();
	}
	return nullptr;
}