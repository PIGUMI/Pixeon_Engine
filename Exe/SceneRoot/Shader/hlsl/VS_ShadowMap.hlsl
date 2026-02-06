cbuffer ShadowCB : register(b0)
{
    matrix gLightViewProj;
    matrix gWorld;
};

// ボーン行列追加
cbuffer CBBones : register(b1)
{
    float4x4 gBones[256];
};

struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
    uint4 bi : BLENDINDICES;
    float4 bw : BLENDWEIGHT;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float depth : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    
    // ボーンスキニング処理
    float totalW = input.bw.x + input.bw.y + input.bw.z + input.bw.w;
    float4 skinnedPos;
    
    if (totalW < 1e-7f)
    {
        // ウェイトがない場合はそのまま
        skinnedPos = float4(input.pos, 1.0f);
    }
    else
    {
        float4x4 skin = (float4x4) 0;
        [unroll]
        for (int i = 0; i < 4; ++i)
        {
            uint bi = input.bi[i];
            float w = input.bw[i];
            if (w > 0.0f)
            {
                skin += gBones[bi] * w;
            }
        }
        skinnedPos = mul(skin, float4(input.pos, 1.0f));
    }
    
    float4 worldPos = mul(skinnedPos, gWorld);
    output.pos = mul(worldPos, gLightViewProj);
    output.depth = output.pos.z / output.pos.w;
    
    return output;
}