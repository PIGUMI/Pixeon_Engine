// PS_SSAO.hlsl
// VS_Fullscreen (SV_VertexID方式) と組み合わせて使用する
// t1 = 深度バッファ SRV (DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
// t2 = 4x4 ランダムノイズ
// b1 = SSAO パラメーター
// b2 = カーネル (64 サンプル)

cbuffer SSAO_Params : register(b1)
{
    matrix gProj;       // カメラプロジェクション行列
    matrix gInvProj;    // プロジェクション逆行列
    float2 gResolution; // 画面解像度 (px)
    float  gRadius;     // サンプリング半径 (ビュー空間)
    float  gBias;       // 法線バイアス (アクネ防止)
    float  gPower;      // AO 強度 (pow の指数)
    float3 _pad;
};

cbuffer KernelCB : register(b2)
{
    float4 gKernel[64]; // 半球サンプルカーネル (xyz=方向, w=未使用)
};

Texture2D    gDepthTex   : register(t1); // 深度 SRV
Texture2D    gNoiseTex   : register(t2); // 4x4 ランダムノイズ
SamplerState gPointClamp : register(s0); // ポイントサンプリング (クランプ)
SamplerState gPointWrap  : register(s1); // ポイントサンプリング (ラップ, ノイズ用)

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

// 深度値からビュー空間座標を再構築
float3 ReconstructViewPos(float2 uv, float depth)
{
    float4 ndc = float4(uv.x * 2.0f - 1.0f, -(uv.y * 2.0f - 1.0f), depth, 1.0f);
    float4 vp  = mul(ndc, gInvProj);
    return vp.xyz / vp.w;
}

// 隣接ピクセルの深度差分から法線を推定
float3 ReconstructNormal(float2 uv, float3 viewPos)
{
    float2 ts = 1.0f / gResolution;
    float3 pR = ReconstructViewPos(uv + float2(ts.x,    0), gDepthTex.Sample(gPointClamp, uv + float2(ts.x,    0)).r);
    float3 pU = ReconstructViewPos(uv + float2(   0, ts.y), gDepthTex.Sample(gPointClamp, uv + float2(   0, ts.y)).r);
    return normalize(cross(pU - viewPos, pR - viewPos));
}

float main(PS_INPUT i) : SV_TARGET
{
    float depth = gDepthTex.Sample(gPointClamp, i.uv).r;

    // スカイボックス等 (far plane) はオクルージョンなし
    if (depth >= 1.0f) return 1.0f;

    float3 viewPos    = ReconstructViewPos(i.uv, depth);
    float3 viewNormal = ReconstructNormal(i.uv, viewPos);

    // 4x4 ノイズテクスチャでカーネルをランダム回転
    float2 noiseScale = gResolution / 4.0f;
    float3 randomVec  = normalize(gNoiseTex.Sample(gPointWrap, i.uv * noiseScale).xyz * 2.0f - 1.0f);

    // TBN 行列 (接線空間 -> ビュー空間)
    float3 tangent   = normalize(randomVec - viewNormal * dot(randomVec, viewNormal));
    float3 bitangent = cross(viewNormal, tangent);
    float3x3 TBN     = float3x3(tangent, bitangent, viewNormal);

    float occlusion = 0.0f;

    [unroll]
    for (int k = 0; k < 64; ++k)
    {
        // カーネル方向をビュー空間に変換しサンプル位置を決定
        float3 sp  = viewPos + mul(gKernel[k].xyz, TBN) * gRadius;

        // サンプル位置をスクリーン UV に変換
        float4 off = mul(float4(sp, 1.0f), gProj);
        off.xyz   /= off.w;
        float2 sUV = float2(off.x * 0.5f + 0.5f, -off.y * 0.5f + 0.5f);

        // 画面外はスキップ
        if (any(sUV < 0.0f) || any(sUV > 1.0f)) continue;

        // 実際の深度と比較してオクルージョンを加算
        float3 sVP = ReconstructViewPos(sUV, gDepthTex.Sample(gPointClamp, sUV).r);
        float  rc  = smoothstep(0.0f, 1.0f, gRadius / abs(viewPos.z - sVP.z));
        occlusion += (sVP.z >= sp.z + gBias ? 1.0f : 0.0f) * rc;
    }

    return pow(saturate(1.0f - occlusion / 64.0f), gPower);
}
