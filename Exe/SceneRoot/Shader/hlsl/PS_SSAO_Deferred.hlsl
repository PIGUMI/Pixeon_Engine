// =======================================================
// PS_SSAO_Deferred.hlsl
// GBuffer の正確な法線を使った SSAO
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

Texture2D    gNormalTex : register(t0);
Texture2D    gDepthTex  : register(t1);
Texture2D    gNoiseTex  : register(t2);

SamplerState gPointClamp : register(s0);
SamplerState gPointWrap  : register(s1);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

float3 ReconstructViewPos(float2 uv, float depth)
{
    float2 ndc;
    ndc.x =  uv.x * 2.0 - 1.0;
    ndc.y = -uv.y * 2.0 + 1.0;
    float4 clipPos = float4(ndc, depth, 1.0);
    float4 viewPos = mul(clipPos, gInvProj);
    return viewPos.xyz / viewPos.w;
}

float4 main(PS_INPUT i) : SV_TARGET
{
    float depth = gDepthTex.Sample(gPointClamp, i.uv).r;

    if (depth >= 1.0f)
        return float4(1.0f, 1.0f, 1.0f, 1.0f);

    // ビュー空間法線を GBuffer から復元
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

    float occlusion = 0.0;
    const int KERNEL_SIZE = 64;

    [unroll]
    for (int k = 0; k < KERNEL_SIZE; ++k)
    {
        // カーネルをビュー空間に変換してサンプル位置を計算
        float3 s = mul(gKernel[k].xyz, TBN);
        float3 samplePos3D = fragPos + s * gRadius;

        // ビュー空間 → クリップ空間 → UV
        float4 offset = mul(float4(samplePos3D, 1.0), gProj);
        offset.xy /= offset.w;
        float2 sampleUV;
        sampleUV.x =  offset.x * 0.5 + 0.5;
        sampleUV.y = -offset.y * 0.5 + 0.5;

        // UV範囲チェック
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 ||
            sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        float sampleDepth = gDepthTex.Sample(gPointClamp, sampleUV).r;
        float3 sampleGeomPos = ReconstructViewPos(sampleUV, sampleDepth);

        // ビュー空間ではカメラから遠いほどZが大きい（左手系）
        // サンプル位置よりジオメトリが同じかより遠い（Zが大きい）→ 遮蔽されていない
        // サンプル位置よりジオメトリが手前（Zが小さい）→ 遮蔽
        float rangeCheck = smoothstep(0.0, 1.0, gRadius / max(abs(fragPos.z - sampleGeomPos.z), 0.0001));
        occlusion += (sampleGeomPos.z >= samplePos3D.z + gBias ? 0.0 : 1.0) * rangeCheck;
    }

    // occlusion: 0=開放, 1=完全遮蔽 → ao: 1=開放, 0=遮蔽
    occlusion = occlusion / float(KERNEL_SIZE);
    float ao = pow(saturate(1.0 - occlusion), gPower);

    return float4(ao, ao, ao, 1.0);
}
