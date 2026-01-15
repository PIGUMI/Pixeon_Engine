cbuffer ModelCB : register(b0)
{
    matrix gWorld;
    matrix gView;
    matrix gProj;
    float4 gBaseColor;
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

Texture2D gBaseTex : register(t0);
SamplerState gLinear : register(s0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float3 worldPos : WORLDPOS;
    float3 viewPos : VIEWPOS;
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

float3 ApplyLight(LightGPU l, float3 P, float3 N)
{
    if (l.enabled < 0.5)
        return 0;
    float3 result = 0;
    if (l.type == 0)
    {
        float3 L = -normalize(l.direction);
        float ndl = saturate(dot(N, L));
        result = l.color * (ndl * l.intensity);
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
        if (l.type == 2)
        {
            float sf = SpotFactor(L, l.direction, l.innerCos, l.outerCos);
            att *= sf;
            if (att <= 0)
                return 0;
        }
        result = l.color * (ndl * l.intensity * att);
    }
    return result;
}

float4 main(PS_INPUT i) : SV_TARGET
{
    float4 texCol = gBaseTex.Sample(gLinear, i.uv);
    float3 N = normalize(i.normal);
    float3 P = i.worldPos;

    float3 lighting = 0;
    [unroll]
    for (int li = 0; li < gLightCount; ++li)
    {
        lighting += ApplyLight(gLights[li], P, N);
    }

    float3 ambient = 0.1 * gBaseColor.rgb;
    float3 color = (ambient + lighting) * texCol.rgb * gBaseColor.rgb;
    
    // ビュー空間でのZ距離（カメラからの深度）を使用
    float distToCamera = length(i.viewPos);
    
    // 透過パラメータ（カメラのNearPlane=0.1を考慮）
    float fadeStartDist = 1.5; // この距離から透明化開始
    float fadeEndDist = 0.5; // この距離で完全透明
    
    // 近いほど透明に（逆補間）
    float alphaByDistance = saturate((distToCamera - fadeEndDist) / (fadeStartDist - fadeEndDist));
    
    // スムーズな透過曲線を適用
    alphaByDistance = smoothstep(0.0, 1.0, alphaByDistance);
    
    float finalAlpha = texCol.a * gBaseColor.a * alphaByDistance;
    
    return float4(color, finalAlpha);
}