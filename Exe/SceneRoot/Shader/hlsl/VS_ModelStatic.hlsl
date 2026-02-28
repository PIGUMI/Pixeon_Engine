cbuffer ModelCB : register(b0)
{
    matrix gWorld;
    matrix gView;
    matrix gProj;
    float4 gBaseColor;
};

struct VS_INPUT
{
    float3 pos      : POSITION;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
    float2 uv       : TEXCOORD;
    uint4  bi       : BLENDINDICES;
    float4 bw       : BLENDWEIGHT;
};

struct VS_OUTPUT
{
    float4 pos        : SV_POSITION;
    float3 normal     : NORMAL;       // ワールド空間法線 (既存ライティング用)
    float2 uv         : TEXCOORD;
    float3 worldPos   : WORLDPOS;
    float3 viewNormal : VIEWNORMAL;   // ★追加: ビュー空間法線 (GBuffer/SSAO用)
};

VS_OUTPUT main(VS_INPUT i)
{
    VS_OUTPUT o;

    float4 wp  = mul(float4(i.pos, 1), gWorld);
    o.worldPos = wp.xyz;

    float4 vp  = mul(wp, gView);
    o.pos      = mul(vp, gProj);

    // ワールド空間法線 (既存ライティング用)
    o.normal = mul(float4(i.normal, 0), gWorld).xyz;

    // ビュー空間法線 (GBuffer書き込み / SSAO用)
    // ワールド法線をさらにView行列で変換する
    // 平行移動を無視するために w=0 で mul する
    o.viewNormal = mul(float4(o.normal, 0), gView).xyz;

    o.uv = i.uv;
    return o;
}
