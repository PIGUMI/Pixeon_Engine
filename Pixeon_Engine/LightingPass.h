#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

class GBuffer;

// ============================================================
// LightingPass
//
// GBuffer の Albedo / Normal / Depth を読み込んで
// ライティング計算を行い、最終カラーを出力するクラス
//
// 使い方:
//   Init()       → 起動時に1回
//   Execute()    → フレームごとに Geometry Pass の後に呼ぶ
// ============================================================
class LightingPass
{
public:
	bool Init(ID3D11Device* device);
	void Release();

	// Lighting Pass を実行して output RTV に書き込む
	// gbuffer    : Geometry Pass で書き込んだ GBuffer
	// outputRTV  : 書き込み先 RTV（layerRT の RTV）
	// width/height: レンダリング解像度
	// proj       : カメラのプロジェクション行列
	// view       : カメラのビュー行列
	// cameraPos  : カメラのワールド座標
	void Execute(
		ID3D11DeviceContext* ctx,
		GBuffer* gbuffer,
		ID3D11RenderTargetView* outputRTV,
		ID3D11ShaderResourceView* shadowMapSRV,
		int width, int height,
		const DirectX::XMMATRIX& proj,
		const DirectX::XMMATRIX& view,
		const DirectX::XMFLOAT3& cameraPos
	);

	// SSAO テクスチャをセット
	void SetSSAOSRV(ID3D11ShaderResourceView* srv) { m_ssaoSRV = srv; }

	// ライト定数バッファをセット（Scene::UploadLightsToGPU()の後に呼ぶ）
	void SetLightBuffers(ID3D11Buffer* lightArrayCB, ID3D11Buffer* lightCountCB)
	{
		m_lightArrayCB = lightArrayCB;
		m_lightCountCB = lightCountCB;
	}

private:
	// 定数バッファ構造体
	struct LightingCB
	{
		DirectX::XMMATRIX invProj;
		DirectX::XMMATRIX invView;
		DirectX::XMMATRIX view;
		DirectX::XMFLOAT3 cameraPos;
		float             _pad0;
		DirectX::XMFLOAT2 resolution;
		DirectX::XMFLOAT2 _pad1;
	};

	// 白テクスチャ（SSAO が未設定の場合のダミー）
	bool EnsureWhiteSRV(ID3D11Device* device);

	Microsoft::WRL::ComPtr<ID3D11Buffer>       m_lightingCB;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> m_pointClampSmp;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> m_shadowSmp;

	// 深度書き込みOFF用ステート（フルスクリーン描画では深度不要）
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_dsOff;

	// SSAO SRV（外部からセット、なければダミー白テクスチャを使用）
	ID3D11ShaderResourceView* m_ssaoSRV = nullptr;

	// ライトCB（Scene::UploadLightsToGPU()からセット）
	ID3D11Buffer* m_lightArrayCB = nullptr;
	ID3D11Buffer* m_lightCountCB = nullptr;

	// ダミー白テクスチャ（SSAO未設定時のフォールバック）
	Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_whiteTex;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_whiteSRV;

	bool m_initialized = false;
};