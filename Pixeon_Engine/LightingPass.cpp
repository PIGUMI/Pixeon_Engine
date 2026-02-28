#include "LightingPass.h"
#include "GBuffer.h"
#include "System.h"
#include "ShaderManager.h"

using namespace DirectX;

// ============================================================
// Init
// ============================================================
bool LightingPass::Init(ID3D11Device* device)
{
    if (!device) return false;

    // ---- 定数バッファ ----
    {
        D3D11_BUFFER_DESC bd = {};
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.ByteWidth = sizeof(LightingCB);
        bd.Usage = D3D11_USAGE_DEFAULT;
        if (FAILED(device->CreateBuffer(&bd, nullptr, m_lightingCB.GetAddressOf())))
            return false;
    }

    // ---- PointClamp サンプラー（GBuffer 読み取り用）----
    {
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device->CreateSamplerState(&sd, m_pointClampSmp.GetAddressOf())))
            return false;
    }

    // ---- シャドウサンプラー（Comparison Sampler）----
    {
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device->CreateSamplerState(&sd, m_shadowSmp.GetAddressOf())))
            return false;
    }

    // ---- 深度書き込みOFF ステート ----
    {
        D3D11_DEPTH_STENCIL_DESC dd = {};
        dd.DepthEnable = FALSE;
        dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
        if (FAILED(device->CreateDepthStencilState(&dd, m_dsOff.GetAddressOf())))
            return false;
    }

    // ---- ダミー白テクスチャ（SSAO 未設定時のフォールバック）----
    if (!EnsureWhiteSRV(device)) return false;

    m_initialized = true;
    return true;
}

// ============================================================
// EnsureWhiteSRV
// ============================================================
bool LightingPass::EnsureWhiteSRV(ID3D11Device* device)
{
    if (m_whiteSRV) return true;

    uint32_t pixel = 0xFFFFFFFF; // 白（R=1, G=1, B=1, A=1）
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height = 1;
    td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = &pixel;
    init.SysMemPitch = 4;

    if (FAILED(device->CreateTexture2D(&td, &init, m_whiteTex.GetAddressOf())))
        return false;
    if (FAILED(device->CreateShaderResourceView(m_whiteTex.Get(), nullptr, m_whiteSRV.GetAddressOf())))
        return false;

    return true;
}

// ============================================================
// Release
// ============================================================
void LightingPass::Release()
{
    m_lightingCB.Reset();
    m_pointClampSmp.Reset();
    m_shadowSmp.Reset();
    m_dsOff.Reset();
    m_whiteTex.Reset();
    m_whiteSRV.Reset();
    m_ssaoSRV = nullptr;
    m_initialized = false;
}

// ============================================================
// Execute
// GBuffer を読んで outputRTV にライティング結果を書き込む
// ============================================================
void LightingPass::Execute(
    ID3D11DeviceContext* ctx,
    GBuffer* gbuffer,
    ID3D11RenderTargetView* outputRTV,
    ID3D11ShaderResourceView* shadowMapSRV,
    int width, int height,
    const XMMATRIX& proj,
    const XMMATRIX& view,
    const XMFLOAT3& cameraPos)
{
    if (!m_initialized || !gbuffer || !outputRTV) return;

    auto* sm = ShaderManager::GetInstance();

    // シェーダー取得
    ID3D11VertexShader* vs = sm->GetVertexShader("VS_Fullscreen");
    ID3D11PixelShader* ps = sm->GetPixelShader("PS_Lighting");
    if (!vs || !ps) return;

    // ---- 定数バッファ更新 ----
    LightingCB cb = {};
    cb.invProj = XMMatrixTranspose(XMMatrixInverse(nullptr, proj));
    cb.invView = XMMatrixTranspose(XMMatrixInverse(nullptr, view));
    cb.view = XMMatrixTranspose(view);
    cb.cameraPos = cameraPos;
    cb.resolution = XMFLOAT2((float)width, (float)height);
    ctx->UpdateSubresource(m_lightingCB.Get(), 0, nullptr, &cb, 0, 0);

    // ---- 出力先 RTV をセット（深度なし）----
    ctx->OMSetRenderTargets(1, &outputRTV, nullptr);
    ctx->OMSetDepthStencilState(m_dsOff.Get(), 0);

    // ---- ビューポート ----
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)width;
    vp.Height = (float)height;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);

    // ---- シェーダーセット ----
    ctx->VSSetShader(vs, nullptr, 0);
    ctx->PSSetShader(ps, nullptr, 0);

    // ---- 定数バッファ（b0 = LightingCB）----
    // b1 / b2 / b3 はシーンが既にセット済みのものをそのまま使う
    // （LightArrayCB / LightCountCB / ShadowCB）
    ID3D11Buffer* cbs[] = { m_lightingCB.Get() };
    ctx->PSSetConstantBuffers(0, 1, cbs);

    // ---- SRV セット ----
    // t0=Albedo, t1=Normal, t2=Depth, t3=SSAO, t4=ShadowMap
    ID3D11ShaderResourceView* ssaoSRV = m_ssaoSRV ? m_ssaoSRV : m_whiteSRV.Get();
    ID3D11ShaderResourceView* srvs[] = {
        gbuffer->GetAlbedoSRV(),  // t0
        gbuffer->GetNormalSRV(),  // t1
        gbuffer->GetDepthSRV(),   // t2
        ssaoSRV,                  // t3
        shadowMapSRV,             // t4
    };
    ctx->PSSetShaderResources(0, 5, srvs);

    // ---- サンプラー ----
    ID3D11SamplerState* smps[] = {
        m_pointClampSmp.Get(),  // s0: GBuffer 読み取り
        m_shadowSmp.Get(),      // s1: シャドウマップ
    };
    ctx->PSSetSamplers(0, 2, smps);

    // ---- フルスクリーン三角形描画（頂点バッファ不要）----
    ctx->IASetInputLayout(nullptr);
    ctx->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->Draw(3, 0);

    // ---- SRV バインド解除（次フレームで RTV として使えるように）----
    ID3D11ShaderResourceView* nullSRVs[5] = {};
    ctx->PSSetShaderResources(0, 5, nullSRVs);

    // ---- 深度ステートを戻す ----
    ctx->OMSetDepthStencilState(nullptr, 0);
}