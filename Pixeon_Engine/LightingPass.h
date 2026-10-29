#pragma once
#include <DirectXMath.h>
#include "RHI/RHI.h"

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
//                  （出力先のレンダーターゲットは呼び出し側でセットしておく）
// ============================================================
class LightingPass
{
public:
	bool Init();
	void Release();

	// Lighting Pass を実行して現在のレンダーターゲットに書き込む
	// gbuffer    : Geometry Pass で書き込んだ GBuffer
	// shadowMap  : シャドウマップ（深度）
	// width/height: レンダリング解像度
	// proj       : カメラのプロジェクション行列
	// view       : カメラのビュー行列
	// cameraPos  : カメラのワールド座標
	void Execute(
		GBuffer* gbuffer,
		RHI::Texture* shadowMap,
		int width, int height,
		const DirectX::XMMATRIX& proj,
		const DirectX::XMMATRIX& view,
		const DirectX::XMFLOAT3& cameraPos
	);

	// SSAO テクスチャをセット
	void SetSSAOTexture(RHI::Texture* texture) { m_cls_SSAO = texture; }
	void SetAOStrength(float s) { m_f_AOStrength = s; }

	// ライト定数バッファをセット（Scene::UploadLightsToGPU()の後に呼ぶ）
	void SetLightBuffers(RHI::Buffer* lightArrayCB, RHI::Buffer* lightCountCB)
	{
		m_cls_LightArrayCB = lightArrayCB;
		m_cls_LightCountCB = lightCountCB;
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
		float             aoStrength;
		float             _pad1;
	};

	RHI::BufferRef  m_cls_LightingCB;
	RHI::SamplerRef m_cls_PointClampSmp;
	RHI::SamplerRef m_cls_ShadowSmp;

	// SSAO（外部からセット、なければ白テクスチャを使用）
	RHI::Texture* m_cls_SSAO = nullptr;
	float m_f_AOStrength = 1.0f;

	// ライトCB（Scene::UploadLightsToGPU()からセット）
	RHI::Buffer* m_cls_LightArrayCB = nullptr;
	RHI::Buffer* m_cls_LightCountCB = nullptr;

	bool m_b_Initialized = false;
};
