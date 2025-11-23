cbuffer CameraBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Proj;
    float4 BaseColor;
};

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
    VS_OUTPUT o;
    float4 pos = float4(input.position, 1);
    float4 skinned = float4(0, 0, 0, 0);
    float3 n = float3(0, 0, 0);

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        uint bi = input.boneIndices[i];
        float w = input.boneWeights[i];
        if (w > 0 && bi < 256)
        {
            skinned += w * mul(pos, Bones[bi]);
            n += w * mul(input.normal, (float3x3) Bones[bi]);
        }
    }
    if (skinned.w == 0)
    {
        skinned = pos;
        n = input.normal;
    }
    skinned = mul(skinned, World);
    skinned = mul(skinned, View);
    o.position = mul(skinned, Proj);
    o.normal = normalize(mul(n, (float3x3) World));
    o.uv = input.uv;
    o.color = BaseColor;
    return o;
}