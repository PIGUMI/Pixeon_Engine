#pragma once
#include <d3d11.h>
#include <wrl/client.h>

// ============================================================
// GBuffer
//
// Deferred Rendering 用の複数レンダーターゲット管理クラス
//
// 構成:
//   RT0: Albedo        RGBA8_UNORM      (RGB=テクスチャ色, A=未使用)
//   RT1: Normal        RGBA16F          (RGB=ビュー空間法線, A=未使用)
//   Depth: R24G8_TYPELESS               (深度SRVとして読み取り可能)
//
// 深度バッファは Geometry Pass で書き込み、
// SSAO Pass / Lighting Pass で SRV として読み取る
// ============================================================
class GBuffer
{
public:
    // 初期化・解放
    bool Init(ID3D11Device* device, int width, int height);
    void Release();

    // Geometry Pass の開始
    // 複数RTと深度バッファをセットし、クリアする
    void BeginGeometryPass(ID3D11DeviceContext* ctx);

    // Geometry Pass の終了
    // RTをデフォルトに戻す（End後にSRVとして読める）
    void EndGeometryPass(ID3D11DeviceContext* ctx);

    // サイズ変更（解像度変更時）
    bool Resize(ID3D11Device* device, int width, int height);

    // ---- SRV アクセサ（Lighting / SSAO Pass で使用）----
    // RT0: Albedo SRV
    ID3D11ShaderResourceView* GetAlbedoSRV()  const { return m_albedoSRV.Get(); }
    // RT1: Normal SRV
    ID3D11ShaderResourceView* GetNormalSRV()  const { return m_normalSRV.Get(); }
    // Depth SRV (R24_UNORM_X8_TYPELESS)
    ID3D11ShaderResourceView* GetDepthSRV()   const { return m_depthSRV.Get(); }
    // DSV（Geometry Pass 後にTransparent描画等で再利用する場合）
    ID3D11DepthStencilView* GetDSV()        const { return m_dsv.Get(); }

    int GetWidth()  const { return m_width; }
    int GetHeight() const { return m_height; }

private:
    // RT0: Albedo  (RGBA8_UNORM)
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_albedoTex;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_albedoRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_albedoSRV;

    // RT1: Normal  (RGBA16F)
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_normalTex;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_normalRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_normalSRV;

    // Depth (R24G8_TYPELESS → DSV + SRV)
    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_depthTex;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   m_dsv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_depthSRV;

    // ビューポート
    D3D11_VIEWPORT m_viewport = {};
    int m_width = 0;
    int m_height = 0;

    // 内部生成ヘルパー
    bool CreateRenderTargets(ID3D11Device* device, int width, int height);
};