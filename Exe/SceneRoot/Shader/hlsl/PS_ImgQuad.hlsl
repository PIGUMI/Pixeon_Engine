Texture2D gTex0 : register(t0);
SamplerState gSamp : register(s0);

struct PS_IN
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 col : COLOR0;
};

float4 main(PS_IN i) : SV_Target
{
    float4 tex = gTex0.Sample(gSamp, i.uv);
    return tex * i.col;
}