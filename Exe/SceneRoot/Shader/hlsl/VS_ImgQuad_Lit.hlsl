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
        
        // Z軸方向の法線をワールド変換
        float3 localNormal = float3(0, 0, 1);
        float3 worldNormalX = normalize(World[0].xyz);
        float3 worldNormalY = normalize(World[1].xyz);
        float3 worldNormalZ = normalize(World[2].xyz);
        
        o.normal = normalize(
            localNormal.x * worldNormalX +
            localNormal.y * worldNormalY +
            localNormal.z * worldNormalZ
        );
    }
    else
    {
        // ★ ビルボード/UIモード - 完全修正版
        
        // 1. World行列から位置とスケールを抽出
        float3 worldPos = World[3].xyz; // 位置
        float scaleX = length(World[0].xyz); // Xスケール
        float scaleY = length(World[1].xyz); // Yスケール
        
        // 2. View行列の転置から、ビュー空間の右・上ベクトルを **ワールド空間** に変換
        // View行列は「ワールド→ビュー」変換なので、転置すれば「ビュー→ワールド」
        // 右ベクトル = View行列の第1列（転置後は第1行）
        float3 camRight = normalize(float3(View[0][0], View[1][0], View[2][0]));
        // 上ベクトル = View行列の第2列（転置後は第2行）
        float3 camUp = normalize(float3(View[0][1], View[1][1], View[2][1]));
        
        // 3. ローカル頂点位置をワールド空間でビルボード化（スケール適用）
        float3 billboardWorldPos = worldPos
            + camRight * (i.pos.x * scaleX)
            + camUp * (i.pos.y * scaleY);
        
        o.worldPos = billboardWorldPos;
        
        // 4. ワールド座標をビュー変換→プロジェクション変換
        float4 vpos = mul(float4(billboardWorldPos, 1.0f), View);
        o.pos = mul(vpos, Proj);
        
        // 5. カメラの前方向ベクトルを法線として使用（ビルボードは常にカメラを向く）
        // View行列の第3列（Z軸 = forward）の逆方向
        float3 camForward = float3(View[0][2], View[1][2], View[2][2]);
        o.normal = -normalize(camForward);
    }
    
    o.uv = i.uv;
    o.col = Color;
    return o;
}