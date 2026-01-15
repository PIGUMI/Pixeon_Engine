Texture2D gTex0 : register(t0);
SamplerState gSamp : register(s0);

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

struct PS_IN
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 col : COLOR0;
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL;
    bool isFrontFace : SV_IsFrontFace; // í«â¡ÅFï\ó†îªíË
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
        // Directional
        float3 L = -normalize(l.direction);
        float ndl = saturate(abs(dot(N, L))); // abs()Ç≈óºñ ëŒâû
        result = l.color * (ndl * l.intensity);
    }
    else
    {
        float3 Lvec = l.position - P;
        float dist = length(Lvec);
        if (dist > l.range)
            return 0;
        
        float3 L = Lvec / dist;
        float ndl = saturate(abs(dot(N, L))); // abs()Ç≈óºñ ëŒâû
        if (ndl <= 0)
            return 0;
        
        float att = AttenuationPoint(dist, l.range);
        
        if (l.type == 2)
        {
            // Spot
            float sf = SpotFactor(L, l.direction, l.innerCos, l.outerCos);
            att *= sf;
            if (att <= 0)
                return 0;
        }
        
        result = l.color * (ndl * l.intensity * att);
    }
    
    return result;
}

float4 main(PS_IN i) : SV_Target
{
    float4 tex = gTex0.Sample(gSamp, i.uv);
    
    if (tex.a * i.col.a < 0.01)
        discard;
    
    float3 N = normalize(i.normal);
    if (!i.isFrontFace)
    {
        N = -N;
    }
    
    float3 P = i.worldPos;
    
    float3 lighting = 0;
    [unroll]
    for (int li = 0; li < gLightCount; ++li)
    {
        lighting += ApplyLight(gLights[li], P, N);
    }
    
    float3 ambient = 0.1 * i.col.rgb;
    
    float3 color = (ambient + lighting) * tex.rgb * i.col.rgb;
    
    return float4(color, tex.a * i.col.a);
}