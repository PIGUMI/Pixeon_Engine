Texture2D gTex0 : register(t0);
SamplerState gSamp : register(s0);

cbuffer PS_CB : register(b1)
{
    float pixelSize;
    float screenWidth;
    float screenHeight;
    float intensity;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 col : COLOR0;
};

float4 main(PS_IN i) : SV_Target
{
    float2 pixelatedUV;
    
    float2 pixelCoord = i.uv * float2(screenWidth, screenHeight);

    float2 gridCoord = floor(pixelCoord / pixelSize) * pixelSize;
    
    float2 centerCoord = gridCoord + (pixelSize * 0.5);
    
    pixelatedUV = centerCoord / float2(screenWidth, screenHeight);
    
    float4 pixelatedColor = gTex0.Sample(gSamp, pixelatedUV);
    float4 originalColor = gTex0.Sample(gSamp, i.uv);
    
    float4 result = lerp(originalColor, pixelatedColor, intensity);
    result *= i.col;
    
    if (result.a < 0.01)
        discard;
    
    return result;
}