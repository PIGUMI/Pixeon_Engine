cbuffer ModelCB : register(b0)
{
    matrix gWorld;
    matrix gView;
    matrix gProj;
    float4 gBaseColor;
};

Texture2D gBaseTex : register(t0);
SamplerState gLinear : register(s0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

float4 main(PS_INPUT i) : SV_TARGET
{
    float3 n = normalize(i.normal);
    float ndl = saturate(dot(n, normalize(float3(0.3, 0.7, 0.6))));
    float lighting = 0.2 + 0.8 * ndl;

    // 必ず何かしらの SRV がバインドされている前提（CPU 側が保証）
    float4 texCol = gBaseTex.Sample(gLinear, i.uv);
    return texCol * gBaseColor;
}