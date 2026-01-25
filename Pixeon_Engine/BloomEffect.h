#pragma once
#include "LayerSettings.h"
#include "ShaderManager.h"

class BloomEffect : public PostEffectBase {
public:
    float threshold = 0.8f;
    float intensity = 0.5f;
    float blurSize = 2.0f;
    DirectX::XMFLOAT4 tint = DirectX::XMFLOAT4(1, 1, 1, 1);

    void Apply(ID3D11ShaderResourceView* input,
        ID3D11RenderTargetView* output,
        int width, int height) override;

    void DrawInspector() override;
    std::string GetName() const override { return "Bloom"; }
    PostEffectType GetType() const override { return PostEffectType::BLOOM; }

    void SaveToJson(nlohmann::json& j) const override;
    void LoadFromJson(const nlohmann::json& j) override;
    PostEffectBase* Clone() const override;

private:
    ID3D11Texture2D* tempTexture_ = nullptr;
    ID3D11RenderTargetView* tempRTV_ = nullptr;
    ID3D11ShaderResourceView* tempSRV_ = nullptr;
    int bufferWidth_ = 0;
    int bufferHeight_ = 0;

    void CreateTempBuffer(int width, int height);
    void ReleaseTempBuffer();
    void InitializeResources(ID3D11Device* dev, ID3D11VertexShader* vs, ShaderManager* sm);
    void DrawQuad(ID3D11DeviceContext* ctx, ID3D11VertexShader* vs, ID3D11PixelShader* ps,
        ID3D11ShaderResourceView* srv, float x, float y, float width, float height,
        int screenWidth, int screenHeight,
        const DirectX::XMFLOAT4& color = DirectX::XMFLOAT4(1, 1, 1, 1),
        float opacity = 1.0f);
};