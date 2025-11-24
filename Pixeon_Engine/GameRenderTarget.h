// GameRenderTarget.h への修正
#pragma once
#include<d3d11.h>

class GameRenderTarget
{
public:
	void Init(ID3D11Device* device, int width, int height);
	void InitWithDepthSRV(ID3D11Device* device, int width, int height); // DoF用の初期化
	void Begin(ID3D11DeviceContext* context);
	void End();

	ID3D11ShaderResourceView* GetShaderResourceView() const { return m_pSRV; }
	ID3D11RenderTargetView* GetRenderTargetView() { return m_pRTV; }
	ID3D11ShaderResourceView* GetDepthShaderResourceView() const { return m_pDepthSRV; } // DoF用

	void SetRenderZBuffer(bool isRenderZBuffer) { m_isRenderZBuffer = isRenderZBuffer; }
	bool IsRenderZBuffer() const { return m_isRenderZBuffer; }

private:
	ID3D11Texture2D* m_pTexture = nullptr;
	ID3D11RenderTargetView* m_pRTV = nullptr;
	ID3D11ShaderResourceView* m_pSRV = nullptr;
	D3D11_VIEWPORT m_viewport = {};
	ID3D11Texture2D* m_pDepthStencilTexture = nullptr;
	ID3D11DepthStencilView* m_pDSV = nullptr;
	ID3D11ShaderResourceView* m_pDepthSRV = nullptr; // DoF用の深度SRV

	bool m_isRenderZBuffer = false;
};