// PS_AtmosphericFog.hlsl
// t0: シーンカラー
// t1: 深度バッファ [0,1]
// b0: FogCB

cbuffer FogCB : register(b0)
{
    matrix  invProj;        // クリップ空間 → ビュー空間（転置済み）
    matrix  invView;        // ビュー空間 → ワールド空間（転置済み）★追加
    float3  fogColor;
    float   fogDensity;
    float   fogStart;
    float   fogEnd;
    float   fogHeight;      // 0 = 高さフォグ無効
    float   heightFalloff;
    float   resolutionX;
    float   resolutionY;
    int     fogMode;        // 0=Linear, 1=Exp, 2=Exp2
    float3  cameraPos;      // ワールド空間カメラ位置 ★追加
    float   _pad;
};

Texture2D<float4> SceneColor : register(t0);
Texture2D<float>  DepthTex   : register(t1);
SamplerState      PointClamp : register(s0);

float3 ReconstructViewPos(float2 uv, float ndcDepth)
{
    float ndcX = uv.x * 2.0f - 1.0f;
    float ndcY = -uv.y * 2.0f + 1.0f;

    float4 clipPos = float4(ndcX, ndcY, ndcDepth, 1.0f);
    float4 viewPos = mul(invProj, clipPos);
    viewPos /= viewPos.w;
    return viewPos.xyz;
}

float3 ViewToWorld(float3 viewPos)
{
    float4 worldPos = mul(invView, float4(viewPos, 1.0f));
    return worldPos.xyz;
}

float ComputeFogFactor(float dist)
{
    float f = 0.0f;
    if (fogMode == 0)
    {
        // Linear
        f = saturate((dist - fogStart) / max(fogEnd - fogStart, 0.0001f));
    }
    else if (fogMode == 1)
    {
        // Exponential
        f = 1.0f - exp(-fogDensity * dist);
    }
    else
    {
        // Exponential Squared
        float x = fogDensity * dist;
        f = 1.0f - exp(-(x * x));
    }
    return saturate(f);
}

float4 main(float4 pos : SV_POSITION) : SV_TARGET
{
    float2 uv = pos.xy / float2(resolutionX, resolutionY);

    float4 sceneColor = SceneColor.Sample(PointClamp, uv);
    float  rawDepth = DepthTex.Sample(PointClamp, uv).r;

    float3 viewPos = ReconstructViewPos(uv, rawDepth);

    float3 worldPos = ViewToWorld(viewPos);

    float dist;
    if (rawDepth >= 0.9999f)
    {
        dist = fogEnd;
    }
    else
    {
        dist = length(worldPos - cameraPos);
    }

    float fogFactor = ComputeFogFactor(dist);

    if (fogHeight > 0.0f)
    {
        float heightAbove = max(0.0f, worldPos.y - fogHeight);
        fogFactor *= exp(-heightAbove * heightFalloff);
    }

    fogFactor = saturate(fogFactor);

    float3 result = lerp(sceneColor.rgb, fogColor, fogFactor);
    return float4(result, sceneColor.a);
}