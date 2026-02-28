#include "GBuffer.h"
#include "System.h" // DirectX11::GetInstance() のため

// ============================================================
// Init
// ============================================================
bool GBuffer::Init(ID3D11Device* device, int width, int height)
{
    if (!device || width <= 0 || height <= 0) return false;
    return CreateRenderTargets(device, width, height);
}

// ============================================================
// Release
// ============================================================
void GBuffer::Release()
{
    m_albedoTex.Reset();  m_albedoRTV.Reset();  m_albedoSRV.Reset();
    m_normalTex.Reset();  m_normalRTV.Reset();  m_normalSRV.Reset();
    m_depthTex.Reset();   m_dsv.Reset();        m_depthSRV.Reset();
    m_width = 0;
    m_height = 0;
}

// ============================================================
// Resize
// ============================================================
bool GBuffer::Resize(ID3D11Device* device, int width, int height)
{
    Release();
    return CreateRenderTargets(device, width, height);
}

// ============================================================
// BeginGeometryPass
// 2枚のRTVと深度バッファをセット、クリアして描画準備
// ============================================================
void GBuffer::BeginGeometryPass(ID3D11DeviceContext* ctx)
{
    // 2枚のRTをセット
    ID3D11RenderTargetView* rtvs[] = {
        m_albedoRTV.Get(),  // [0] Albedo
        m_normalRTV.Get(),  // [1] Normal
    };
    ctx->OMSetRenderTargets(2, rtvs, m_dsv.Get());
    ctx->RSSetViewports(1, &m_viewport);

    // Albedo クリア（黒・不透明）
    float clearAlbedo[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    ctx->ClearRenderTargetView(m_albedoRTV.Get(), clearAlbedo);

    // Normal クリア（0, 0, 0）
    float clearNormal[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ctx->ClearRenderTargetView(m_normalRTV.Get(), clearNormal);

    // 深度クリア
    ctx->ClearDepthStencilView(m_dsv.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

// ============================================================
// EndGeometryPass
// RTをデフォルトに戻す（この後SRVとして読み取れる）
// ============================================================
void GBuffer::EndGeometryPass(ID3D11DeviceContext* ctx)
{
    // SRV として使う前に RT の解除が必要
    ID3D11RenderTargetView* nullRTVs[2] = { nullptr, nullptr };
    ctx->OMSetRenderTargets(2, nullRTVs, nullptr);
}

// ============================================================
// CreateRenderTargets  内部生成ヘルパー
// ============================================================
bool GBuffer::CreateRenderTargets(ID3D11Device* device, int width, int height)
{
    m_width = width;
    m_height = height;

    HRESULT hr;

    // --------------------------------------------------------
    // RT0: Albedo  (RGBA8_UNORM)
    // --------------------------------------------------------
    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = width;
        td.Height = height;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        hr = device->CreateTexture2D(&td, nullptr, m_albedoTex.GetAddressOf());
        if (FAILED(hr)) return false;

        hr = device->CreateRenderTargetView(m_albedoTex.Get(), nullptr, m_albedoRTV.GetAddressOf());
        if (FAILED(hr)) return false;

        hr = device->CreateShaderResourceView(m_albedoTex.Get(), nullptr, m_albedoSRV.GetAddressOf());
        if (FAILED(hr)) return false;
    }

    // --------------------------------------------------------
    // RT1: Normal  (RGBA16F)
    // 16bit float にすることで法線の精度を確保
    // --------------------------------------------------------
    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = width;
        td.Height = height;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        hr = device->CreateTexture2D(&td, nullptr, m_normalTex.GetAddressOf());
        if (FAILED(hr)) return false;

        hr = device->CreateRenderTargetView(m_normalTex.Get(), nullptr, m_normalRTV.GetAddressOf());
        if (FAILED(hr)) return false;

        hr = device->CreateShaderResourceView(m_normalTex.Get(), nullptr, m_normalSRV.GetAddressOf());
        if (FAILED(hr)) return false;
    }

    // --------------------------------------------------------
    // Depth (R24G8_TYPELESS)
    // DSV と SRV の両方で使うために TYPELESS フォーマットを使用
    // DSV: DXGI_FORMAT_D24_UNORM_S8_UINT
    // SRV: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
    // --------------------------------------------------------
    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = width;
        td.Height = height;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R24G8_TYPELESS;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        hr = device->CreateTexture2D(&td, nullptr, m_depthTex.GetAddressOf());
        if (FAILED(hr)) return false;

        // DSV
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        hr = device->CreateDepthStencilView(m_depthTex.Get(), &dsvDesc, m_dsv.GetAddressOf());
        if (FAILED(hr)) return false;

        // SRV（深度チャンネルのみ読み取り）
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        hr = device->CreateShaderResourceView(m_depthTex.Get(), &srvDesc, m_depthSRV.GetAddressOf());
        if (FAILED(hr)) return false;
    }

    // --------------------------------------------------------
    // ビューポート
    // --------------------------------------------------------
    m_viewport.Width = (float)width;
    m_viewport.Height = (float)height;
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;
    m_viewport.TopLeftX = 0;
    m_viewport.TopLeftY = 0;

    return true;
}