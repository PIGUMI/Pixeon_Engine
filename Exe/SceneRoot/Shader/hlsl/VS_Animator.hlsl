// =======================================================
// VS_ModelSkinned.hlsl
// ボーンスキニング対応モデル用頂点シェーダ
// 入力要素: POSITION, NORMAL, TANGENT, TEXCOORD, BLENDINDICES, BLENDWEIGHT
// 定数バッファ b0: World / View / Proj / BaseColor
// 定数バッファ b1: Bone 行列配列（final = InverseBindPose * CurrentGlobal）
//   CPU側で既に final 行列を計算し gBones[i] に転置せず格納 → ここで転置不要
//   もし CPU 側で転置済みを渡している場合はそのまま mul して問題なし
// =======================================================

cbuffer CBFrame : register(b0)
{
    float4x4 gWorld;
    float4x4 gView;
    float4x4 gProj;
    float4 gBaseColor;
};

// 256 本まで想定（必要なら縮小 / 拡張）
cbuffer CBBones : register(b1)
{
    float4x4 gBones[256];
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    uint4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};

struct VSOutput
{
    float4 svPos : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 worldPos : POSITION0;
    float4 color : COLOR0;
};

float3x3 InverseTranspose3x3(float4x4 M)
{
    // 非一様スケール時の法線補正（GPUコスト許容なら使用）
    float3x3 A = (float3x3) M;
    // 行列の逆転置
    // 直接 inverse(A) を計算する簡易版
    float3 a0 = A[0];
    float3 a1 = A[1];
    float3 a2 = A[2];

    float3 c0 = cross(a1, a2);
    float3 c1 = cross(a2, a0);
    float3 c2 = cross(a0, a1);

    float det = dot(a0, c0);
    // det が極端に小さい時はそのまま（破綻防止）
    if (abs(det) < 1e-8f)
        return A;

    float invDet = 1.0f / det;
    float3x3 inv;
    inv[0] = c0 * invDet;
    inv[1] = c1 * invDet;
    inv[2] = c2 * invDet;

    // 逆転置なので最後に転置
    float3x3 invT;
    invT[0] = float3(inv[0].x, inv[1].x, inv[2].x);
    invT[1] = float3(inv[0].y, inv[1].y, inv[2].y);
    invT[2] = float3(inv[0].z, inv[1].z, inv[2].z);
    return invT;
}

VSOutput main(VSInput IN)
{
    VSOutput OUT;

    // --- ボーン合成行列（線形ブレンド）---
    // 重みが全て 0 の頂点への保険
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
            {
                skin += gBones[bi] * w;
            }
        }
    }

    // 位置スキン
    float4 skinnedPos = mul(skin, float4(IN.position, 1.0f));

    // ワールド変換
    float4 worldPos = mul(skinnedPos, gWorld);
    OUT.worldPos = worldPos;

    // クリップ座標
    float4 viewPos = mul(worldPos, gView);
    OUT.svPos = mul(viewPos, gProj);

    // 法線スキン
    // （簡易）skin * world で変換後 normalize
    // 非一様スケール対応が必要なら InverseTranspose3x3(world * skin) を使う
    float3 n = IN.normal;
    // float3x3 N = InverseTranspose3x3(mul(skin, gWorld)); // より正確
    // OUT.normal = normalize(mul(n, N));
    OUT.normal = normalize(mul(n, (float3x3) skin));
    OUT.normal = normalize(mul(OUT.normal, (float3x3) gWorld));

    OUT.uv = IN.uv;
    OUT.color = gBaseColor;
    return OUT;
}