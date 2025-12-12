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
#include "SoundManager.h"
#include "ComponentManager.h"
#include "ScriptManager.h"
#include "ResourceService.h"
#include "Animator2DManager.h"
// 入力処理
#include "Input.h"
// ソフトウェアモード
#include "EngineFrame.h"
#include "Animator2DFrame.h"

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
	// UI用レンダーテクスチャ初期化
	m_uiRenderTarget_ = new GameRenderTarget();
	// UI用レンダーテクスチャ初期化
	m_uiRenderTarget_->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);
	// Zバッファ設定
	m_uiRenderTarget_->SetRenderZBuffer(false);
	m_finalRenderTarget_ = new GameRenderTarget();
	m_finalRenderTarget_->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);
	m_finalRenderTarget_->SetRenderZBuffer(false);

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
	Animator2DFrame::GetInstance()->Init();

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
			EngineFrame::GetInstance()->Update();
			break;
		case SoftWareMode::ANIMTOR2D:
			Animator2DFrame::GetInstance()->Update();
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
			EngineFrame::GetInstance()->Draw(0);
			break;
		case SoftWareMode::ANIMTOR2D:
			Animator2DFrame::GetInstance()->Draw();
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

		bUpdateDraw = false;
	}
}

void MainFrame::UnInit() {
	Animator2DFrame::GetInstance()->UnInit();
	EngineFrame::GetInstance()->UnInit();

	EngineFrame::DestroyInstance();
	Animator2DFrame::DestroyInstance();

	UninitInput();
	// AssetManager の自動同期停止
	AssetManager::Instance()->StopAutoSync();
	// 保存
	SettingManager::GetInstance()->SaveConfig();
	// 破棄処理
	// マネージャーの破棄
	GUI::DestroyInstance();
	ComponentManager::DestroyInstance();
	SettingManager::DestroyInstance();
	ScriptManager::Release();
	ShaderManager::DestroyInstance();
	Animator2DManager::GetInstance()->ResetAllAnimator2D();
	AssetManager::DeleteInstance();
	ModelManager::DeleteInstance();
	TextureManager::DeleteInstance();
	SoundManager::DeleteInstance();

	if(m_gameRenderTarget_){
		delete m_gameRenderTarget_;
		m_gameRenderTarget_ = nullptr;
	}
	if (m_uiRenderTarget_){
		delete m_uiRenderTarget_;
		m_uiRenderTarget_ = nullptr;
	}
	ResourceService::DeleteInstance();

	DirectX11::GetInstance()->Uninit();
	DirectX11::DestroyInstance();
	CoUninitialize();
}

ID3D11ShaderResourceView* MainFrame::GetGameRenderTargetSRV()
{
	return m_gameRenderTarget_->GetShaderResourceView();
}

ID3D11ShaderResourceView* MainFrame::GetFinalRenderTargetSRV()
{
	if (m_finalRenderTarget_) {
		return m_finalRenderTarget_->GetShaderResourceView();
	}
	return nullptr;
}

void MainFrame::CompositePass(ID3D11ShaderResourceView* sceneSRV, ID3D11ShaderResourceView* uiSRV, GameRenderTarget* finalRT)
{
	if (!finalRT) {
		OutputDebugStringA("[CompositePass] finalRT is null\n");
		return;
	}
	auto dev = DirectX11::GetInstance()->GetDevice();
	auto ctx = DirectX11::GetInstance()->GetContext();
	if (!dev || !ctx) {
		OutputDebugStringA("[CompositePass] device/context null\n");
		return;
	}

	// デバッグログ
	if (!sceneSRV) OutputDebugStringA("[CompositePass] sceneSRV is null\n");
	if (!uiSRV)    OutputDebugStringA("[CompositePass] uiSRV is null\n");

	ShaderManager* sm = ShaderManager::GetInstance();
	const std::string vsName = "VS_ImgQuad";
	const std::string psName = "PS_ImgQuad";

	ID3D11VertexShader* vs = sm->GetVertexShader(vsName);
	ID3D11PixelShader* ps = sm->GetPixelShader(psName);
	if (!vs || !ps) {
		// シェーダが無ければ再コンパイルを試みる
		sm->UpdateAndCompileShaders();
		vs = sm->GetVertexShader(vsName);
		ps = sm->GetPixelShader(psName);
		if (!vs || !ps) {
			OutputDebugStringA("[CompositePass] required shaders missing (VS_ImgQuad/PS_ImgQuad)\n");
			return;
		}
	}

	// static リソース（一度だけ作る）
	static ID3D11Buffer* s_vb = nullptr;
	static ID3D11InputLayout* s_layout = nullptr;
	static ID3D11Buffer* s_cb = nullptr;
	static ID3D11DepthStencilState* s_dsOff = nullptr;

	if (!s_vb) {
		struct V { float p[3]; float uv[2]; };
		V verts[6] = {
			{ {-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f} },
			{ { 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f} },
			{ { 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f} },

			{ {-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f} },
			{ { 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f} },
			{ {-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f} },
		};
		D3D11_BUFFER_DESC bd{};
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.ByteWidth = sizeof(verts);
		bd.Usage = D3D11_USAGE_IMMUTABLE;
		D3D11_SUBRESOURCE_DATA init{ verts, 0, 0 };
		if (FAILED(dev->CreateBuffer(&bd, &init, &s_vb))) {
			OutputDebugStringA("[CompositePass] Create VB failed\n");
			return;
		}
	}

	if (!s_layout) {
		const void* bc = nullptr; size_t bcSize = 0;
		if (!sm->GetVSBytecode(vsName, &bc, &bcSize)) {
			OutputDebugStringA("[CompositePass] GetVSBytecode failed\n");
			return;
		}
		D3D11_INPUT_ELEMENT_DESC desc[] = {
			{ "POSITION",0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "TEXCOORD",0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA,0 },
		};
		if (FAILED(dev->CreateInputLayout(desc, _countof(desc), bc, bcSize, &s_layout))) {
			OutputDebugStringA("[CompositePass] CreateInputLayout failed\n");
			return;
		}
	}

	if (!s_cb) {
		struct CBVS { DirectX::XMMATRIX View; DirectX::XMMATRIX Proj; DirectX::XMFLOAT4 Color; int mode2D; float pad[3]; };
		D3D11_BUFFER_DESC cbd{};
		cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbd.ByteWidth = sizeof(CBVS);
		cbd.Usage = D3D11_USAGE_DEFAULT;
		if (FAILED(dev->CreateBuffer(&cbd, nullptr, &s_cb))) {
			OutputDebugStringA("[CompositePass] Create CB failed\n");
			return;
		}
	}

	if (!s_dsOff) {
		D3D11_DEPTH_STENCIL_DESC dsdesc{};
		dsdesc.DepthEnable = FALSE;
		dsdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsdesc.StencilEnable = FALSE;
		if (FAILED(dev->CreateDepthStencilState(&dsdesc, &s_dsOff))) {
			OutputDebugStringA("[CompositePass] Create DepthStencilState failed\n");
			return;
		}
	}

	// 合成先をバインドして描画（scene -> ui の順で描画）
	finalRT->Begin(ctx);

	// IA
	UINT stride = sizeof(float) * 5;
	UINT offset = 0;
	ctx->IASetVertexBuffers(0, 1, &s_vb, &stride, &offset);
	ctx->IASetInputLayout(s_layout);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// シェーダ
	ctx->VSSetShader(vs, nullptr, 0);
	ctx->PSSetShader(ps, nullptr, 0);

	// CB 更新（identity）
	struct CBData { DirectX::XMMATRIX V; DirectX::XMMATRIX P; DirectX::XMFLOAT4 Color; int mode2D; float pad[3]; } cb;
	cb.V = DirectX::XMMatrixIdentity();
	cb.P = DirectX::XMMatrixIdentity();
	cb.Color = DirectX::XMFLOAT4(1, 1, 1, 1);
	cb.mode2D = 1;
	ctx->UpdateSubresource(s_cb, 0, nullptr, &cb, 0, 0);
	ID3D11Buffer* cbs[] = { s_cb };
	ctx->VSSetConstantBuffers(0, 1, cbs);
	ctx->PSSetConstantBuffers(0, 1, cbs);

	// Depth 無効 / サンプラ / blend
	ctx->OMSetDepthStencilState(s_dsOff, 0);
	DirectX11::GetInstance()->SetSamplerState(SAMPLER_LINEAR);

	// 1) scene を描く（sceneSRV が無ければ目立つ色で塗る）
	if (sceneSRV) {
		ctx->PSSetShaderResources(0, 1, &sceneSRV);
		DirectX11::GetInstance()->SetBlendMode(BLEND_NONE);
		ctx->Draw(6, 0);
		ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
		ctx->PSSetShaderResources(0, 1, nullSRV);
	}
	else {
		// デバッグ用: sceneSRV が無ければマゼンタで塗る
		// 一時的に Color を変えて描く（完全に簡易的な手段）
		cb.Color = DirectX::XMFLOAT4(1, 0, 1, 1);
		cb.mode2D = 1;
		ctx->UpdateSubresource(s_cb, 0, nullptr, &cb, 0, 0);
		ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
		ctx->PSSetShaderResources(0, 1, nullSRV);
		DirectX11::GetInstance()->SetBlendMode(BLEND_NONE);
		ctx->Draw(6, 0);
		// 後で元に戻すカラー
		cb.Color = DirectX::XMFLOAT4(1, 1, 1, 1);
		ctx->UpdateSubresource(s_cb, 0, nullptr, &cb, 0, 0);
	}

	// 2) UI を alpha 合成で重ねる（uiSRV があれば）
	if (uiSRV) {
		ctx->PSSetShaderResources(0, 1, &uiSRV);
		DirectX11::GetInstance()->SetBlendMode(BLEND_ALPHA);
		ctx->Draw(6, 0);
		ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
		ctx->PSSetShaderResources(0, 1, nullSRV);
	}

	// 合成完了
	finalRT->End();

	// SRV を解除（念のため）
	ID3D11ShaderResourceView* nullSRV2[1] = { nullptr };
	ctx->PSSetShaderResources(0, 1, nullSRV2);
	// ブレンドはデフォルトに戻しておく（Editor の既存コードが BLEND_ALPHA を期待しているので戻す）
	DirectX11::GetInstance()->SetBlendMode(BLEND_ALPHA);

	OutputDebugStringA("[CompositePass] completed\n");
}