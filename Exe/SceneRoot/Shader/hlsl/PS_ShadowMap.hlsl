struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float depth : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    return float4(input.depth, input.depth, input.depth, 1.0f);
}