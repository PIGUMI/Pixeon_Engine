cbuffer VS_CB : register(b0)
{
    float4x4 World; // ’Ç‰Á
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
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL;
};

VS_OUT main(VS_IN i)
{
    VS_OUT o;
    
    if (mode2D != 0)
    {
        o.pos = float4(i.pos, 1.0f);
        o.worldPos = float3(0, 0, 0);
        o.normal = float3(0, 0, -1);
    }
    else
    {
        float4 wpos = mul(float4(i.pos, 1.0f), World);
        o.worldPos = wpos.xyz;
        
        float4 vpos = mul(wpos, View);
        o.pos = mul(vpos, Proj);
        
        o.normal = normalize(-View[2].xyz);
    }
    
    o.uv = i.uv;
    o.col = Color;
    return o;
}