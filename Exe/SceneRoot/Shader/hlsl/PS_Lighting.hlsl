// =======================================================
// PS_Lighting.hlsl
// Deferred Lighting Pass 用ピクセルシェーダー
//
// 入力 (GBuffer):
//   t0 = Albedo  (RGBA8)    RGB=テクスチャ色
//   t1 = Normal  (RGBA16F)  RGB=ビュー空間法線 [0,1]圧縮
//   t2 = Depth   (R24)      深度値
//   t3 = SSAO    (R8)       AO値（後のステップで追加、今は1.0固定）
//   t4 = ShadowMap
//
// 定数バッファ:
//   b0 = LightingCB  (InvProj / InvView / カメラ情報)
//   b1 = LightArrayCB
//   b2 = LightCountCB
//   b3 = ShadowCB
// =======================================================

// --------------------------------------------------------
// 定数バッファ
// --------------------------------------------------------
cbuffer LightingCB : register(b0)
{
    matrix gInvProj;
    matrix gInvView;
    matrix gView;
    float3 gCameraPos;
    float  _pad0;
    float2 gResolution;
    float  gAOStrength;
    float  _pad1;
};

struct LightGPU
{
    float3 position;
    float  intensity;
    float3 direction;
    float  type;
    float3 color;
    float  range;
    float  innerCos;
    float  outerCos;
    float  enabled;
    float  pad;
};

cbuffer LightArrayCB : register(b1)
{
    LightGPU gLights[8];
};

cbuffer LightCountCB : register(b2)
{
    int   gLightCount;
    float3 _padLC;
};

cbuffer ShadowCB : register(b3)
{
    matrix gLightViewProj;
};

// --------------------------------------------------------
// テクスチャ
// --------------------------------------------------------
Texture2D gAlbedoTex    : register(t0);  // GBuffer Albedo
Texture2D gNormalTex    : register(t1);  // GBuffer Normal (ビュー空間 [0,1])
Texture2D gDepthTex     : register(t2);  // GBuffer Depth
Texture2D gSSAOTex      : register(t3);  // SSAO (ステップ5で有効化)
Texture2D gShadowMap    : register(t4);  // シャドウマップ

SamplerState              gPointClamp   : register(s0);
SamplerComparisonState    gShadowSampler: register(s1);

// --------------------------------------------------------
// VS_Fullscreen からの入力
// --------------------------------------------------------
struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

// --------------------------------------------------------
// ユーティリティ関数
// --------------------------------------------------------

// 深度とUVからビュー空間座標を再構築
float3 ReconstructViewPos(float2 uv, float depth)
{
    // NDC座標 [-1,1]
    float2 ndc;
    ndc.x =  uv.x * 2.0 - 1.0;
    ndc.y = -uv.y * 2.0 + 1.0;  // Y反転

    float4 clipPos = float4(ndc, depth, 1.0);
    float4 viewPos = mul(clipPos, gInvProj);
    return viewPos.xyz / viewPos.w;
}

// ビュー空間座標からワールド座標を復元
float3 ViewToWorld(float3 viewPos)
{
    float4 wp = mul(float4(viewPos, 1.0), gInvView);
    return wp.xyz;
}

// シャドウ計算（PS_ModelStatic.hlsl と同じ実装）
float CalculateShadow(float3 worldPos)
{
    float4 lightSpacePos = mul(float4(worldPos, 1.0f), gLightViewProj);
    lightSpacePos.xyz /= lightSpacePos.w;

    float2 shadowTexCoord;
    shadowTexCoord.x =  lightSpacePos.x * 0.5f + 0.5f;
    shadowTexCoord.y = -lightSpacePos.y * 0.5f + 0.5f;

    if (shadowTexCoord.x < 0.0f || shadowTexCoord.x > 1.0f ||
        shadowTexCoord.y < 0.0f || shadowTexCoord.y > 1.0f)
        return 1.0f;

    float currentDepth = lightSpacePos.z;
    float shadow = 0.0f;
    float bias   = 0.001f;

    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset = float2(x, y) * (1.0f / 8192.0f);
            shadow += gShadowMap.SampleCmpLevelZero(
                gShadowSampler,
                shadowTexCoord + offset,
                currentDepth - bias
            );
        }
    }
    return shadow / 9.0f;
}

// 減衰関数（PS_ModelStatic.hlsl と同じ）
float AttenuationPoint(float dist, float range)
{
    float a = saturate(1.0 - dist / range);
    return a * a;
}

float SpotFactor(float3 L, float3 dir, float innerCos, float outerCos)
{
    float c = dot(-L, dir);
    if (c <= outerCos) return 0;
    if (c >= innerCos) return 1;
    return saturate((c - outerCos) / (innerCos - outerCos));
}

// ライティング計算（PS_ModelStatic.hlsl の ApplyLight と同じ）
float3 ApplyLight(LightGPU l, float3 P, float3 N, float3 V, float shadow)
{
    if (l.enabled < 0.5) return 0;

    float3 result = 0;

    if (l.type == 0) // Directional
    {
        float3 L   = -normalize(l.direction);
        float  ndl = saturate(dot(N, L));
        float3 H   = normalize(L + V);
        float  spec = pow(saturate(dot(N, H)), 32.0f) * 0.5f;
        result = l.color * l.intensity * shadow * (ndl + spec);
    }
    else
    {
        float3 Lvec = l.position - P;
        float  dist = length(Lvec);
        if (dist > l.range) return 0;

        float3 L   = Lvec / dist;
        float  ndl = saturate(dot(N, L));
        if (ndl <= 0) return 0;

        float att = AttenuationPoint(dist, l.range);

        if (l.type == 2) // Spot
        {
            float sf = SpotFactor(L, l.direction, l.innerCos, l.outerCos);
            att *= sf;
            if (att <= 0) return 0;
        }

        float3 H    = normalize(L + V);
        float  spec = pow(saturate(dot(N, H)), 32.0f) * 0.5f;
        result = l.color * l.intensity * att * (ndl + spec);
    }

    return result;
}

// --------------------------------------------------------
// メイン
// --------------------------------------------------------
float4 main(PS_INPUT i) : SV_TARGET
{
    // GBuffer からサンプリング
    float4 albedoSample = gAlbedoTex.Sample(gPointClamp, i.uv);
    float4 normalSample = gNormalTex.Sample(gPointClamp, i.uv);
    float  depth        = gDepthTex.Sample(gPointClamp, i.uv).r;

    // 深度が1.0（未描画ピクセル）は完全透明で返す
    // → 下のレイヤーや背景が透けて見える
    if (depth >= 1.0f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);  // alpha=0 = 完全透明

    // アルベド
    float3 albedo = albedoSample.rgb;

    // ビュー空間法線を復元: [0,1] → [-1,1] → normalize
    float3 viewNormal = normalize(normalSample.rgb * 2.0 - 1.0);

    // ワールド空間法線（ライティングはワールド空間で計算）
    float3 N = normalize(mul(float4(viewNormal, 0.0), gInvView).xyz);

    // ビュー空間座標 → ワールド空間座標
    float3 viewPos   = ReconstructViewPos(i.uv, depth);
    float3 worldPos  = ViewToWorld(viewPos);

    // カメラ方向（ワールド空間）
    float3 V = normalize(gCameraPos - worldPos);

    // SSAO（LightingPass::SetSSAOSRV() でセットされた AO テクスチャ）
    // セットされていない場合は白テクスチャ（ao=1.0）が使われる
    float aoRaw = gSSAOTex.Sample(gPointClamp, i.uv).r;
    float ao = lerp(1.0f, aoRaw, gAOStrength);

    // シャドウ
    float shadow = CalculateShadow(worldPos);

    // ライティング計算
    float3 lighting = 0;
    [unroll]
    for (int li = 0; li < gLightCount; ++li)
    {
        float shadowFactor = (gLights[li].type == 0) ? shadow : 1.0f;
        lighting += ApplyLight(gLights[li], worldPos, N, V, shadowFactor);
    }

    // アンビエント（AO適用）
    float3 ambient = 0.1 * albedo * ao;   // ← AO はここだけ

    // 最終カラー
    // alpha=1.0 固定：このピクセルはモデルが描画されている
    // → 最終合成でこのレイヤーが正しく上のレイヤーとして合成される
    float3 color = ambient + lighting * albedo;
    return float4(color, 1.0f);
}
