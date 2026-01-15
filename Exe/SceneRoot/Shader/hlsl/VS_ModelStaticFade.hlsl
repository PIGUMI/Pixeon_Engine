cbuffer ModelCB : register(b0)
{
    matrix gWorld;
    matrix gView;
    matrix gProj;
    float4 gBaseColor;
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
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float3 worldPos : WORLDPOS;
    float3 viewPos : VIEWPOS;
};

VS_OUTPUT main(VS_INPUT i)
{
    VS_OUTPUT o;
    float4 wp = mul(float4(i.pos, 1), gWorld);
    o.worldPos = wp.xyz;
    
    // ビュー空間での位置を計算
    float4 vp = mul(wp, gView);
    o.viewPos = vp.xyz;
    
    o.pos = mul(vp, gProj);
    o.normal = mul(float4(i.normal, 0), gWorld).xyz;
    o.uv = i.uv;
    
    return o;
}