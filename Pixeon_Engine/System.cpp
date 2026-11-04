/*
* ファイル名　System
* 説　　　明　描画システム（RHI デバイスと共通ステートの管理）
*/
#include "System.h"

GraphicsSystem* GraphicsSystem::m_cls_Instance = nullptr;

namespace {
	RHI::Backend BackendConvert(GraphicsBackend backend)
	{
		switch (backend) {
		case GraphicsBackend::D3D12:  return RHI::Backend::D3D12;
		case GraphicsBackend::Vulkan: return RHI::Backend::Vulkan;
		default:                      return RHI::Backend::D3D11;
		}
	}
}

/*
* 関数名　GetInstance
* 引　数　なし
* 戻り値　インスタンス
* 説　明　シングルトンインスタンスを取得する
*/
GraphicsSystem* GraphicsSystem::GetInstance()
{
	if (m_cls_Instance == nullptr) m_cls_Instance = new GraphicsSystem();
	return m_cls_Instance;
}

void GraphicsSystem::DestroyInstance()
{
	if (m_cls_Instance != nullptr) {
		m_cls_Instance->Uninit();
		delete m_cls_Instance;
		m_cls_Instance = nullptr;
	}
}

/*
* 関数名　Init
* 引　数　hWnd：ウィンドウ　width / height：サイズ　fullScreen / vsync：表示設定
* 　　　　backend：描画 API　out_error：エラー内容
* 戻り値　成功なら true
* 説　明　RHI デバイスを生成し、共通ステートと既定の深度バッファを用意する
*         指定のバックエンドが使えない場合は D3D11 で再試行する
*/
bool GraphicsSystem::Init(HWND hWnd, UINT width, UINT height, bool fullScreen, bool vsync, GraphicsBackend backend, std::string* out_error)
{
	RHI::DeviceCreateInfo info;
	info.p_WindowHandle = hWnd;
	info.i_Width = width;
	info.i_Height = height;
	info.b_Fullscreen = fullScreen;
	info.b_VSync = vsync;
#if defined(_DEBUG)
	info.b_Debug = true;
#endif
	std::string error;
	m_cls_Device = RHI::DeviceCreate(BackendConvert(backend), info, &error);
	if (!m_cls_Device && backend != GraphicsBackend::D3D11) {
		RHI::LogWrite("%s の初期化に失敗したため DirectX 11 で起動します: %s", GraphicsBackendUtil::NameGet(backend), error.c_str());
		m_cls_Device = RHI::DeviceCreate(RHI::Backend::D3D11, info, &error);
	}
	if (!m_cls_Device) {
		if (out_error) *out_error = error;
		return false;
	}
	RHI::DeviceSet(m_cls_Device.get());
	m_i_Width = width;
	m_i_Height = height;

	if (!StatesCreate() || !DefaultTargetsCreate(width, height)) {
		if (out_error) *out_error = "共通ステートの作成に失敗しました";
		return false;
	}
	m_cls_ImGui = RHI::ImGuiRendererCreate(m_cls_Device.get());

	SetCullingMode(CULL_BACK);
	SetBlendMode(BLEND_ALPHA);
	SetSamplerState(SAMPLER_LINEAR);
	BackBufferTargetSet(false);
	return true;
}

/*
* 関数名　StatesCreate
* 引　数　なし
* 戻り値　成功なら true
* 説　明　旧 DirectX11 クラスと同じ組み合わせのステートを作成する
*/
bool GraphicsSystem::StatesCreate()
{
	RHI::Device* dev = m_cls_Device.get();
	const RHI::CullMode culls[CULL_MAX] = { RHI::CullMode::None, RHI::CullMode::Front, RHI::CullMode::Back };
	for (int i = 0; i < CULL_MAX; ++i) {
		RHI::RasterizerDesc rd;
		rd.e_Cull = culls[i];
		m_RasterizerStates[i] = dev->RasterizerStateCreate(rd);
	}

	using RHI::Blend;
	const Blend blends[BLEND_MAX][2] = {
		{ Blend::One, Blend::Zero },
		{ Blend::SrcAlpha, Blend::InvSrcAlpha },
		{ Blend::One, Blend::One },
		{ Blend::SrcAlpha, Blend::One },
		{ Blend::Zero, Blend::InvSrcColor },
		{ Blend::InvDestColor, Blend::One },
		{ Blend::One, Blend::InvSrcAlpha },
		{ Blend::One, Blend::One },
	};
	for (int i = 0; i < BLEND_MAX; ++i) {
		RHI::BlendDesc bd;
		auto& rt = bd.renderTarget[0];
		rt.b_BlendEnable = true;
		rt.e_SrcBlend = blends[i][0];
		rt.e_DestBlend = blends[i][1];
		rt.e_SrcBlendAlpha = Blend::One;
		rt.e_DestBlendAlpha = (i == BLEND_ADD_ALL) ? Blend::One : Blend::InvSrcAlpha;
		m_BlendStates[i] = dev->BlendStateCreate(bd);
	}

	struct SamplerPreset { RHI::Filter filter; RHI::AddressMode address; };
	const SamplerPreset presets[SAMPLER_MAX] = {
		{ RHI::Filter::Linear, RHI::AddressMode::Wrap },
		{ RHI::Filter::Point, RHI::AddressMode::Wrap },
		{ RHI::Filter::Linear, RHI::AddressMode::Clamp },
		{ RHI::Filter::Point, RHI::AddressMode::Clamp },
		{ RHI::Filter::ComparisonMinMagLinearMipPoint, RHI::AddressMode::Border },
	};
	for (int i = 0; i < SAMPLER_MAX; ++i) {
		RHI::SamplerDesc sd;
		sd.e_Filter = presets[i].filter;
		sd.e_AddressU = sd.e_AddressV = sd.e_AddressW = presets[i].address;
		if (i == SAMPLER_SHADOW) {
			sd.e_Compare = RHI::CompareFunc::LessEqual;
			for (float& c : sd.f_BorderColor) c = 1.0f;
		}
		m_Samplers[i] = dev->SamplerCreate(sd);
	}

	RHI::DepthStencilDesc dd;
	m_DepthStates[DEPTH_DEFAULT] = dev->DepthStencilStateCreate(dd);
	dd.b_DepthEnable = false;
	dd.b_DepthWrite = false;
	m_DepthStates[DEPTH_OFF] = dev->DepthStencilStateCreate(dd);
	dd.b_DepthEnable = true;
	dd.e_DepthFunc = RHI::CompareFunc::LessEqual;
	m_DepthStates[DEPTH_READ_ONLY] = dev->DepthStencilStateCreate(dd);
	dd.b_DepthWrite = true;
	m_DepthStates[DEPTH_LESS_EQUAL] = dev->DepthStencilStateCreate(dd);

	const uint32_t white = 0xFFFFFFFFu;
	RHI::TextureDesc td;
	RHI::SubresourceData init{ &white, 4, 4 };
	m_cls_WhiteTexture = dev->TextureCreate(td, &init);

	for (auto& s : m_RasterizerStates) if (!s) return false;
	for (auto& s : m_BlendStates) if (!s) return false;
	for (auto& s : m_Samplers) if (!s) return false;
	for (auto& s : m_DepthStates) if (!s) return false;
	return m_cls_WhiteTexture != nullptr;
}

/*
* 関数名　DefaultTargetsCreate
* 引　数　width / height：サイズ
* 戻り値　成功なら true
* 説　明　バックバッファと組で使う深度バッファ（シェーダーからも読める）を作る
*/
bool GraphicsSystem::DefaultTargetsCreate(UINT width, UINT height)
{
	RHI::TextureDesc td;
	td.i_Width = width;
	td.i_Height = height;
	td.e_Format = RHI::Format::D32_Float;
	td.i_BindFlags = RHI::Bind_DepthStencil | RHI::Bind_ShaderResource;
	td.c_DebugName = "DefaultDepth";
	m_cls_DefaultDepth = m_cls_Device->TextureCreate(td, nullptr);
	return m_cls_DefaultDepth != nullptr;
}

/*
* 関数名　Uninit
* 引　数　なし
* 戻り値　なし
* 説　明　GPU の処理完了を待ってから解放する
*/
void GraphicsSystem::Uninit()
{
	if (!m_cls_Device) return;
	m_cls_Device->IdleWait();
	m_cls_Device->StateClear();
	m_cls_ImGui.reset();
	m_cls_DefaultDepth.reset();
	m_cls_WhiteTexture.reset();
	for (auto& s : m_RasterizerStates) s.reset();
	for (auto& s : m_BlendStates) s.reset();
	for (auto& s : m_Samplers) s.reset();
	for (auto& s : m_DepthStates) s.reset();
	RHI::DeviceSet(nullptr);
	m_cls_Device.reset();
}

/*
* 関数名　BeginDraw
* 引　数　なし
* 戻り値　なし
* 説　明　バックバッファを描画先にしてクリアする
*/
void GraphicsSystem::BeginDraw()
{
	const float color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
	m_cls_Device->RenderTargetClear(GetBackBuffer(), color);
	m_cls_Device->DepthStencilClear(m_cls_DefaultDepth.get(), true, true, 1.0f, 0);
	BackBufferTargetSet(false);
}

void GraphicsSystem::EndDraw()
{
	m_cls_Device->Present();
}

/*
* 関数名　OnResize
* 引　数　width / height：新しいクライアントサイズ
* 戻り値　なし
* 説　明　スワップチェーンと既定の深度バッファを作り直す
*/
void GraphicsSystem::OnResize(UINT width, UINT height)
{
	if (!m_cls_Device || width == 0 || height == 0) return;
	if (m_cls_ImGui) m_cls_ImGui->DeviceObjectsInvalidate();
	m_cls_Device->IdleWait();
	m_cls_Device->SwapChainResize(width, height);
	m_i_Width = width;
	m_i_Height = height;
	DefaultTargetsCreate(width, height);
	BackBufferTargetSet(true);
	if (m_cls_ImGui) m_cls_ImGui->DeviceObjectsCreate();
}

void GraphicsSystem::BackBufferTargetSet(bool with_depth)
{
	RHI::Texture* bb = GetBackBuffer();
	RenderTargetsSet(1, &bb, with_depth ? m_cls_DefaultDepth.get() : nullptr);
}

/*
* 関数名　RenderTargetsSet
* 引　数　num / colors：カラーターゲット　depth：深度ターゲット
* 戻り値　なし
* 説　明　描画先を切り替え、先頭ターゲットのサイズでビューポートを設定する
*/
void GraphicsSystem::RenderTargetsSet(UINT num, RHI::Texture* const* colors, RHI::Texture* depth)
{
	m_cls_Device->RenderTargetsSet(num, colors, depth);
	RHI::Texture* ref = (num > 0 && colors && colors[0]) ? colors[0] : depth;
	if (!ref) return;
	RHI::Viewport vp;
	vp.f_Width = (float)ref->GetWidth();
	vp.f_Height = (float)ref->GetHeight();
	m_cls_Device->ViewportSet(vp);
}

void GraphicsSystem::SetCullingMode(CullMode cull)
{
	if (cull < 0 || cull >= CULL_MAX) return;
	m_cls_Device->RasterizerStateSet(m_RasterizerStates[cull].get());
}

void GraphicsSystem::SetBlendMode(BlendMode blend)
{
	if (blend < 0 || blend >= BLEND_MAX) return;
	m_cls_Device->BlendStateSet(m_BlendStates[blend].get());
}

void GraphicsSystem::SetSamplerState(SamplerState state, UINT slot, RHI::ShaderStage stage)
{
	if (state < 0 || state >= SAMPLER_MAX) return;
	m_cls_Device->SamplerSet(stage, slot, m_Samplers[state].get());
}

void GraphicsSystem::SetDepthMode(DepthMode mode)
{
	if (mode < 0 || mode >= DEPTH_MAX) return;
	m_cls_Device->DepthStencilStateSet(m_DepthStates[mode].get());
}
