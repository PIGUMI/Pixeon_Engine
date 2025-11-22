// VS_ModelSkinned.hlsl - スキニングアニメーション用頂点シェーダー

// WVP行列用
cbuffer CameraBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Proj;
    float4 BaseColor;
};

// ボーン行列用（最大256ボーン）
cbuffer BoneBuffer : register(b1)
{
    matrix Bones[256];
};

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    uint4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    
    // スキニング計算
    float4 pos = float4(input.position, 1.0f);
    float4 skinnedPos = float4(0, 0, 0, 0);
    float3 skinnedNormal = float3(0, 0, 0);
    
    // 最大4つのボーンの影響を合成
    for (int i = 0; i < 4; ++i)
    {
        uint boneIndex = input.boneIndices[i];
        float weight = input.boneWeights[i];
        
        if (weight > 0.0f && boneIndex < 256)
        {
            // 位置の変換
            skinnedPos += weight * mul(pos, Bones[boneIndex]);
            
            // 法線の変換
            float3 n = mul(input.normal, (float3x3) Bones[boneIndex]);
            skinnedNormal += weight * n;
        }
    }
    
    // ウェイトが0の場合は元の位置を使用
    if (length(skinnedPos.xyz) < 0.001f)
    {
        skinnedPos = pos;
        skinnedNormal = input.normal;
    }
    
    // ワールド・ビュー・射影変換
    skinnedPos = mul(skinnedPos, World);
    skinnedPos = mul(skinnedPos, View);
    output.position = mul(skinnedPos, Proj);
    
    // 法線の変換
    skinnedNormal = normalize(skinnedNormal);
    output.normal = mul(skinnedNormal, (float3x3) World);
    
    // UV座標とカラーをそのまま渡す
    output.uv = input.uv;
    output.color = BaseColor;
    
    return output;
}