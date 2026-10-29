/*
* MainFrame.cpp
*/

#include "MainFrame.h"
#include "System.h"
#include "GameRenderTarget.h"
#include "GBuffer.h"
#include "LightingPass.h"
#include "GUI.h"
#include "Builder.h"
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
#include "Scene.h"
#include "SSAOEffect.h"
#include "IZANAGI.h"
#include "_Geometry.h"
#include <crtdbg.h>

MainFrame* MainFrame::instance_ = nullptr;

/*
* 関数名：GetInstance
* 引　数：なし
* 戻り値：MainFrame*
* 説　明：MainFrameのシングルトンインスタンスを取得する関数。
*/
MainFrame* MainFrame::GetInstance() {
	if (instance_ == nullptr) instance_ = new MainFrame();
	return instance_;
}

/*
* 関数名：DeleteInstance
* 引　数：なし
* 戻り値：なし
* 説　明：MainFrameのシングルトンインスタンスを削除する関数。
*/
void MainFrame::DeleteInstance() {
	if (instance_ != nullptr) { delete instance_; instance_ = nullptr; }
}

/*
* 関数名：Init
* 引　数：const EngineConfig& InPut - エンジンの初期化に必要な設定をまとめた構造体
* 戻り値：int - 初期化の成功を示すステータスコード。0は成功、負の値はエラーを示す。
* 説　明：エンジンの初期化を行う関数
*/
int MainFrame::Init(const EngineConfig& InPut)
{
	_targetFrameTime = 1000.0f / 200.0f;
	_lastUpdateTime = timeGetTime();
	_wnd = (HWND)InPut.wnd;
	_updateDraw = false;
	_engineConfig = InPut;
	_startSceneName = InPut.startScene ? InPut.startScene : "";
	_archivePath = InPut.archivePath ? InPut.archivePath : "data.pak";
	_engineConfig.startScene = nullptr;
	_engineConfig.archivePath = nullptr;

	if (IsEditorMode()) {
		if (!IZANAGI::GetInstance()->Initialize("SceneRoot/Tool/IZANAGI/qwen2.5-7b-instruct-q3_k_m.gguf", 8192, 4, 2))
			MessageBox(nullptr, "IZANAGIエンジンの初期化に失敗しました。", "エラー", MB_OK | MB_ICONERROR);
	}

	if (!AssetsMount()) return -1;
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr)) return -1;

	std::string graphicsError;
	if (!GraphicsSystem::GetInstance()->Init(_wnd, InPut.screenWidth, InPut.screenHeight, InPut.fullscreen,
		InPut.vsync, InPut.backend, &graphicsError)) {
		MessageBox(nullptr, ("描画デバイスの初期化に失敗しました\n" + graphicsError).c_str(), "エラー", MB_OK | MB_ICONERROR);
		CoUninitialize();
		return -1;
	}
	ShaderManager::GetInstance()->Initialize(GraphicsSystem::GetInstance()->GetDevice());
	AssetHotReloadRegister();
	EffectManager::Instance()->Init(GraphicsSystem::GetInstance()->GetDevice());

	for (int layer = 0; layer < MAX_LAYER_COUNT; layer++)
	{
		GameRenderTarget* layerRT = new GameRenderTarget();
		layerRT->InitWithDepthSRV(InPut.screenWidth, InPut.screenHeight);
		layerRT->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());
		_layerRenderTargets.push_back(layerRT);

		GBuffer* gb = new GBuffer();
		if (!gb->Init(InPut.screenWidth, InPut.screenHeight))
		{
			delete gb; gb = nullptr;
		}
		_gBuffers.push_back(gb);

		LightingPass* lp = new LightingPass();
		if (!lp->Init())
		{
			delete lp; lp = nullptr;
		}
		_lightingPasses.push_back(lp);
	}

	_finalRenderTarget = new GameRenderTarget();
	_finalRenderTarget->Init(InPut.screenWidth, InPut.screenHeight);
	_finalRenderTarget->SetRenderZBuffer(false);

	GUI::GetInstance()->Init();
	if (IsEditorMode()) Builder::GetInstance()->Init();
	ComponentManager::GetInstance()->Init();
	ScriptManager::Instance().RegisterAllScripts();
	InitInput();
	EngineFrame::GetInstance()->Init();
	Animator2DFrame::GetInstance()->Init();
	return 0;
}

/*
* 関数名：AssetsMount
* 引　数：なし
* 戻り値：bool - マウントに成功したら true
* 説　明：起動モードに応じてアセットの供給元をマウントする
*         エディタ: SceneRoot 以下のディレクトリ（Assets / Scene は変更を監視）
*         ゲーム  : ビルド時に作成したアーカイブ
*/
bool MainFrame::AssetsMount()
{
	AssetManager* am = AssetManager::Instance();
	if (!IsEditorMode()) {
		std::string error;
		if (!am->ArchiveMount(_archivePath, &error)) {
			MessageBox(nullptr, ("ゲームデータを開けません: " + _archivePath + "\n" + error).c_str(), "エラー", MB_OK | MB_ICONERROR);
			return false;
		}
		SettingManager::GetInstance()->LoadConfig();
		return true;
	}

	SettingManager* settings = SettingManager::GetInstance();
	am->DirectoryMount(VfsPrefix::CONFIG, settings->GetConfigDirectoryPath(), false);
	settings->LoadConfig();
	am->DirectoryMount(std::string(VfsPrefix::SHADER) + "cso", settings->GetCSOFilePath(), false);
	am->DirectoryMount(std::string(VfsPrefix::SHADER) + "spv", settings->GetSPIRVFilePath(), false);
	am->DirectoryMount(VfsPrefix::ANIM2D, settings->GetAnimator2DProjectFilePath(), false);
	am->DirectoryMount(VfsPrefix::SCENE, settings->GetSceneFilePath(), true);
	am->DirectoryMount(VfsPrefix::ASSETS, settings->GetAssetsFilePath(), true);
	am->WatchStart(std::chrono::milliseconds(1000));
	return true;
}

/*
* 関数名：AssetHotReloadRegister
* 引　数：なし
* 戻り値：なし
* 説　明：アセットの変更通知を受けて、読み込み済みのテクスチャを再読込する
*/
void MainFrame::AssetHotReloadRegister()
{
	_assetListenerId = AssetManager::Instance()->ListenerAdd([](const FileChange& change) {
		const std::string prefix = VfsPrefix::ASSETS;
		if (change.e_Type != FileChange::Type::Modified) return;
		if (PixArchive::PathNormalize(change.path).rfind(prefix, 0) != 0) return;
		const std::string name = change.path.substr(prefix.size());
		switch (AssetTypeUtil::TypeFromPath(name)) {
		case AssetType::Texture:
			if (TextureManager::Instance()->IsLoaded(name)) TextureManager::Instance()->Reload(name);
			break;
		case AssetType::Model:
			// 使用中のモデルはボーン構成が変わり得るため差し替えず、次回の読み込みから新しい内容を使う
			ModelManager::Instance()->CacheInvalidate(name);
			break;
		default:
			break;
		}
		});
}

/*
* 関数名：Update
* 引　数：なし
* 戻り値：なし
* 説　明：エンジンの更新処理を行う関数
*/
void MainFrame::Update()
{
	DWORD current_Time = timeGetTime();
	float delta_Time = static_cast<float>(current_Time - _lastUpdateTime);

	if (delta_Time >= _targetFrameTime) {
		_deltaTime = delta_Time * 0.001f;
		AssetManager::Instance()->ChangesDispatch();
		UpdateInput(GetWindowHandle());

		switch (_softwareMode)
		{
		case SoftWareMode::ENGINE:
			EngineFrame::GetInstance()->Update();
			if (auto scene = SceneManger::GetInstance()->GetCurrentScene())
				scene->UpdateEffects(_deltaTime);
			break;
		case SoftWareMode::ANIMTOR2D:
			Animator2DFrame::GetInstance()->Update();
			break;
		}
		SetMouseFreeze(_fixedMouseCursorFlag);
		_lastUpdateTime = current_Time;
		_updateDraw = true;
	}
}

/*
* 関数名：DrawGeometryPass
* 引　数：int layerIndex - 描画するレイヤーのインデックス
* 	  GBuffer* gbuffer - ジオメトリパスで使用するGBufferのポインタ
* 戻り値：なし
* 説　明：ジオメトリパスを実行する関数。指定されたレイヤーのオブジェクトをGBufferに描画する。
*/
void MainFrame::DrawGeometryPass(int layerIndex, GBuffer* gbuffer)
{
	if (!gbuffer) return;
	gbuffer->BeginGeometryPass();
	if (_softwareMode == SoftWareMode::ENGINE)
	{
		if (auto scene = SceneManger::GetInstance()->GetCurrentScene())
			scene->DrawForGBuffer(layerIndex);
	}
	gbuffer->EndGeometryPass();
}

/*
* 関数名：GetGBuffer
* 引　数：int layerIndex - 取得するGBufferのレイヤーインデックス
* 戻り値：GBuffer* - 指定されたレイヤーのGBufferのポインタ。インデックスが範囲外の場合はnullptrを返す。
* 説　明：指定されたレイヤーのGBufferを取得する関数。レイヤーインデックスが有効な範囲内であれば、そのレイヤーに対応するGBufferのポインタを返す。インデックスが無効な場合はnullptrを返す。
*/
GBuffer* MainFrame::GetGBuffer(int layerIndex) const
{
	if (layerIndex < 0 || layerIndex >= (int)_gBuffers.size()) return nullptr;
	return _gBuffers[layerIndex];
}

/*
* 関数名：Draw
* 引　数：なし
* 戻り値：なし
* 説　明：エンジンの描画処理を行う関数。
*/
void MainFrame::Draw()
{
	if (!_updateDraw) return;

	GraphicsSystem* gs = GraphicsSystem::GetInstance();
	RHI::Device* dev = gs->GetDevice();
	dev->DepthStencilClear(gs->GetDefaultDepth(), true, true, 1.0f, 0);

	// ---- 各レイヤーをオフスクリーンへ描画 ----
	int layerIndex = 0;
	for (auto it = _layerRenderTargets.begin(); it != _layerRenderTargets.end() && layerIndex < MAX_LAYER_COUNT; ++it, ++layerIndex)
	{
		Layer* layerSettings = LayerSettingsGet(layerIndex);
		if (layerSettings && !layerSettings->visible) continue;
		if (_softwareMode == SoftWareMode::ENGINE) LayerSceneDraw(layerIndex, *it, layerSettings);
		else if (_softwareMode == SoftWareMode::ANIMTOR2D) LayerAnimator2DDraw(layerIndex, *it);
	}

	// ---- レイヤーを合成（ポストエフェクト適用）----
	_finalRenderTarget->SetRenderZBuffer(false);
	_finalRenderTarget->Begin();
	gs->SetBlendMode(BLEND_ALPHA);
	layerIndex = 0;
	for (auto it = _layerRenderTargets.begin(); it != _layerRenderTargets.end() && layerIndex < MAX_LAYER_COUNT; ++it, ++layerIndex)
	{
		Layer* layerSettings = LayerSettingsGet(layerIndex);
		if (layerSettings && !layerSettings->visible) continue;
		LayerComposite(layerIndex, *it, layerSettings);
	}
	_finalRenderTarget->End();

	// ---- バックバッファへ（エディタは GUI の GameView、ゲームは全画面に表示）----
	gs->BeginDraw();
	if (IsEditorMode()) {
		GUI::GetInstance()->BeginDraw();
		switch (_softwareMode)
		{
		case SoftWareMode::ENGINE:    EngineFrame::GetInstance()->DrawGUI(); break;
		case SoftWareMode::ANIMTOR2D: Animator2DFrame::GetInstance()->DrawGUI(); break;
		}
		GUI::GetInstance()->EndDraw();
	}
	else {
		RHI::Texture* bb = gs->GetBackBuffer();
		ImageUtils::DrawSRV(_finalRenderTarget->GetTexture(), 0.0f, 0.0f,
			(float)bb->GetWidth(), (float)bb->GetHeight(),
			DirectX::XMFLOAT4(1, 1, 1, 1), DirectX::XMFLOAT4(0, 0, 1, 1), false, 1.0f);
	}
	gs->EndDraw();

	_updateDraw = false;
}

/*
* 関数名：LayerSettingsGet
* 引　数：int layerIndex - レイヤー番号
* 戻り値：Layer* - 現在のシーンのレイヤー設定（無ければ nullptr）
* 説　明：エンジンモードのときだけシーンのレイヤー設定を返す
*/
Layer* MainFrame::LayerSettingsGet(int layerIndex) const
{
	if (_softwareMode != SoftWareMode::ENGINE) return nullptr;
	auto scene = SceneManger::GetInstance()->GetCurrentScene();
	return scene ? scene->GetLayer(layerIndex) : nullptr;
}

/*
* 関数名：LayerSceneDraw
* 引　数：layerIndex / layerRT / layerSettings - 対象レイヤー
* 戻り値：なし
* 説　明：シャドウ → ジオメトリ (GBuffer) → SSAO → ライティング → 前方描画 → エフェクト の順に描く
*/
void MainFrame::LayerSceneDraw(int layerIndex, GameRenderTarget* layerRT, Layer* layerSettings)
{
	auto scene = SceneManger::GetInstance()->GetCurrentScene();
	if (!scene) return;
	GraphicsSystem* gs = GraphicsSystem::GetInstance();
	RHI::Device* dev = gs->GetDevice();
	GBuffer* gb = GetGBuffer(layerIndex);
	LightingPass* lp = (layerIndex < (int)_lightingPasses.size()) ? _lightingPasses[layerIndex] : nullptr;

	scene->PrepareShadowAndLights(layerIndex);
	if (lp) lp->SetLightBuffers(scene->GetLightArrayCB(), scene->GetLightCountCB());
	DrawGeometryPass(layerIndex, gb);
	if (!lp || !gb) return;

	lp->SetSSAOTexture(DeferredSSAOGenerate(scene, layerSettings, gb));
	auto cam = scene->GetMainCamera();

	layerRT->SetBlend(true);
	layerRT->SetRenderZBuffer(false);
	layerRT->Begin();
	if (cam)
	{
		lp->Execute(gb, scene->GetShadowMap(), _engineConfig.screenWidth, _engineConfig.screenHeight,
			cam->GetProjection(), cam->GetView(), cam->GetWorldPosition());
	}

	// 前方描画は GBuffer の深度を使ってライティング結果の上に重ねる
	RHI::Texture* color = layerRT->GetTexture();
	gs->RenderTargetsSet(1, &color, gb->GetDepth());
	dev->DepthStencilStateSet(nullptr);
	gs->SetBlendMode(BLEND_ALPHA);
	scene->DrawForwardObjects(layerIndex);
	if (cam) scene->DrawEffects(layerIndex, cam);
	layerRT->End();
}

/*
* 関数名：DeferredSSAOGenerate
* 引　数：scene / layerSettings / gb - 対象
* 戻り値：RHI::Texture* - 生成した AO テクスチャ（SSAO が無効なら nullptr）
* 説　明：レイヤーに SSAO があれば GBuffer の法線・深度から AO を生成する
*/
RHI::Texture* MainFrame::DeferredSSAOGenerate(AbstractScene* scene, Layer* layerSettings, GBuffer* gb)
{
	if (!layerSettings || !gb) return nullptr;
	for (std::shared_ptr<PostEffectBase>& fx : layerSettings->postEffects)
	{
		if (!fx || !fx->enabled) continue;
		SSAOEffect* ssao = dynamic_cast<SSAOEffect*>(fx.get());
		if (!ssao) continue;
		auto cam = scene->GetMainCamera();
		if (!cam) return nullptr;
		ssao->SetNormalTexture(gb->GetNormal());
		ssao->SetDepthTexture(gb->GetDepth());
		ssao->SetCameraMatrices(cam->GetProjection(), DirectX::XMMatrixInverse(nullptr, cam->GetProjection()));
		ssao->GenerateAO(_engineConfig.screenWidth, _engineConfig.screenHeight);
		return ssao->IsAOGenerated() ? ssao->GetAOTexture() : nullptr;
	}
	return nullptr;
}

/*
* 関数名：LayerAnimator2DDraw
* 引　数：layerIndex / layerRT - 対象レイヤー
* 戻り値：なし
* 説　明：Animator2D モードではレイヤー 0 にエディタのプレビューを描く
*/
void MainFrame::LayerAnimator2DDraw(int layerIndex, GameRenderTarget* layerRT)
{
	layerRT->SetBlend(true);
	layerRT->SetRenderZBuffer(false);
	layerRT->Begin();
	if (layerIndex == 0) Animator2DFrame::GetInstance()->Draw();
	layerRT->End();
}

/*
* 関数名：LayerComposite
* 引　数：layerIndex / layerRT / layerSettings - 対象レイヤー
* 戻り値：なし
* 説　明：レイヤーを最終ターゲットへ合成する。ポストエフェクトがあれば適用しながら描く
*/
void MainFrame::LayerComposite(int layerIndex, GameRenderTarget* layerRT, Layer* layerSettings)
{
	const float opacity = layerSettings ? layerSettings->opacity : 1.0f;
	if (!layerSettings || layerSettings->postEffects.empty())
	{
		ImageUtils::DrawSRV(layerRT->GetTexture(), 0.0f, 0.0f,
			(float)_engineConfig.screenWidth, (float)_engineConfig.screenHeight,
			DirectX::XMFLOAT4(1, 1, 1, 1), DirectX::XMFLOAT4(0, 0, 1, 1), true, opacity);
		return;
	}

	// デファードでは深度は GBuffer に書かれている。無ければレイヤーの深度にフォールバック
	GBuffer* gb = GetGBuffer(layerIndex);
	RHI::Texture* depth = gb ? gb->GetDepth() : layerRT->GetDepthShaderResource();

	DirectX::XMMATRIX proj = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX invProj = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX invView = DirectX::XMMatrixIdentity();
	DirectX::XMFLOAT3 cameraPos = { 0.f, 0.f, 0.f };
	if (auto scene = SceneManger::GetInstance()->GetCurrentScene())
		if (auto cam = scene->GetMainCamera())
		{
			proj = cam->GetProjection();
			invProj = DirectX::XMMatrixInverse(nullptr, proj);
			invView = DirectX::XMMatrixInverse(nullptr, cam->GetView());
			cameraPos = cam->GetWorldPosition();
		}

	layerSettings->ApplyPostEffectsToScreen(layerRT->GetTexture(),
		_engineConfig.screenWidth, _engineConfig.screenHeight, opacity,
		depth, &proj, &invProj, &invView, &cameraPos);
}

/*
* 関数名：UnInit
* 引　数：なし
* 戻り値：なし
* 説　明：エンジンの終了処理を行う関数。
*/
void MainFrame::UnInit()
{
	for (auto lp : _lightingPasses)
	{
		if (lp) { lp->Release(); delete lp; }
	}
	_lightingPasses.clear();

	for (auto gb : _gBuffers)
	{
		if (gb) { gb->Release(); delete gb; }
	}
	_gBuffers.clear();

	for (auto layerRT : _layerRenderTargets) delete layerRT;
	_layerRenderTargets.clear();

	if (_finalRenderTarget) { delete _finalRenderTarget; _finalRenderTarget = nullptr; }

	Animator2DFrame::GetInstance()->UnInit();
	Animator2DFrame::DestroyInstance();
	EngineFrame::GetInstance()->UnInit();
	EngineFrame::DestroyInstance();
	UninitInput();
	AssetManager::Instance()->ListenerRemove(_assetListenerId);
	AssetManager::Instance()->WatchStop();
	if (IsEditorMode()) SettingManager::GetInstance()->SaveConfig();
	Builder::DeleteInstance();
	GUI::DestroyInstance();
	LineRenderer::GetInstance()->Finalize();
	ImageUtils::Release();
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
	EffectManager::Instance()->UnInit();
	GraphicsSystem::DestroyInstance();
	if (IsEditorMode()) {
		IZANAGI::GetInstance()->Shutdown();
		IZANAGI::DestroyInstance();
	}
	CoUninitialize();
}

/*
* 関数名：GetFinalRenderTarget
* 引　数：なし
* 戻り値：RHI::Texture* - 最終的なレンダリング結果（レンダーターゲットが存在しない場合は nullptr）
* 説　明：最終的なレンダリング結果のテクスチャを取得する関数（GameView 表示用）。
*/
RHI::Texture* MainFrame::GetFinalRenderTarget()
{
	return _finalRenderTarget ? _finalRenderTarget->GetTexture() : nullptr;
}
