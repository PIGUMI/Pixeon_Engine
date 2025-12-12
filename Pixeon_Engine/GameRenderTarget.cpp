#include "GameRenderTarget.h"
#include "System.h"
#include "SettingManager.h"

void GameRenderTarget::Init(ID3D11Device* device, int width, int height)
{
	// �����̎��������̂܂܈ێ�
	if (!device || width <= 0 || height <= 0)
	{
		return;
	}

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &m_pTexture);
	if (FAILED(hr) || !m_pTexture)
	{
		return;
	}

	hr = device->CreateRenderTargetView(m_pTexture, nullptr, &m_pRTV);
	if (FAILED(hr) || !m_pRTV)
	{
		return;
	}

	hr = device->CreateShaderResourceView(m_pTexture, nullptr, &m_pSRV);
	if (FAILED(hr) || !m_pSRV)
	{
		return;
	}

	m_viewport.Width = (FLOAT)width;
	m_viewport.Height = (FLOAT)height;
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;
	m_viewport.TopLeftX = 0;
	m_viewport.TopLeftY = 0;

	// �[�x�o�b�t�@�쐬�i�ʏ�Łj
	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	hr = device->CreateTexture2D(&depthDesc, nullptr, &m_pDepthStencilTexture);
	if (FAILED(hr) || !m_pDepthStencilTexture)
	{
		return;
	}

	hr = device->CreateDepthStencilView(m_pDepthStencilTexture, nullptr, &m_pDSV);
	if (FAILED(hr) || !m_pDSV)
	{
		return;
	}
}

void GameRenderTarget::InitWithDepthSRV(ID3D11Device* device, int width, int height)
{
	// DoF�p�̏������i�[�x�o�b�t�@��SRV�Ƃ��Ďg�p�\�j
	if (!device || width <= 0 || height <= 0)
	{
		return;
	}

	// �J���[�e�N�X�`���̍쐬
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &m_pTexture);
	if (FAILED(hr) || !m_pTexture)
	{
		return;
	}

	hr = device->CreateRenderTargetView(m_pTexture, nullptr, &m_pRTV);
	if (FAILED(hr) || !m_pRTV)
	{
		return;
	}

	hr = device->CreateShaderResourceView(m_pTexture, nullptr, &m_pSRV);
	if (FAILED(hr) || !m_pSRV)
	{
		return;
	}

	// �r���[�|�[�g�ݒ�
	m_viewport.Width = (FLOAT)width;
	m_viewport.Height = (FLOAT)height;
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;
	m_viewport.TopLeftX = 0;
	m_viewport.TopLeftY = 0;

	// �[�x�o�b�t�@��SRV�Ƃ��Ă��g�p�ł���悤�ɍ쐬
	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS; // SRV��DSV�̗����Ŏg�p�\
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	hr = device->CreateTexture2D(&depthDesc, nullptr, &m_pDepthStencilTexture);
	if (FAILED(hr) || !m_pDepthStencilTexture)
	{
		return;
	}

	// DSV�쐬
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	hr = device->CreateDepthStencilView(m_pDepthStencilTexture, &dsvDesc, &m_pDSV);
	if (FAILED(hr) || !m_pDSV)
	{
		return;
	}

	// SRV�쐬�i�[�x�e�N�X�`���p�j
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	hr = device->CreateShaderResourceView(m_pDepthStencilTexture, &srvDesc, &m_pDepthSRV);
	if (FAILED(hr) || !m_pDepthSRV)
	{
		return;
	}
}

void GameRenderTarget::Begin(ID3D11DeviceContext* context, bool clearTarget)
{
	if (m_isRenderZBuffer)
		context->OMSetRenderTargets(1, &m_pRTV, m_pDSV);
	else
		context->OMSetRenderTargets(1, &m_pRTV, nullptr);

	context->RSSetViewports(1, &m_viewport);

	if (clearTarget)
	{
		DirectX::XMFLOAT4 Temp = SettingManager::GetInstance()->GetBackgroundColor();
		float clearColor[4] = { Temp.x, Temp.y, Temp.z, 0.0f };
		context->ClearRenderTargetView(m_pRTV, clearColor);
	}

	if (m_pDSV)
		context->ClearDepthStencilView(m_pDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void GameRenderTarget::End()
{
	RenderTarget* defaultRTV = DirectX11::GetInstance()->GetDefaultRTV();
	DepthStencil* defaultDSV = DirectX11::GetInstance()->GetDefaultDSV();

	if (defaultRTV)
	{
		DirectX11::GetInstance()->SetRenderTargets(1, &defaultRTV, defaultDSV);
	}
}

void GameRenderTarget::Clear(ID3D11DeviceContext* context, float r, float g, float b, float a)
{
	if (!context || !m_pRTV) return;
	float clearColor[4] = { r, g, b, a };
	context->ClearRenderTargetView(m_pRTV, clearColor);
}
