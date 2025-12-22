Texture2D gHDRTex : register(t0);
SamplerState gSamp : register(s0);

cbuffer TonemapCB : register(b0)
{
    float exposure;
    float gamma;
    float pad1, pad2;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// ACES Filmic Tone Mapping
float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 main(PS_IN input) : SV_Target
{
    float3 hdrColor = gHDRTex.Sample(gSamp, input.uv).rgb;
    
    // 露出調整
    hdrColor *= exposure;
    
    // トーンマッピング
    float3 color = ACESFilm(hdrColor);
    
    // ガンマ補正
    color = pow(color, 1.0 / gamma);
    
    return float4(color, 1.0);
}