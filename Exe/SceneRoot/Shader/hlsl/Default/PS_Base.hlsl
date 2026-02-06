cbuffer MaterialCB : register(b0)
{
    float4 LineColor;
};
struct PS_INPUT {
    float4 pos   : SV_POSITION;
    float4 color : COLOR0;
};
float4 main(PS_INPUT input) : SV_Target {
    // 頂点色とMaterial色の乗算例
    return input.color * LineColor;
    return float4(1, 0, 0, 1); // 赤色で塗りつぶし
}
