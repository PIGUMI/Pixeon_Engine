#pragma once
#include "LayerSettings.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>

class SSAOEffect : public PostEffectBase
{
public:
    SSAOEffect();
    ~SSAOEffect() = default;

    // PostEffectBase インターフェース
    void Apply(
        ID3D11ShaderResourceView* input,
        ID3D11RenderTargetView* output,
        int width, int height) override;

    void DrawInspector() override;
    std::string    GetName()  const override { return "SSAO"; }
    PostEffectType GetType()  const override { return PostEffectType::SSAO; }
    void SaveToJson(nlohmann::json& j) const override;
    void LoadFromJson(const nlohmann::json& j) override;
    PostEffectBase* Clone() const override;

    // Apply の前に LayerSettings.cpp から呼ぶ
    void SetDepthSRV(ID3D11ShaderResourceView* depthSRV) { depthSRV_ = depthSRV; }
    void SetCameraMatrices(const DirectX::XMMATRIX& proj, const DirectX::XMMATRIX& invProj)
    {
        proj_ = proj;
        invProj_ = invProj;
    }

    // ImGui で調整可能なパラメーター
    float radius = 0.5f;   // サンプル半径 (ビュー空間単位)
    float bias = 0.025f; // 法線バイアス
    float power = 2.0f;   // AO 強度 (pow 指数)
    float aoStrength = 0.8f;

private:
    // 定数バッファ構造体
    struct SSAO_Params {
        DirectX::XMMATRIX proj;
        DirectX::XMMATRIX invProj;
        DirectX::XMFLOAT2 resolution;
        float radius;
        float bias;
        float power;
        DirectX::XMFLOAT3 _pad;
    };
    struct KernelCB {
        DirectX::XMFLOAT4 kernel[64];
    };
    struct BlurParams {
        DirectX::XMFLOAT2 texelSize;
        DirectX::XMFLOAT2 _pad;
    };
    struct CompositeParams {
        float aoStrength;
        DirectX::XMFLOAT3 _pad;
    };

    // レンダーターゲット (SSAOバッファ / ブラーバッファ)
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          ssaoTex_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   ssaoRTV_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ssaoSRV_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          blurTex_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   blurRTV_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> blurSRV_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>          noiseTex_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noiseSRV_;

    // サンプラー
    Microsoft::WRL::ComPtr<ID3D11SamplerState>       pointClampSmp_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>       pointWrapSmp_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>       linearSmp_;

    // 深度書き込み無効ステート
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  dsOff_;

    // 定数バッファ
    Microsoft::WRL::ComPtr<ID3D11Buffer>             ssaoCB_;      // b1
    Microsoft::WRL::ComPtr<ID3D11Buffer>             kernelCB_;    // b2
    Microsoft::WRL::ComPtr<ID3D11Buffer>             blurCB_;      // b1 (ブラーパス)
    Microsoft::WRL::ComPtr<ID3D11Buffer>             compositeCB_; // b1 (合成パス)

    // 半球カーネル (CPU 側)
    std::vector<DirectX::XMFLOAT4> kernel_;

    // 外部からセットされる情報 (Apply のたびにリセット)
    ID3D11ShaderResourceView* depthSRV_ = nullptr;
    DirectX::XMMATRIX proj_ = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX invProj_ = DirectX::XMMatrixIdentity();

    // バッファサイズキャッシュ
    int cachedWidth_ = 0;
    int cachedHeight_ = 0;

    // 遅延初期化
    bool EnsureRenderTargets(int width, int height);
    void EnsureKernel();
    void EnsureNoise();
    void EnsureSamplers();
    void EnsureDepthStencilState();
    void EnsureConstantBuffers();

    // VS_Fullscreen 方式のフルスクリーン描画
    // 頂点バッファ不要: Draw(3, 0) で描画
    void DrawFullscreen(
        ID3D11PixelShader* ps,
        ID3D11ShaderResourceView* const* srvs,   // 配列先頭
        UINT                              srvCount,
        UINT                              srvSlot,
        ID3D11Buffer* const* psCBs,  // 配列先頭
        UINT                              cbCount,
        UINT                              cbSlot,
        ID3D11RenderTargetView* rtv,
        int width, int height
    );
};