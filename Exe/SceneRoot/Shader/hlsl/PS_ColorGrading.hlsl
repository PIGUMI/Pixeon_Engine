// PS_ColorGrading.hlsl
Texture2D mainTexture : register(t0);
SamplerState mainSampler : register(s0);

cbuffer ColorGradingParams : register(b0)
{
    float brightness;      // 明度 (-1.0 ~ 1.0)
    float contrast;        // コントラスト (0.0 ~ 2.0)
    float saturation;      // 彩度 (0.0 ~ 2.0)
    float hueShift;        // 色相シフト (0.0 ~ 360.0)
    
    float temperature;     // 色温度 (-1.0 ~ 1.0)
    float tint;           // 色合い (-1.0 ~ 1.0)
    float gamma;          // ガンマ補正 (0.1 ~ 3.0)
    float padding;
};

// RGB to HSV 変換
float3 RGBtoHSV(float3 rgb)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = lerp(float4(rgb.bg, K.wz), float4(rgb.gb, K.xy), step(rgb.b, rgb.g));
    float4 q = lerp(float4(p.xyw, rgb.r), float4(rgb.r, p.yzx), step(p.x, rgb.r));
    
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// HSV to RGB 変換
float3 HSVtoRGB(float3 hsv)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(hsv.xxx + K.xyz) * 6.0 - K.www);
    return hsv.z * lerp(K.xxx, saturate(p - K.xxx), hsv.y);
}

// 輝度計算
float Luminance(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    // 元の色を取得
    float4 color = mainTexture.Sample(mainSampler, input.uv);
    float3 rgb = color.rgb;
    
    // 1. 明度調整
    rgb += brightness;
    
    // 2. コントラスト調整
    rgb = ((rgb - 0.5) * contrast) + 0.5;
    
    // 3. 彩度調整
    float lum = Luminance(rgb);
    rgb = lerp(float3(lum, lum, lum), rgb, saturation);
    
    // 4. 色相シフト
    if (hueShift != 0.0)
    {
        float3 hsv = RGBtoHSV(rgb);
        hsv.x = frac(hsv.x + hueShift / 360.0);
        rgb = HSVtoRGB(hsv);
    }
    
    // 5. 色温度調整
    if (temperature != 0.0)
    {
        if (temperature > 0.0)
        {
            // 暖色方向
            rgb.r = lerp(rgb.r, rgb.r * (1.0 + temperature * 0.3), temperature);
            rgb.b = lerp(rgb.b, rgb.b * (1.0 - temperature * 0.2), temperature);
        }
        else
        {
            // 寒色方向
            float temp = -temperature;
            rgb.b = lerp(rgb.b, rgb.b * (1.0 + temp * 0.3), temp);
            rgb.r = lerp(rgb.r, rgb.r * (1.0 - temp * 0.2), temp);
        }
    }
    
    // 6. 色合い調整 (マゼンタ-グリーン)
    if (tint != 0.0)
    {
        if (tint > 0.0)
        {
            // マゼンタ方向
            rgb.r = lerp(rgb.r, rgb.r * (1.0 + tint * 0.2), tint);
            rgb.b = lerp(rgb.b, rgb.b * (1.0 + tint * 0.2), tint);
            rgb.g = lerp(rgb.g, rgb.g * (1.0 - tint * 0.1), tint);
        }
        else
        {
            // グリーン方向
            float t = -tint;
            rgb.g = lerp(rgb.g, rgb.g * (1.0 + t * 0.3), t);
            rgb.r = lerp(rgb.r, rgb.r * (1.0 - t * 0.1), t);
            rgb.b = lerp(rgb.b, rgb.b * (1.0 - t * 0.1), t);
        }
    }
    
    // 7. ガンマ補正
    if (gamma != 1.0)
    {
        rgb = pow(abs(rgb), 1.0 / gamma);
    }
    
    // 値を0-1にクランプ
    rgb = saturate(rgb);
    
    return float4(rgb, color.a);
}
