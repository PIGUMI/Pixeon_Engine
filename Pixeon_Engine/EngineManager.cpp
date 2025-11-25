#include "EngineManager.h"
#include "System.h"
#include "GameRenderTarget.h"
#include "EditrGUI.h"
#include "PostEffectBase.h"
#include "ModelManager.h"
#include "TextureManager.h"
#include "ResourceService.h"
#include "AssetManager.h"
#include "SceneManger.h"
#include "SettingManager.h"
#include "ShaderManager.h"
#include "ComponentManager.h"
#include "ScriptManager.h"
#include "Object.h"
#include "Input.h"
#include <chrono>

EngineManager* EngineManager::instance_ = nullptr;

EngineManager* EngineManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new EngineManager();
	}
	return instance_;
}

void EngineManager::DeleteInstance() {
	if (instance_ != nullptr) {
		delete instance_;
		instance_ = nullptr;
	}
}

int EngineManager::Init(const EngineConfig& InPut)
{
	/* メンバー変数の初期化 */
	m_bInGame_ = false;
	m_bIsShowGUI_ = false;
	targetFrameTime_ = 1000.0f / 60.0f;
	lastDrawTime_ = timeGetTime();
	lastUpdateTime_ = timeGetTime();
	m_hWnd_ = InPut.wnd;

	/* 設定の読み込み */
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

	/* AssetManager 初期化 */
	// AssetManager のルートパス設定
	AssetManager::Instance()->SetRoot(SettingManager::GetInstance()->GetAssetsFilePath());
	// AssetManager のロードモード設定
	AssetManager::Instance()->SetLoadMode(AssetManager::LoadMode::FromSource);
	// AssetManager の自動同期開始
	AssetManager::Instance()->StartAutoSync(std::chrono::milliseconds(1000), true);

	/* エンジン用レンダーテクスチャ初期化 */
	// ゲーム用レンダーテクスチャ初期化
	m_gameRenderTarget_ = new GameRenderTarget();
	// ゲーム用レンダーテクスチャ初期化
	m_gameRenderTarget_->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);
	// Zバッファ設定
	m_gameRenderTarget_->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());

	/* GUIの初期化 */
	EditrGUI::GetInstance()->Init();

	/* シーンマネージャーの初期化 */
	SceneManger::GetInstance()->Init();
	// 設定の読み込み
	SceneManger::GetInstance()->Load();
	
	/* シェーダーマネージャーの初期化 */
	ShaderManager::GetInstance()->Initialize(DirectX11::GetInstance()->GetDevice());
	
	/* コンポーネントマネージャーの初期化 */
	ComponentManager::GetInstance()->Init();

	/* スクリプトマネージャーの初期化 */
	ScriptManager::Instance().RegisterAllScripts();

	/* 入力初期化 */
	InitInput();

	return 0;
}

void EngineManager::Update() 
{
	// フレーム制御
	DWORD currentTime = timeGetTime();
	float deltaTime = static_cast<float>(currentTime - lastUpdateTime_);
	if (deltaTime >= targetFrameTime_) {
		deltaTime_ = deltaTime;
		UpdateInput(GetWindowHandle());
		if (IsKeyPress(VK_SHIFT) && IsKeyTrigger(VK_RETURN))m_bIsShowGUI_ = !m_bIsShowGUI_;
		if (m_bInGame_)
			InGameUpdate();
		else
			EditorUpdate();
		lastUpdateTime_ = currentTime;
	}
}

void EngineManager::Draw() {
	DWORD currentTime = timeGetTime();
	float deltaTime = static_cast<float>(currentTime - lastDrawTime_);
	if (deltaTime >= targetFrameTime_) {
		m_gameRenderTarget_->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());
		EditorDraw();
		lastDrawTime_ = currentTime;
	}
}

void EngineManager::UnInit() {
	UninitInput();
	// AssetManager の自動同期停止
	AssetManager::Instance()->StopAutoSync();
	// 保存
	SceneManger::GetInstance()->Save();
	SettingManager::GetInstance()->SaveConfig();
	// 破棄処理
	EditrGUI::DestroyInstance();
	AssetManager::DeleteInstance();
	ComponentManager::DestroyInstance();
	SceneManger::DestroyInstance();
	SettingManager::DestroyInstance();
	ShaderManager::DestroyInstance();
	TextureManager::DeleteInstance();
	ModelManager::DeleteInstance();
	ResourceService::DeleteInstance();
	ScriptManager::Release();
	DirectX11::GetInstance()->Uninit();
	DirectX11::DestroyInstance();
	CoUninitialize();
}

ID3D11ShaderResourceView* EngineManager::GetGameRender() {
	return m_gameRenderTarget_->GetShaderResourceView();
}

void EngineManager::EditorUpdate() {
	ScriptManager::Instance().Update();
	ShaderManager::GetInstance()->UpdateAndCompileShaders();
	EditrGUI::GetInstance()->Update();
	SceneManger::GetInstance()->EditUpdate();
	m_bIsBeginPlayCalled = false;
}

void EngineManager::InGameUpdate() {
	if (!m_bIsBeginPlayCalled) {
		SceneManger::GetInstance()->BeginPlay();
		m_bIsBeginPlayCalled = true;
	}
	SceneManger::GetInstance()->PlayUpdate();
}

void EngineManager::EditorDraw() {
	m_gameRenderTarget_->Begin(DirectX11::GetInstance()->GetContext());
	SceneManger::GetInstance()->Draw();
	m_gameRenderTarget_->End();

	ID3D11DeviceContext* ctx = DirectX11::GetInstance()->GetContext();
	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	ctx->PSSetShaderResources(0, 1, nullSRV);

	DirectX11::GetInstance()->BeginDraw();
	EditrGUI::GetInstance()->Draw();
	DirectX11::GetInstance()->EndDraw();
}

void EngineManager::InGameDraw() {
	DirectX11::GetInstance()->BeginDraw();
	SceneManger::GetInstance()->Draw();
	DirectX11::GetInstance()->EndDraw();
}
