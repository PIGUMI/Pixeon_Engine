cbuffer VS_CB : register(b0)
{
    float4x4 World;
    float4x4 View;
    float4x4 Proj;
    float4 Color;
    int mode2D;
    float3 _pad;
};

struct VS_IN
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct VS_OUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 col : COLOR0;
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL;
};

VS_OUT main(VS_IN i)
{
    VS_OUT o;
    
    if (mode2D == 1)
    {
        // 2D配置モード（スクリーン座標）
        o.pos = float4(i.pos, 1.0f);
        o.worldPos = float3(0, 0, 0);
        o.normal = float3(0, 0, -1);
    }
    else if (mode2D == 2)
    {
        // 3D配置モード（World3D）
        float4 wpos = mul(float4(i.pos, 1.0f), World);
        o.worldPos = wpos.xyz;
        
        float4 vpos = mul(wpos, View);
        o.pos = mul(vpos, Proj);
        
        // ローカル法線 (0, 0, 1) をワールド変換
        float3 localNormal = float3(0, 0, 1);
        
        // ワールド行列の回転部分から法線を計算（スケール除去）
        float3 worldNormalX = normalize(World[0].xyz);
        float3 worldNormalY = normalize(World[1].xyz);
        float3 worldNormalZ = normalize(World[2].xyz);
        
        // ローカル法線をワールド空間に変換
        o.normal = normalize(
            localNormal.x * worldNormalX +
            localNormal.y * worldNormalY +
            localNormal.z * worldNormalZ
        );
    }
    else
    {
        // ビルボード/UIモード - カメラに向かう法線
        float4 wpos = mul(float4(i.pos, 1.0f), World);
        o.worldPos = wpos.xyz;
        
        float4 vpos = mul(wpos, View);
        o.pos = mul(vpos, Proj);
        
        float3 camPosWorld = float3(
            -dot(View[0].xyz, View[3].xyz),
            -dot(View[1].xyz, View[3].xyz),
            -dot(View[2].xyz, View[3].xyz)
        );
        
        // ★ ビルボードの位置からカメラへ向かうベクトルを法線とする
        o.normal = normalize(camPosWorld - o.worldPos);
    }
    
    o.uv = i.uv;
    o.col = Color;
    return o;
}