cbuffer PerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gView;
    float4x4 gProj;
};

cbuffer Bones : register(b1)
{
    float4x4 gBoneMatrices[256];
};

struct VSInput
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    uint4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
};

VSOutput main(VSInput IN)
{
    float4 skinned = float4(0, 0, 0, 0);
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        float w = IN.boneWeights[i];
        if (w > 0)
        {
            skinned += w * mul(gBoneMatrices[IN.boneIndices[i]], float4(IN.pos, 1));
        }
    }
    float4 worldPos = mul(gWorld, skinned);
    VSOutput OUT;
    OUT.pos = mul(gProj, mul(gView, worldPos));
    OUT.normal = IN.normal; // 簡易: 法線スキニングは省略
    return OUT;
}