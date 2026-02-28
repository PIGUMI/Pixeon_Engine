// =======================================================
// PS_GBuffer.hlsl
// Geometry Pass 用ピクセルシェーダー
//
// 出力:
//   SV_TARGET0 = Albedo  (RGBA8)   RGB=テクスチャ色*マテリアル色
//   SV_TARGET1 = Normal  (RGBA16F) RGB=ビュー空間法線 (normalize済み)
//
// ライティングはここでは行わない
// Lighting Pass (PS_Lighting.hlsl) で GBuffer を読んで計算する
// =======================================================

cbuffer ModelCB : register(b0)
{
    matrix gWorld;
    matrix gView;
    matrix gProj;
    float4 gBaseColor;
};

Texture2D    gBaseTex : register(t0);
SamplerState gLinear  : register(s0);

struct PS_INPUT
{
    float4 pos        : SV_POSITION;
    float3 normal     : NORMAL;
    float2 uv         : TEXCOORD;
    float3 worldPos   : WORLDPOS;
    float3 viewNormal : VIEWNORMAL;  // VSから受け取るビュー空間法線
};

struct PS_OUTPUT
{
    float4 albedo : SV_TARGET0;  // RT0: Albedo
    float4 normal : SV_TARGET1;  // RT1: Normal (ビュー空間)
};

PS_OUTPUT main(PS_INPUT i)
{
    PS_OUTPUT o;

    // アルベド: テクスチャ色 × マテリアル色
    float4 texCol = gBaseTex.Sample(gLinear, i.uv);
    o.albedo = float4(texCol.rgb * gBaseColor.rgb, texCol.a * gBaseColor.a);

    // ビュー空間法線を [-1,1] → [0,1] に圧縮して保存
    // 復元時: normal = normalize(n.xyz * 2.0 - 1.0)
    float3 vn = normalize(i.viewNormal);
    o.normal = float4(vn * 0.5 + 0.5, 1.0);

    return o;
}
