#include "LightingPass.h"
#include "GBuffer.h"
#include "System.h"
#include "ShaderManager.h"

using namespace DirectX;

// ============================================================
// Init
// ============================================================
bool LightingPass::Init()
{
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	if (!dev) return false;

	// ---- 定数バッファ ----
	RHI::BufferDesc bd;
	bd.i_Size = sizeof(LightingCB);
	bd.i_BindFlags = RHI::Bind_ConstantBuffer;
	m_cls_LightingCB = dev->BufferCreate(bd);

	// ---- PointClamp サンプラー（GBuffer 読み取り用）----
	RHI::SamplerDesc sd;
	sd.e_Filter = RHI::Filter::Point;
	sd.e_AddressU = sd.e_AddressV = sd.e_AddressW = RHI::AddressMode::Clamp;
	sd.e_Compare = RHI::CompareFunc::Always;
	m_cls_PointClampSmp = dev->SamplerCreate(sd);

	// ---- シャドウサンプラー（Comparison Sampler）----
	sd.e_Filter = RHI::Filter::ComparisonMinMagLinearMipPoint;
	sd.e_Compare = RHI::CompareFunc::LessEqual;
	m_cls_ShadowSmp = dev->SamplerCreate(sd);

	m_b_Initialized = m_cls_LightingCB && m_cls_PointClampSmp && m_cls_ShadowSmp;
	return m_b_Initialized;
}

// ============================================================
// Release
// ============================================================
void LightingPass::Release()
{
	m_cls_LightingCB.reset();
	m_cls_PointClampSmp.reset();
	m_cls_ShadowSmp.reset();
	m_cls_SSAO = nullptr;
	m_b_Initialized = false;
}

// ============================================================
// Execute
// GBuffer を読んで現在のレンダーターゲットにライティング結果を書き込む
// ============================================================
void LightingPass::Execute(
	GBuffer* gbuffer,
	RHI::Texture* shadowMap,
	int width, int height,
	const XMMATRIX& proj,
	const XMMATRIX& view,
	const XMFLOAT3& cameraPos)
{
	if (!m_b_Initialized || !gbuffer) return;

	GraphicsSystem* gs = GraphicsSystem::GetInstance();
	RHI::Device* dev = gs->GetDevice();
	auto* sm = ShaderManager::GetInstance();

	// シェーダー取得
	RHI::Shader* vs = sm->GetVertexShader("VS_Fullscreen");
	RHI::Shader* ps = sm->GetPixelShader("PS_Lighting");
	if (!vs || !ps) return;

	// ---- 定数バッファ更新 ----
	LightingCB cb = {};
	cb.invProj = XMMatrixTranspose(XMMatrixInverse(nullptr, proj));
	cb.invView = XMMatrixTranspose(XMMatrixInverse(nullptr, view));
	cb.view = XMMatrixTranspose(view);
	cb.cameraPos = cameraPos;
	cb.resolution = XMFLOAT2((float)width, (float)height);
	cb.aoStrength = m_f_AOStrength;
	dev->BufferUpdate(m_cls_LightingCB.get(), &cb, sizeof(cb));

	// ---- 深度書き込みOFF（フルスクリーン描画では深度不要）----
	gs->SetDepthMode(DEPTH_OFF);

	// ---- ビューポート ----
	RHI::Viewport vp;
	vp.f_Width = (float)width;
	vp.f_Height = (float)height;
	dev->ViewportSet(vp);

	// ---- シェーダー・定数バッファ ----
	dev->ShaderSet(RHI::ShaderStage::Vertex, vs);
	dev->ShaderSet(RHI::ShaderStage::Pixel, ps);
	dev->ConstantBufferSet(RHI::ShaderStage::Pixel, 0, m_cls_LightingCB.get());
	// b1/b2: ライトCBを再セット（GeometryPassでPSが切り替わった後も確実に反映）
	if (m_cls_LightArrayCB) dev->ConstantBufferSet(RHI::ShaderStage::Pixel, 1, m_cls_LightArrayCB);
	if (m_cls_LightCountCB) dev->ConstantBufferSet(RHI::ShaderStage::Pixel, 2, m_cls_LightCountCB);

	// ---- テクスチャ ----
	// t0=Albedo, t1=Normal, t2=Depth, t3=SSAO, t4=ShadowMap
	dev->TextureSet(RHI::ShaderStage::Pixel, 0, gbuffer->GetAlbedo());
	dev->TextureSet(RHI::ShaderStage::Pixel, 1, gbuffer->GetNormal());
	dev->TextureSet(RHI::ShaderStage::Pixel, 2, gbuffer->GetDepth());
	dev->TextureSet(RHI::ShaderStage::Pixel, 3, m_cls_SSAO ? m_cls_SSAO : gs->GetWhiteTexture());
	dev->TextureSet(RHI::ShaderStage::Pixel, 4, shadowMap);

	// ---- サンプラー ----
	dev->SamplerSet(RHI::ShaderStage::Pixel, 0, m_cls_PointClampSmp.get());  // s0: GBuffer 読み取り
	dev->SamplerSet(RHI::ShaderStage::Pixel, 1, m_cls_ShadowSmp.get());      // s1: シャドウマップ

	// ---- フルスクリーン三角形描画（頂点バッファ不要）----
	dev->InputLayoutSet(nullptr);
	dev->TopologySet(RHI::Topology::TriangleList);
	dev->Draw(3, 0);

	// ---- テクスチャのバインド解除（次フレームで RT として使えるように）----
	for (uint32_t i = 0; i < 5; ++i) dev->TextureSet(RHI::ShaderStage::Pixel, i, nullptr);

	// ---- 深度ステートを戻す ----
	dev->DepthStencilStateSet(nullptr);
}
