#pragma once
#include "RHI/RHI.h"

// ============================================================
// GBuffer
//
// Deferred Rendering 用の複数レンダーターゲット管理クラス
//
// 構成:
//   RT0: Albedo        RGBA8_UNORM      (RGB=テクスチャ色, A=未使用)
//   RT1: Normal        RGBA16F          (RGB=ビュー空間法線, A=未使用)
//   Depth: D24S8                        (深度SRVとして読み取り可能)
//
// 深度バッファは Geometry Pass で書き込み、
// SSAO Pass / Lighting Pass で SRV として読み取る
// ============================================================
class GBuffer
{
public:
	// 初期化・解放
	bool Init(int width, int height);
	void Release();

	// Geometry Pass の開始
	// 複数RTと深度バッファをセットし、クリアする
	void BeginGeometryPass();

	// Geometry Pass の終了
	// RTを外す（End後にSRVとして読める）
	void EndGeometryPass();

	// サイズ変更（解像度変更時）
	bool Resize(int width, int height);

	// ---- アクセサ（Lighting / SSAO Pass で使用）----
	RHI::Texture* GetAlbedo() const { return m_cls_Albedo.get(); }
	RHI::Texture* GetNormal() const { return m_cls_Normal.get(); }
	// 深度（Geometry Pass 後に SRV として読む / Transparent 描画で DSV として再利用）
	RHI::Texture* GetDepth()  const { return m_cls_Depth.get(); }

	int GetWidth()  const { return m_i_Width; }
	int GetHeight() const { return m_i_Height; }

private:
	RHI::TextureRef m_cls_Albedo;
	RHI::TextureRef m_cls_Normal;
	RHI::TextureRef m_cls_Depth;
	int m_i_Width = 0;
	int m_i_Height = 0;

	// 内部生成ヘルパー
	bool CreateRenderTargets(int width, int height);
};
