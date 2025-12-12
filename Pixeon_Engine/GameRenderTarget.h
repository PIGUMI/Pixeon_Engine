// GameRenderTarget.h Ç÷ÇÃèCê≥
#pragma once
#include<d3d11.h>

class GameRenderTarget
{
public:
	void Init(ID3D11Device* device, int width, int height);
	void InitWithDepthSRV(ID3D11Device* device, int width, int height); // DoFópÇÃèâä˙âª
	void Begin(ID3D11DeviceContext* context, bool clearTarget = true);
	void End();
	void Clear(ID3D11DeviceContext* context, float r, float g, float b, float a);
	ID3D11ShaderResourceView* GetShaderResourceView() const { return m_pSRV; }
	ID3D11RenderTargetView* GetRenderTargetView() { return m_pRTV; }
	ID3D11ShaderResourceView* GetDepthShaderResourceView() const { return m_pDepthSRV; } // DoFóp

	void SetRenderZBuffer(bool isRenderZBuffer) { m_isRenderZBuffer = isRenderZBuffer; }
	bool IsRenderZBuffer() const { return m_isRenderZBuffer; }
	int GetWidth() const { return (int)m_viewport.Width; }
	int GetHeight() const { return (int)m_viewport.Height; }

private:
	ID3D11Texture2D* m_pTexture = nullptr;
	ID3D11RenderTargetView* m_pRTV = nullptr;
	ID3D11ShaderResourceView* m_pSRV = nullptr;
	D3D11_VIEWPORT m_viewport = {};
	ID3D11Texture2D* m_pDepthStencilTexture = nullptr;
	ID3D11DepthStencilView* m_pDSV = nullptr;
	ID3D11ShaderResourceView* m_pDepthSRV = nullptr;

	bool m_isRenderZBuffer = false;
};