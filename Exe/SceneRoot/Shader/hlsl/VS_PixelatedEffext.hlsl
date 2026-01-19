cbuffer VS_CB : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4 Color;
    int mode2D;
    float3 _pad;
};

struct VS_IN
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct VS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 col : COLOR0;
};

VS_OUT main(VS_IN i)
{
    VS_OUT o;
    if (mode2D != 0)
    {
        o.pos = float4(i.pos, 1.0f);
    }
    else
    {
        float4 wpos = float4(i.pos, 1.0f);
        float4 vpos = mul(wpos, View);
        o.pos = mul(vpos, Proj);
    }
    o.uv = i.uv;
    o.col = Color;
    return o;
}