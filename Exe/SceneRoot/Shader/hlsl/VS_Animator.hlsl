// =======================================================
// VS_Animator.hlsl
// ボーンスキニング対応モデル用頂点シェーダー
// =======================================================

cbuffer CBFrame : register(b0)
{
    float4x4 gWorld;
    float4x4 gView;
    float4x4 gProj;
    float4   gBaseColor;
};

cbuffer CBBones : register(b1)
{
    float4x4 gBones[256];
};

struct VSInput
{
    float3 position     : POSITION;
    float3 normal       : NORMAL;
    float4 tangent      : TANGENT;
    float2 uv           : TEXCOORD0;
    uint4  boneIndices  : BLENDINDICES;
    float4 boneWeights  : BLENDWEIGHT;
};

struct VSOutput
{
    float4 svPos      : SV_POSITION;
    float3 normal     : NORMAL;       // ワールド空間法線 (既存ライティング用)
    float2 uv         : TEXCOORD0;
    float4 worldPos   : POSITION0;
    float4 color      : COLOR0;
    float3 viewNormal : VIEWNORMAL;   // ★追加: ビュー空間法線 (GBuffer/SSAO用)
};

float3x3 InverseTranspose3x3(float4x4 M)
{
    float3x3 A = (float3x3) M;
    float3 a0 = A[0];
    float3 a1 = A[1];
    float3 a2 = A[2];

    float3 c0 = cross(a1, a2);
    float3 c1 = cross(a2, a0);
    float3 c2 = cross(a0, a1);

    float det = dot(a0, c0);
    if (abs(det) < 1e-8f)
        return A;

    float invDet = 1.0f / det;
    float3x3 inv;
    inv[0] = c0 * invDet;
    inv[1] = c1 * invDet;
    inv[2] = c2 * invDet;

    float3x3 invT;
    invT[0] = float3(inv[0].x, inv[1].x, inv[2].x);
    invT[1] = float3(inv[0].y, inv[1].y, inv[2].y);
    invT[2] = float3(inv[0].z, inv[1].z, inv[2].z);
    return invT;
}

VSOutput main(VSInput IN)
{
    VSOutput OUT;

    // ボーン合成行列（線形ブレンド）
    float totalW = IN.boneWeights.x + IN.boneWeights.y + IN.boneWeights.z + IN.boneWeights.w;
    bool forceRoot = (totalW < 1e-7f);

    float4x4 skin = (float4x4) 0;
    if (forceRoot)
    {
        skin = gBones[0];
    }
    else
    {
        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            uint bi = IN.boneIndices[i];
            float w = IN.boneWeights[i];
            if (w > 0.0f)
                skin += gBones[bi] * w;
        }
    }

    // 位置スキン → ワールド変換
    float4 skinnedPos = mul(skin, float4(IN.position, 1.0f));
    float4 worldPos   = mul(skinnedPos, gWorld);
    OUT.worldPos      = worldPos;

    // クリップ座標
    float4 viewPos = mul(worldPos, gView);
    OUT.svPos      = mul(viewPos, gProj);

    // ワールド空間法線（既存ライティング用）
    float3 n      = normalize(mul(IN.normal, (float3x3) skin));
    OUT.normal    = normalize(mul(n, (float3x3) gWorld));

    // ビュー空間法線（GBuffer / SSAO用）
    // ワールド法線をView行列で変換 (w=0で平行移動を無視)
    OUT.viewNormal = mul(float4(OUT.normal, 0.0f), gView).xyz;

    OUT.uv    = IN.uv;
    OUT.color = gBaseColor;
    return OUT;
}
