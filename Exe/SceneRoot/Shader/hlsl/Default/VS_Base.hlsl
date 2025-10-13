cbuffer CameraCB : register(b0)
{
    matrix world;
    matrix view;
    matrix proj;
};

struct VS_INPUT {
    float3 pos   : POSITION;
    float4 color : COLOR0;
};
struct VS_OUTPUT {
    float4 pos   : SV_POSITION;
    float4 color : COLOR0;
};
VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT o;
    float4 p = float4(input.pos, 1.0f);
    p = mul(p, world);
    p = mul(p, view);
    o.pos = mul(p, proj);
    o.color = input.color;
    return o;
}
