#include "AtmosphericFogEffect.h"
#include "System.h"
#include "ShaderManager.h"
#include "GUI.h"
#include "IMGUI/imgui.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

AtmosphericFogEffect::AtmosphericFogEffect()
{
    priority = 10; // Bloom(0)の後、ColorGrading(20)の前
}

AtmosphericFogEffect::~AtmosphericFogEffect()
{
    ReleaseResources();
}

// ============================================================
// EnsureResources
// ShaderManager が HLSL ディレクトリを自動スキャンするため
// GetVertexShader / GetPixelShader で取得するだけでよい
// ============================================================
bool AtmosphericFogEffect::EnsureResources()
{
    if (m_resourcesReady) return true;

    auto* dx = DirectX11::GetInstance();
    if (!dx) return false;
    ID3D11Device* device = dx->GetDevice();
    if (!device) return false;

    auto* sm = ShaderManager::GetInstance();
    m_vs = sm->GetVertexShader("VS_Fullscreen");
    m_ps = sm->GetPixelShader("PS_AtmosphericFog");
    if (!m_vs || !m_ps) return false;

    // ---- 定数バッファ (b0) ----
    {
        D3D11_BUFFER_DESC bd = {};
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.ByteWidth = sizeof(FogCB);
        bd.Usage = D3D11_USAGE_DEFAULT;
        if (FAILED(device->CreateBuffer(&bd, nullptr, m_cb.GetAddressOf())))
            return false;
    }

    // ---- PointClamp サンプラー ----
    {
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        device->CreateSamplerState(&sd, m_pointClamp.GetAddressOf());
    }

    // ---- 深度書き込み OFF ----
    {
        D3D11_DEPTH_STENCIL_DESC dd = {};
        dd.DepthEnable = FALSE;
        dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
        device->CreateDepthStencilState(&dd, m_dsOff.GetAddressOf());
    }

    m_resourcesReady = true;
    return true;
}

void AtmosphericFogEffect::ReleaseResources()
{
    m_cb.Reset();
    m_pointClamp.Reset();
    m_dsOff.Reset();
    // m_vs / m_ps は ShaderManager 所有なので Release しない
    m_vs = nullptr;
    m_ps = nullptr;
    m_resourcesReady = false;
}

// ============================================================
// Apply
// t0 = シーンカラー, t1 = 深度 SRV
// ============================================================
void AtmosphericFogEffect::Apply(
    ID3D11ShaderResourceView* input,
    ID3D11RenderTargetView* output,
    int width, int height)
{
    if (!input || !output) return;
    if (!EnsureResources()) return;

    auto* dx = DirectX11::GetInstance();
    if (!dx) return;
    ID3D11DeviceContext* ctx = dx->GetContext();
    if (!ctx) return;

    FogCB cb = {};
    cb.invProj = DirectX::XMMatrixTranspose(m_invProj);
    cb.invView = DirectX::XMMatrixTranspose(m_invView);
    cb.fogColor = fogColor;
    cb.fogDensity = fogDensity;
    cb.fogStart = fogStart;
    cb.fogEnd = fogEnd;
    cb.fogHeight = fogHeight;
    cb.heightFalloff = heightFalloff;
    cb.resolutionX = (float)width;
    cb.resolutionY = (float)height;
    cb.fogMode = (int)fogMode;
    cb.cameraPos = m_cameraPos;
    ctx->UpdateSubresource(m_cb.Get(), 0, nullptr, &cb, 0, 0);

    ctx->OMSetDepthStencilState(m_dsOff.Get(), 0);
    ctx->OMSetRenderTargets(1, &output, nullptr);

    D3D11_VIEWPORT vp = {};
    vp.Width = (float)width;
    vp.Height = (float)height;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);

    ctx->VSSetShader(m_vs, nullptr, 0);
    ctx->PSSetShader(m_ps, nullptr, 0);

    ID3D11Buffer* cbs[] = { m_cb.Get() };
    ctx->PSSetConstantBuffers(0, 1, cbs);

    ID3D11ShaderResourceView* srvs[] = { input, m_depthSRV };
    ctx->PSSetShaderResources(0, 2, srvs);

    ID3D11SamplerState* smps[] = { m_pointClamp.Get() };
    ctx->PSSetSamplers(0, 1, smps);

    ctx->IASetInputLayout(nullptr);
    ctx->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->Draw(3, 0);

    ID3D11ShaderResourceView* nullSRVs[2] = {};
    ctx->PSSetShaderResources(0, 2, nullSRVs);
    ctx->OMSetDepthStencilState(nullptr, 0);
}

// ============================================================
void AtmosphericFogEffect::DrawInspector()
{
    auto SJ = [](const char* s) { return GUI::GetInstance()->ShiftJISToUTF8(s); };

    ImGui::ColorEdit3(SJ("フォグカラー").c_str(), (float*)&fogColor);

    const char* modeNames[] = { "Linear", "Exponential", "Exponential2" };
    int modeIdx = (int)fogMode;
    if (ImGui::Combo(SJ("フォグモード").c_str(), &modeIdx, modeNames, 3))
        fogMode = (FogMode)modeIdx;

    if (fogMode == FogMode::Linear)
    {
        ImGui::DragFloat(SJ("開始距離").c_str(), &fogStart, 1.0f, 0.0f, fogEnd - 0.1f);
        ImGui::DragFloat(SJ("終了距離").c_str(), &fogEnd, 1.0f, fogStart + 0.1f, 10000.0f);
    }
    else
    {
        ImGui::DragFloat(SJ("フォグ密度").c_str(), &fogDensity, 0.001f, 0.0001f, 1.0f, "%.4f");
    }

    ImGui::Separator();
    ImGui::Text(SJ("高さフォグ").c_str());
    ImGui::DragFloat(SJ("最大高さ (0=無効)").c_str(), &fogHeight, 1.0f, 0.0f, 2000.0f);
    ImGui::DragFloat(SJ("高さ減衰率").c_str(), &heightFalloff, 0.001f, 0.0001f, 1.0f, "%.4f");
}

// ============================================================
void AtmosphericFogEffect::SaveToJson(nlohmann::json& j) const
{
    j["fogColor"] = { fogColor.x, fogColor.y, fogColor.z };
    j["fogDensity"] = fogDensity;
    j["fogStart"] = fogStart;
    j["fogEnd"] = fogEnd;
    j["fogHeight"] = fogHeight;
    j["heightFalloff"] = heightFalloff;
    j["fogMode"] = (int)fogMode;
}

void AtmosphericFogEffect::LoadFromJson(const nlohmann::json& j)
{
    if (j.contains("fogColor") && j["fogColor"].is_array())
    {
        fogColor.x = j["fogColor"][0];
        fogColor.y = j["fogColor"][1];
        fogColor.z = j["fogColor"][2];
    }
    fogDensity = j.value("fogDensity", 0.02f);
    fogStart = j.value("fogStart", 10.0f);
    fogEnd = j.value("fogEnd", 200.0f);
    fogHeight = j.value("fogHeight", 50.0f);
    heightFalloff = j.value("heightFalloff", 0.1f);
    fogMode = (FogMode)j.value("fogMode", 1);
}

PostEffectBase* AtmosphericFogEffect::Clone() const
{
    auto* c = new AtmosphericFogEffect();
    c->fogColor = fogColor;
    c->fogDensity = fogDensity;
    c->fogStart = fogStart;
    c->fogEnd = fogEnd;
    c->fogHeight = fogHeight;
    c->heightFalloff = heightFalloff;
    c->fogMode = fogMode;
    c->enabled = enabled;
    c->priority = priority;
    return c;
}