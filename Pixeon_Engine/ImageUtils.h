#pragma once
#include <DirectXMath.h>
#include "RHI/RHI.h"

namespace ImageUtils {
	void DrawSRV(RHI::Texture* texture,
		float x, float y, float width, float height,
		const DirectX::XMFLOAT4& color = DirectX::XMFLOAT4(1, 1, 1, 1),
		const DirectX::XMFLOAT4& uvRect = DirectX::XMFLOAT4(0, 0, 1, 1),
		bool premultipliedAlpha = true,
		float opacity = 1.0f);

	void DrawSRVPixelated(RHI::Texture* texture,
		float x, float y, float width, float height,
		float pixelSize = 4.0f,
		float intensity = 1.0f,
		const DirectX::XMFLOAT4& color = DirectX::XMFLOAT4(1, 1, 1, 1),
		const DirectX::XMFLOAT4& uvRect = DirectX::XMFLOAT4(0, 0, 1, 1));

	void DrawSRVColorGrading(RHI::Texture* texture,
		float x, float y, float width, float height,
		float brightness = 0.0f,
		float contrast = 1.0f,
		float saturation = 1.0f,
		float hueShift = 0.0f,
		float temperature = 0.0f,
		float tint = 0.0f,
		float gamma = 1.0f,
		const DirectX::XMFLOAT4& uvRect = DirectX::XMFLOAT4(0, 0, 1, 1));

	// 任意のシェーダーで矩形を描く際の設定
	// （VS は POSITION/TEXCOORD 入力と b0 の CBVS {View, Proj, Color, mode2D} を持つこと）
	struct QuadDesc {
		const char* vsName = "VS_ImgQuad";
		const char* psName = "PS_ImgQuad";
		RHI::Texture* texture = nullptr;
		float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
		DirectX::XMFLOAT4 color = DirectX::XMFLOAT4(1, 1, 1, 1);
		DirectX::XMFLOAT4 uvRect = DirectX::XMFLOAT4(0, 0, 1, 1);
		const void* psConstants = nullptr;   // ピクセルシェーダーの定数（任意）
		uint32_t psConstantsSize = 0;
		uint32_t psConstantsSlot = 0;
		int blendMode = -1;                  // BlendMode。-1 なら現在のブレンドステートをそのまま使う
		float screenWidth = 0.0f;            // NDC 変換の基準サイズ（0 ならバックバッファ）
		float screenHeight = 0.0f;
	};
	void DrawQuad(const QuadDesc& desc);

	// 解放（GraphicsSystem の破棄前に呼ぶ）
	void Release();
}
