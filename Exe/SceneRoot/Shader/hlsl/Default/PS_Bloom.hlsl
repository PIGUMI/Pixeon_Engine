// PS_Bloom.hlsl
Texture2D texDiffuse : register(t0);
SamplerState samLinear : register(s0);

cbuffer BloomParams : register(b1)
{
    float threshold; // 輝度閾値
    float intensity; // Bloom強度
    float blurRadius; // ブラー半径
    float screenWidth; // スクリーン幅
    float screenHeight; // スクリーン高さ
    int passType; // 0: 輝度抽出, 1: ブラー, 2: 合成
    float2 pad;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// 輝度計算
float Luminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

// 輝度抽出パス
float4 BrightPass(float2 uv)
{
    float4 color = texDiffuse.Sample(samLinear, uv);
    float lum = Luminance(color.rgb);
    
    // 閾値以上の明るい部分のみ抽出
    float brightness = max(0.0f, lum - threshold);
    color.rgb *= brightness / (lum + 0.0001f);
    
    return color;
}

// ガウシアンブラー（簡易版）
float4 BlurPass(float2 uv)
{
    float2 texelSize = float2(1.0f / screenWidth, 1.0f / screenHeight);
    
    float4 color = float4(0, 0, 0, 0);
    float totalWeight = 0.0f;
    
    // 5x5カーネル
    const int kernelSize = 5;
    const float sigma = blurRadius;
    
    for (int y = -kernelSize / 2; y <= kernelSize / 2; y++)
    {
        for (int x = -kernelSize / 2; x <= kernelSize / 2; x++)
        {
            float2 offset = float2(x, y) * texelSize * blurRadius;
            
            // ガウス関数による重み計算
            float distance = length(float2(x, y));
            float weight = exp(-(distance * distance) / (2.0f * sigma * sigma));
            
            color += texDiffuse.Sample(samLinear, uv + offset) * weight;
            totalWeight += weight;
        }
    }
    
    return color / totalWeight;
}

// 合成パス（元画像とブルームを合成）
float4 CompositePass(float2 uv)
{
    float4 color = texDiffuse.Sample(samLinear, uv);
    return color * intensity;
}

float4 main(PS_INPUT input) : SV_Target
{
    if (passType == 0)
    {
        // 輝度抽出
        return BrightPass(input.uv);
    }
    else if (passType == 1)
    {
        // ブラー
        return BlurPass(input.uv);
    }
    else
    {
        // 合成
        return CompositePass(input.uv);
    }
}