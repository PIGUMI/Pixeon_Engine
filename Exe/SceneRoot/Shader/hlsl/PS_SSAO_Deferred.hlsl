// =======================================================
// PS_SSAO_Deferred.hlsl
// GBuffer の正確な法線を使った SSAO
//
// 入力:
//   t0 = Normal GBuffer (RGBA16F, ビュー空間法線 [0,1]圧縮)
//   t1 = Depth  GBuffer (R24)
//   t2 = Noise  (4x4 ランダム回転テクスチャ)
//
// 出力:
//   AO値 (R8, 0=完全遮蔽, 1=完全開放)
//
// 既存 PS_SSAO との違い:
//   旧: 深度差分から法線を推定 → 精度低い
//   新: GBuffer の正確なビュー空間法線を使用 → 精度高い
// =======================================================

cbuffer SSAO_CB : register(b1)
{
    matrix gProj;
    matrix gInvProj;
    float2 gResolution;
    float  gRadius;
    float  gBias;
    float  gPower;
    float3 _pad;
};

cbuffer KernelCB : register(b2)
{
    float4 gKernel[64];
};

Texture2D    gNormalTex : register(t0);  // GBuffer Normal [0,1]
Texture2D    gDepthTex  : register(t1);  // GBuffer Depth
Texture2D    gNoiseTex  : register(t2);  // ランダムノイズ

SamplerState gPointClamp : register(s0);
SamplerState gPointWrap  : register(s1);  // ノイズテクスチャはWrap

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

// --------------------------------------------------------
// 深度 + UV からビュー空間座標を再構築
// --------------------------------------------------------
float3 ReconstructViewPos(float2 uv, float depth)
{
    float2 ndc;
    ndc.x =  uv.x * 2.0 - 1.0;
    ndc.y = -uv.y * 2.0 + 1.0;
    float4 clipPos = float4(ndc, depth, 1.0);
    float4 viewPos = mul(clipPos, gInvProj);
    return viewPos.xyz / viewPos.w;
}

// --------------------------------------------------------
// メイン
// --------------------------------------------------------
float4 main(PS_INPUT i) : SV_TARGET
{
    float depth = gDepthTex.Sample(gPointClamp, i.uv).r;

    // 未描画ピクセルは AO = 1.0（遮蔽なし）
    if (depth >= 1.0f)
        return float4(1.0f, 1.0f, 1.0f, 1.0f);

    // ビュー空間法線を GBuffer から復元
    // [0,1] → [-1,1] → normalize
    float3 normal = normalize(gNormalTex.Sample(gPointClamp, i.uv).rgb * 2.0 - 1.0);

    // ビュー空間座標
    float3 fragPos = ReconstructViewPos(i.uv, depth);

    // ノイズ（4x4テクスチャ→タイリング）
    float2 noiseScale = gResolution / 4.0;
    float3 randomVec  = normalize(gNoiseTex.Sample(gPointWrap, i.uv * noiseScale).xyz * 2.0 - 1.0);

    // TBN でカーネルをビュー空間に変換
    float3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, normal);

    // カーネルサンプリング
    float occlusion = 0.0;
    const int KERNEL_SIZE = 64;

    [unroll]
    for (int k = 0; k < KERNEL_SIZE; ++k)
    {
        // カーネルをビュー空間に変換
        float3 s = mul(gKernel[k].xyz, TBN);
        s = fragPos + s * gRadius;

        // ビュー空間 → クリップ空間 → UV
        float4 offset = mul(float4(s, 1.0), gProj);
        offset.xy /= offset.w;
        float2 sampleUV;
        sampleUV.x =  offset.x * 0.5 + 0.5;
        sampleUV.y = -offset.y * 0.5 + 0.5;

        // UV範囲チェック
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 ||
            sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        float sampleDepth = gDepthTex.Sample(gPointClamp, sampleUV).r;
        float3 samplePos  = ReconstructViewPos(sampleUV, sampleDepth);

        // 深度比較（サンプルが実ジオメトリより手前にあれば遮蔽）
        float rangeCheck = smoothstep(0.0, 1.0, gRadius / abs(fragPos.z - samplePos.z));
        occlusion += (samplePos.z >= s.z + gBias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(KERNEL_SIZE));
    float ao = pow(occlusion, gPower);

    return float4(ao, ao, ao, 1.0);
}
