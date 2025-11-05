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

EngineManager* EngineManager::instance_ = nullptr;

EngineManager* EngineManager::GetInstance(){
	if (instance_ == nullptr){
		instance_ = new EngineManager();
	}
	return instance_;
}

void EngineManager::DeleteInstance(){
	if (instance_ != nullptr){
		delete instance_;
		instance_ = nullptr;
	}
}

int EngineManager::Init(const EngineConfig& InPut){

	m_bInGame_		= false;
	m_bIsShowGUI_	= true;

	// COM の初期化
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr)) return -1;
	m_hWnd_ = InPut.wnd;
	// DirectX11 初期化
	hr = DirectX11::GetInstance()->Init(InPut.wnd, InPut.screenWidth, InPut.screenHeight, InPut.fullscreen);
	if (FAILED(hr)) {
		CoUninitialize();
		return -1;
	}
	// 設定読み込み
	SettingManager::GetInstance()->LoadConfig();
	// AssetManager 初期化
	AssetManager::Instance()->SetRoot(SettingManager::GetInstance()->GetAssetsFilePath());
	AssetManager::Instance()->SetLoadMode(AssetManager::LoadMode::FromSource);
	AssetManager::Instance()->StartAutoSync(std::chrono::milliseconds(1000), true);
	// エンジン用レンダーテクスチャ初期化
	m_gameRenderTarget_ = new GameRenderTarget();
	m_gameRenderTarget_->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);
	m_gameRenderTarget_->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());
	// GUI 初期化
	EditrGUI::GetInstance()->Init();
	// シーンマネージャー初期化
	SceneManger::GetInstance()->Init();
	SceneManger::GetInstance()->Load();
	// シェーダー初期化
	ShaderManager::GetInstance()->Initialize(DirectX11::GetInstance()->GetDevice());
	// コンポーネント初期化
	ComponentManager::GetInstance()->Init();
	// スクリプトコンポーネントの初期化
	ScriptManager::Instance();
	InitInput();
	return 0;
}

void EngineManager::Update() {
	UpdateInput(GetWindowHandle());
	if (m_bInGame_)
		InGameUpdate();
	else
		EditeUpdate();
}

void EngineManager::Draw() {
	m_gameRenderTarget_->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());
	if (!m_bIsShowGUI_)
		InGameDraw();
	else
		EditeDraw();	
}

void EngineManager::UnInit() {
	UninitInput();
	// AssetManager の自動同期停止
	AssetManager::Instance()->StopAutoSync();
	// 保存
	SceneManger::GetInstance()->Save();
	SettingManager::GetInstance()->SaveConfig();
	// 破棄処理
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

ID3D11ShaderResourceView* EngineManager::GetGameRender(){
	return m_gameRenderTarget_->GetShaderResourceView();
}

void EngineManager::EditeUpdate() {
	ShaderManager::GetInstance()->UpdateAndCompileShaders();
	EditrGUI::GetInstance()->Update();
	SceneManger::GetInstance()->EditUpdate();
}

void EngineManager::InGameUpdate() {
	SceneManger::GetInstance()->PlayUpdate();
}

void EngineManager::EditeDraw() {
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