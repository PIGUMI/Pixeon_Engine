cbuffer ModelCB : register(b0)
{
    matrix gWorld;
    matrix gView;
    matrix gProj;
    float4 gBaseColor;
};

cbuffer ShadowCB : register(b3)
{
    matrix gLightViewProj;
};

struct LightGPU
{
    float3 position;
    float intensity;
    float3 direction;
    float type;
    float3 color;
    float range;
    float innerCos;
    float outerCos;
    float enabled;
    float pad;
};

cbuffer LightArrayCB : register(b1)
{
    LightGPU gLights[8];
};

cbuffer LightCountCB : register(b2)
{
    int gLightCount;
    float3 _padLC;
};

cbuffer CameraCB : register(b4)
{
    float3 gCameraPos;
    float _padCam;
};

Texture2D gBaseTex : register(t0);
Texture2D gShadowMap : register(t1);
SamplerState gLinear : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float3 worldPos : WORLDPOS;
};

float AttenuationPoint(float dist, float range)
{
    float a = saturate(1.0 - dist / range);
    return a * a;
}

float SpotFactor(float3 L, float3 dir, float innerCos, float outerCos)
{
    float c = dot(-L, dir);
    if (c <= outerCos)
        return 0;
    if (c >= innerCos)
        return 1;
    float t = (c - outerCos) / (innerCos - outerCos);
    return saturate(t);
}

float CalculateShadow(float3 worldPos)
{
    float4 lightSpacePos = mul(float4(worldPos, 1.0f), gLightViewProj);
    
    lightSpacePos.xyz /= lightSpacePos.w;
    
    float2 shadowTexCoord;
    shadowTexCoord.x = lightSpacePos.x * 0.5f + 0.5f;
    shadowTexCoord.y = -lightSpacePos.y * 0.5f + 0.5f;
    

    if (shadowTexCoord.x < 0.0f || shadowTexCoord.x > 1.0f ||
        shadowTexCoord.y < 0.0f || shadowTexCoord.y > 1.0f)
        return 1.0f;
    
    float currentDepth = lightSpacePos.z;
    
  
    float shadow = 0.0f;
    float bias = 0.001f;
    
    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset = float2(x, y) * (1.0f / 4096.0f);
            shadow += gShadowMap.SampleCmpLevelZero(
                gShadowSampler,
                shadowTexCoord + offset,
                currentDepth - bias
            );
        }
    }
    shadow /= 9.0f;
    
    return shadow;
}

float3 ApplyLight(LightGPU l, float3 P, float3 N, float3 V, float shadow)
{
    if (l.enabled < 0.5)
        return 0;
        
    float3 result = 0;
    
    if (l.type == 0) // Directional Light
    {
        float3 L = -normalize(l.direction);
        float ndl = saturate(dot(N, L));
        
        // Blinn-Phong スペキュラー
        float3 H = normalize(L + V);
        float spec = pow(saturate(dot(N, H)), 32.0f) * 0.5f;
        
        result = l.color * l.intensity * shadow * (ndl + spec);
    }
    else
    {
        float3 Lvec = l.position - P;
        float dist = length(Lvec);
        if (dist > l.range)
            return 0;
            
        float3 L = Lvec / dist;
        float ndl = saturate(dot(N, L));
        if (ndl <= 0)
            return 0;
            
        float att = AttenuationPoint(dist, l.range);
        
        if (l.type == 2) // Spot Light
        {
            float sf = SpotFactor(L, l.direction, l.innerCos, l.outerCos);
            att *= sf;
            if (att <= 0)
                return 0;
        }
        
        // Blinn-Phong スペキュラー
        float3 H = normalize(L + V);
        float spec = pow(saturate(dot(N, H)), 32.0f) * 0.5f;
        
        result = l.color * l.intensity * att * (ndl + spec);
    }
    
    return result;
}

float4 main(PS_INPUT i) : SV_TARGET
{
    float4 texCol = gBaseTex.Sample(gLinear, i.uv);
    float3 N = normalize(i.normal);
    float3 P = i.worldPos;

    float shadow = CalculateShadow(P);

    float3 V = normalize(gCameraPos - P);

    float3 lighting = 0;
    [unroll]
    for (int li = 0; li < gLightCount; ++li)
    {
        float shadowFactor = (gLights[li].type == 0) ? shadow : 1.0f;
        lighting += ApplyLight(gLights[li], P, N, V, shadowFactor);
    }

    float3 baseColor = texCol.rgb * gBaseColor.rgb;
    float3 ambient = 0.1 * baseColor;
    float3 color = ambient + lighting * baseColor;
    
    return float4(color, texCol.a * gBaseColor.a);
}