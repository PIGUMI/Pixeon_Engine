// PS_SSAOBlur.hlsl
// VS_Fullscreen と組み合わせて使用する
// 5x5 ボックスブラーで SSAO のノイズを除去する
// t0 = 生 SSAO バッファ (R8_UNORM)
// b1 = ブラーパラメーター

cbuffer BlurParams : register(b1)
{
    float2 gTexelSize; // 1.0 / 解像度
    float2 _pad;
};

Texture2D    gSSAOTex  : register(t0);
SamplerState gPointSmp : register(s0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float main(PS_INPUT i) : SV_TARGET
{
    float result = 0.0f;
    [unroll]
    for (int x = -2; x <= 2; ++x)
    {
        [unroll]
        for (int y = -2; y <= 2; ++y)
        {
            result += gSSAOTex.Sample(gPointSmp, i.uv + float2(x, y) * gTexelSize).r;
        }
    }
    return result / 25.0f;
}
