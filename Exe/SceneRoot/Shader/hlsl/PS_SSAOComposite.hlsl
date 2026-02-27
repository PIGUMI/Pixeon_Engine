// PS_SSAOComposite.hlsl
// VS_Fullscreen と組み合わせて使用する
// カラーバッファにブラー済み SSAO を乗算して出力する
// t0 = カラーバッファ (layerRT の SRV)
// t1 = ブラー済み SSAO バッファ (R8_UNORM)
// b1 = 合成パラメーター

cbuffer CompositeParams : register(b1)
{
    float gAOStrength; // 0.0 = AO 無効, 1.0 = フル適用
    float3 _pad;
};

Texture2D    gColorTex : register(t0);
Texture2D    gSSAOTex  : register(t1);
SamplerState gLinear   : register(s0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 main(PS_INPUT i) : SV_TARGET
{
    float4 color = gColorTex.Sample(gLinear, i.uv);
    float  ao    = gSSAOTex.Sample(gLinear,  i.uv).r;

    // AO を強度でブレンド: 1.0 に近いほど影が薄い
    color.rgb *= lerp(1.0f, ao, gAOStrength);
    return color;
}
