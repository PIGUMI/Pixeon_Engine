#pragma once
#include "LayerSettings.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

class AtmosphericFogEffect : public PostEffectBase
{
public:
    AtmosphericFogEffect();
    ~AtmosphericFogEffect() override;

    void Apply(
        ID3D11ShaderResourceView* input,
        ID3D11RenderTargetView* output,
        int width, int height) override;

    void DrawInspector()                            override;
    std::string    GetName()  const override { return "Atmospheric Fog"; }
    PostEffectType GetType()  const override { return PostEffectType::ATMOSPHERIC_FOG; }
    void SaveToJson(nlohmann::json& j)        const override;
    void LoadFromJson(const nlohmann::json& j)      override;
    PostEffectBase* Clone()   const override;

    void SetDepthSRV(ID3D11ShaderResourceView* srv) { m_depthSRV = srv; }

    // ★ invViewとcameraPosも受け取れるように追加
    void SetCameraMatrices(
        const DirectX::XMMATRIX& proj,
        const DirectX::XMMATRIX& invProj,
        const DirectX::XMMATRIX& invView,       // ★追加
        const DirectX::XMFLOAT3& cameraPos)     // ★追加
    {
        m_proj = proj;
        m_invProj = invProj;
        m_invView = invView;
        m_cameraPos = cameraPos;
    }

    // パラメータ
    DirectX::XMFLOAT3 fogColor{ 0.7f, 0.8f, 0.9f };
    float fogDensity = 0.02f;
    float fogStart = 10.0f;
    float fogEnd = 200.0f;
    float fogHeight = 50.0f;
    float heightFalloff = 0.1f;

    enum class FogMode { Linear = 0, Exponential = 1, ExponentialSquared = 2 };
    FogMode fogMode = FogMode::Exponential;

private:
    bool EnsureResources();
    void ReleaseResources();

    ID3D11VertexShader* m_vs = nullptr;
    ID3D11PixelShader* m_ps = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Buffer>            m_cb;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>      m_pointClamp;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_dsOff;

    ID3D11ShaderResourceView* m_depthSRV = nullptr;
    DirectX::XMMATRIX m_proj = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX m_invProj = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX m_invView = DirectX::XMMatrixIdentity();
    DirectX::XMFLOAT3 m_cameraPos = { 0.f, 0.f, 0.f };

    bool m_resourcesReady = false;

    // ★ invViewとcameraPosを追加（+32バイト → 合計160バイト、16の倍数OK）
    struct alignas(16) FogCB
    {
        DirectX::XMMATRIX invProj;        // 64 bytes  offset:  0
        DirectX::XMMATRIX invView;        // 64 bytes  offset: 64  ★追加
        DirectX::XMFLOAT3 fogColor;       // 12 bytes  offset:128
        float             fogDensity;     //  4 bytes  offset:140
        float             fogStart;       //  4 bytes  offset:144
        float             fogEnd;         //  4 bytes  offset:148
        float             fogHeight;      //  4 bytes  offset:152
        float             heightFalloff;  //  4 bytes  offset:156
        float             resolutionX;    //  4 bytes  offset:160
        float             resolutionY;    //  4 bytes  offset:164
        int               fogMode;        //  4 bytes  offset:168
        DirectX::XMFLOAT3 cameraPos;      // 12 bytes  offset:172  ★追加
        float             _pad;           //  4 bytes  offset:184
    }; // 合計: 188 bytes → 192 bytes（16の倍数に切り上げ）
    static_assert(sizeof(FogCB) % 16 == 0, "FogCB must be 16-byte aligned");
};