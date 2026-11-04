#include "GBuffer.h"
#include "System.h"

// ============================================================
// Init
// ============================================================
bool GBuffer::Init(int width, int height)
{
	if (width <= 0 || height <= 0) return false;
	return CreateRenderTargets(width, height);
}

// ============================================================
// Release
// ============================================================
void GBuffer::Release()
{
	m_cls_Albedo.reset();
	m_cls_Normal.reset();
	m_cls_Depth.reset();
	m_i_Width = 0;
	m_i_Height = 0;
}

// ============================================================
// Resize
// ============================================================
bool GBuffer::Resize(int width, int height)
{
	Release();
	return CreateRenderTargets(width, height);
}

// ============================================================
// BeginGeometryPass
// 2枚のRTと深度バッファをセット、クリアして描画準備
// ============================================================
void GBuffer::BeginGeometryPass()
{
	GraphicsSystem* gs = GraphicsSystem::GetInstance();
	RHI::Device* dev = gs->GetDevice();
	RHI::Texture* rts[] = { m_cls_Albedo.get(), m_cls_Normal.get() };
	gs->RenderTargetsSet(2, rts, m_cls_Depth.get());

	// Albedo クリア（黒・不透明）
	const float clearAlbedo[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	dev->RenderTargetClear(m_cls_Albedo.get(), clearAlbedo);

	// Normal クリア（0, 0, 0）
	const float clearNormal[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	dev->RenderTargetClear(m_cls_Normal.get(), clearNormal);

	// 深度クリア
	dev->DepthStencilClear(m_cls_Depth.get(), true, true, 1.0f, 0);
}

// ============================================================
// EndGeometryPass
// RTを外す（この後SRVとして読み取れる）
// ============================================================
void GBuffer::EndGeometryPass()
{
	GraphicsSystem::GetInstance()->GetDevice()->RenderTargetsSet(0, nullptr, nullptr);
}

// ============================================================
// CreateRenderTargets  内部生成ヘルパー
// ============================================================
bool GBuffer::CreateRenderTargets(int width, int height)
{
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	if (!dev) return false;
	m_i_Width = width;
	m_i_Height = height;

	RHI::TextureDesc td;
	td.i_Width = (uint32_t)width;
	td.i_Height = (uint32_t)height;
	td.i_BindFlags = RHI::Bind_RenderTarget | RHI::Bind_ShaderResource;

	// RT0: Albedo  (RGBA8_UNORM)
	td.e_Format = RHI::Format::R8G8B8A8_UNorm;
	td.c_DebugName = "GBuffer.Albedo";
	m_cls_Albedo = dev->TextureCreate(td, nullptr);

	// RT1: Normal  (RGBA16F) 16bit float にすることで法線の精度を確保
	td.e_Format = RHI::Format::R16G16B16A16_Float;
	td.c_DebugName = "GBuffer.Normal";
	m_cls_Normal = dev->TextureCreate(td, nullptr);

	// Depth: DSV と SRV の両方で使う
	td.e_Format = RHI::Format::D24_UNorm_S8_UInt;
	td.i_BindFlags = RHI::Bind_DepthStencil | RHI::Bind_ShaderResource;
	td.c_DebugName = "GBuffer.Depth";
	m_cls_Depth = dev->TextureCreate(td, nullptr);

	return m_cls_Albedo && m_cls_Normal && m_cls_Depth;
}
