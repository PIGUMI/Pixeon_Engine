cbuffer ShadowCB : register(b0)
{
    matrix gLightViewProj;
    matrix gWorld;
};

struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
    uint4 bi : BLENDINDICES;
    float4 bw : BLENDWEIGHT;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float depth : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    
    float4 worldPos = mul(float4(input.pos, 1.0f), gWorld);
    output.pos = mul(worldPos, gLightViewProj);
    output.depth = output.pos.z / output.pos.w;
    
    return output;
}