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
    LightGPU gLights[8]; // kMaxLights
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
    float3 worldPos : WORLDPOS; // ← ここを追加
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
    { // Directional
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
        { // Spot
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
    float3 P = i.worldPos; // ← ワールド座標を正しく取得

    float3 lighting = 0;
    [unroll]
    for (int li = 0; li < gLightCount; ++li)
    {
        lighting += ApplyLight(gLights[li], P, N);
    }

    // 環境光(適当)
    float3 ambient = 0.1 * gBaseColor.rgb;

    float3 color = (ambient + lighting) * texCol.rgb * gBaseColor.rgb;
    return float4(color, texCol.a * gBaseColor.a);
}