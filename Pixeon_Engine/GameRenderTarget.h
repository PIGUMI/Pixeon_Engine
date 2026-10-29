/*
* ファイル名　GameRenderTarget
* 説　　　明　レイヤーごとのオフスクリーン描画先（カラー + 深度）
*/
#pragma once
#include "RHI/RHI.h"

class GameRenderTarget
{
public:
	void Init(int width, int height);
	void InitWithDepthSRV(int width, int height); // 深度もシェーダーから読める版（DoF / フォグ用）
	void Begin(bool clearTarget = true);
	void End();
	void Clear(float r, float g, float b, float a);

	RHI::Texture* GetTexture() const { return m_cls_Color.get(); }
	RHI::Texture* GetDepthTexture() const { return m_cls_Depth.get(); }
	// 深度を SRV として読めるか（InitWithDepthSRV で作成した場合のみ）
	RHI::Texture* GetDepthShaderResource() const { return m_b_DepthReadable ? m_cls_Depth.get() : nullptr; }

	void SetRenderZBuffer(bool isRenderZBuffer) { m_b_RenderZBuffer = isRenderZBuffer; }
	bool IsRenderZBuffer() const { return m_b_RenderZBuffer; }
	int GetWidth() const { return m_i_Width; }
	int GetHeight() const { return m_i_Height; }
	void SetBlend(bool Enable) { m_b_BlendEnabled = Enable; }

private:
	bool TargetsCreate(int width, int height, bool depthReadable);

	RHI::TextureRef m_cls_Color;
	RHI::TextureRef m_cls_Depth;
	int  m_i_Width = 0;
	int  m_i_Height = 0;
	bool m_b_DepthReadable = false;
	bool m_b_RenderZBuffer = false;
	bool m_b_BlendEnabled = false;
};
