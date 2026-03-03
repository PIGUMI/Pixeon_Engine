// PS_AtmosphericFog.hlsl
// t0: シーンカラー
// t1: 深度バッファ [0,1]
// b0: FogCB

cbuffer FogCB : register(b0)
{
    matrix  invProj;        // offset:  0  (64 bytes)
    matrix  invView;        // offset: 64  (64 bytes)
    float3  fogColor;       // offset:128  (12 bytes)
    float   fogDensity;     // offset:140
    float   fogStart;       // offset:144
    float   fogEnd;         // offset:148
    float   fogHeight;      // offset:152
    float   heightFalloff;  // offset:156
    float   resolutionX;    // offset:160
    float   resolutionY;    // offset:164
    int     fogMode;        // offset:168
    // fogMode(4bytes) の後、float3 cameraPos は 172-183
    // 172は16バイトスロット160-175の途中、終端183はスロット176-191に入る
    // → 16バイト境界(176)を跨ぐためHLSLは自動パディングを挿入しoffset:176から配置
    // → C++側も _padFog を追加してoffsetを176に合わせる
    float   _padFog;        // offset:172 (C++パディング穴埋め用)
    float3  cameraPos;      // offset:176 (HLSLのパッキングと一致)
    float   _pad;           // offset:188
};                          // 合計: 192 bytes (16の倍数)

Texture2D<float4> SceneColor : register(t0);
Texture2D<float>  DepthTex   : register(t1);
SamplerState      PointClamp : register(s0);

float3 ReconstructViewPos(float2 uv, float ndcDepth)
{
    float ndcX =  uv.x * 2.0f - 1.0f;
    float ndcY = -uv.y * 2.0f + 1.0f;
    float4 clipPos = float4(ndcX, ndcY, ndcDepth, 1.0f);
    // C++でXMMatrixTranspose済み → HLSLのcolumn_major → mul(mat,vec)で正しい変換
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
        f = saturate((dist - fogStart) / max(fogEnd - fogStart, 0.0001f));
    }
    else if (fogMode == 1)
    {
        f = 1.0f - exp(-fogDensity * dist);
    }
    else
    {
        float x = fogDensity * dist;
        f = 1.0f - exp(-(x * x));
    }
    return saturate(f);
}

float4 main(float4 pos : SV_POSITION) : SV_TARGET
{
    float2 uv = pos.xy / float2(resolutionX, resolutionY);

    float4 sceneColor = SceneColor.Sample(PointClamp, uv);
    float  rawDepth   = DepthTex.Sample(PointClamp, uv).r;

    if (rawDepth <= 0.0f)
        return sceneColor;

    float dist;
    float3 worldPos = float3(0, 0, 0);

    if (rawDepth >= 0.9999f)
    {
        dist = fogEnd;
    }
    else
    {
        float3 viewPos = ReconstructViewPos(uv, rawDepth);
        worldPos = ViewToWorld(viewPos);
        dist = length(worldPos - cameraPos);
    }

    float fogFactor = ComputeFogFactor(dist);

    if (fogHeight > 0.0f && rawDepth < 0.9999f)
    {
        float heightAbove = max(0.0f, worldPos.y - fogHeight);
        fogFactor *= exp(-heightAbove * heightFalloff);
    }

    fogFactor = saturate(fogFactor);
    float3 result = lerp(sceneColor.rgb, fogColor, fogFactor);
    return float4(result, sceneColor.a);
}
