/*
* ファイル名　GameRenderTarget
* 説　　　明　レイヤーごとのオフスクリーン描画先の実装
*/
#include "GameRenderTarget.h"
#include "System.h"
#include "SettingManager.h"

/*
* 関数名　TargetsCreate
* 引　数　width / height：サイズ　depthReadable：深度を SRV でも使うか
* 戻り値　成功なら true
* 説　明　RGBA8 のカラーと D24S8 の深度を作成する
*/
bool GameRenderTarget::TargetsCreate(int width, int height, bool depthReadable)
{
	if (width <= 0 || height <= 0) return false;
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	if (!dev) return false;

	RHI::TextureDesc color;
	color.i_Width = (uint32_t)width;
	color.i_Height = (uint32_t)height;
	color.e_Format = RHI::Format::R8G8B8A8_UNorm;
	color.i_BindFlags = RHI::Bind_RenderTarget | RHI::Bind_ShaderResource;
	color.c_DebugName = "GameRenderTarget.Color";
	m_cls_Color = dev->TextureCreate(color, nullptr);

	RHI::TextureDesc depth;
	depth.i_Width = (uint32_t)width;
	depth.i_Height = (uint32_t)height;
	depth.e_Format = RHI::Format::D24_UNorm_S8_UInt;
	depth.i_BindFlags = RHI::Bind_DepthStencil | (depthReadable ? RHI::Bind_ShaderResource : 0);
	depth.c_DebugName = "GameRenderTarget.Depth";
	m_cls_Depth = dev->TextureCreate(depth, nullptr);

	m_i_Width = width;
	m_i_Height = height;
	m_b_DepthReadable = depthReadable;
	return m_cls_Color && m_cls_Depth;
}

void GameRenderTarget::Init(int width, int height)
{
	TargetsCreate(width, height, false);
}

void GameRenderTarget::InitWithDepthSRV(int width, int height)
{
	TargetsCreate(width, height, true);
}

/*
* 関数名　Begin
* 引　数　clearTarget：カラーをクリアするか
* 戻り値　なし
* 説　明　描画先をこのターゲットに切り替える。ブレンド有効時は透明、無効時は背景色でクリアする
*/
void GameRenderTarget::Begin(bool clearTarget)
{
	if (!m_cls_Color) return;
	GraphicsSystem* gs = GraphicsSystem::GetInstance();
	RHI::Device* dev = gs->GetDevice();
	RHI::Texture* color = m_cls_Color.get();
	gs->RenderTargetsSet(1, &color, m_b_RenderZBuffer ? m_cls_Depth.get() : nullptr);

	if (clearTarget) {
		if (m_b_BlendEnabled) {
			const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			dev->RenderTargetClear(color, clearColor);
		}
		else {
			DirectX::XMFLOAT4 bg = SettingManager::GetInstance()->GetBackgroundColor();
			const float clearColor[4] = { bg.x, bg.y, bg.z, 1.0f };
			dev->RenderTargetClear(color, clearColor);
		}
	}
	if (m_cls_Depth) dev->DepthStencilClear(m_cls_Depth.get(), true, true, 1.0f, 0);
}

void GameRenderTarget::End()
{
	GraphicsSystem::GetInstance()->BackBufferTargetSet(true);
}

void GameRenderTarget::Clear(float r, float g, float b, float a)
{
	if (!m_cls_Color) return;
	const float clearColor[4] = { r, g, b, a };
	GraphicsSystem::GetInstance()->GetDevice()->RenderTargetClear(m_cls_Color.get(), clearColor);
}
