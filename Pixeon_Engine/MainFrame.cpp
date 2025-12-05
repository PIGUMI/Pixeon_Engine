#include "MainFrame.h"
#include "System.h"
#include "GameRenderTarget.h"
#include "GUI.h"
// アセット管理クラス
#include "AssetManager.h"
#include "ModelManager.h"
#include "TextureManager.h"
#include "SettingManager.h"
#include "ShaderManager.h"
#include "ComponentManager.h"
#include "ScriptManager.h"
#include "ResourceService.h"
// 入力処理
#include "Input.h"

// ソフトウェアモード
#include "EngineFrame.h"

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
	targetFrameTime_ = 1000.0f / 70.0f;
	lastUpdateTime_ = timeGetTime();
	m_hWnd_ = InPut.wnd;
	bUpdateDraw = false;;

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
	GUI::GetInstance()->Init();
	/* シェーダーマネージャーの初期化 */
	ShaderManager::GetInstance()->Initialize(DirectX11::GetInstance()->GetDevice());

	/* コンポーネントマネージャーの初期化 */
	ComponentManager::GetInstance()->Init();

	/* スクリプトマネージャーの初期化 */
	ScriptManager::Instance().RegisterAllScripts();

	/* 入力初期化 */
	InitInput();

	EngineFrame::GetInstance()->Init();

	return 0;
}

void MainFrame::Update()
{
	// フレーム制御
	DWORD currentTime = timeGetTime();
	float deltaTime = static_cast<float>(currentTime - lastUpdateTime_);

	if (deltaTime >= targetFrameTime_) {
		// deltaTime を秒単位に変換
		deltaTime_ = deltaTime * 0.001f; // ms -> s
		// 入力更新
		UpdateInput(GetWindowHandle());
		// ソフトウェアモードごとの更新処理
		switch (softwareMode_)
		{
		case SoftWareMode::ENGINE:
			break;
		case SoftWareMode::ANIMTOR2D:
			break;
		default:
			break;
		}

		// 更新時間記録
		lastUpdateTime_ = currentTime;
		bUpdateDraw = true;
	}
}

void MainFrame::Draw() 
{
	if (bUpdateDraw) {
		m_gameRenderTarget_->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());

		m_gameRenderTarget_->Begin(DirectX11::GetInstance()->GetContext());

		// ソフトウェアモードごとの描画処理
		switch (softwareMode_)
		{
		case SoftWareMode::ENGINE:
			break;
		case SoftWareMode::ANIMTOR2D:
			break;
		default:
			break;
		}

		m_gameRenderTarget_->End();

		ID3D11DeviceContext* ctx = DirectX11::GetInstance()->GetContext();
		ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
		ctx->PSSetShaderResources(0, 1, nullSRV);

		DirectX11::GetInstance()->BeginDraw();
		GUI::GetInstance()->BeginDraw();
		// ソフトウェアモードごとの描画処理
		switch (softwareMode_)
		{
		case SoftWareMode::ENGINE:
			break;
		case SoftWareMode::ANIMTOR2D:
			break;
		default:
			break;
		}
		GUI::GetInstance()->EndDraw();
		DirectX11::GetInstance()->EndDraw();

		bUpdateDraw = false;
	}
}

void MainFrame::UnInit() {

	EngineFrame::GetInstance()->UnInit();
	UninitInput();
	// AssetManager の自動同期停止
	AssetManager::Instance()->StopAutoSync();
	// 保存
	SettingManager::GetInstance()->SaveConfig();
	// 破棄処理
	// マネージャーの破棄
	GUI::DestroyInstance();
	AssetManager::DeleteInstance();
	ComponentManager::DestroyInstance();
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


