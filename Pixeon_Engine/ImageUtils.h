#pragma once
#include <d3d11.h>
#include <DirectXMath.h>

namespace ImageUtils {
	void DrawSRV(ID3D11ShaderResourceView* srv,
		float x, float y, float width, float height,
		const DirectX::XMFLOAT4& color = DirectX::XMFLOAT4(1, 1, 1, 1),
		const DirectX::XMFLOAT4& uvRect = DirectX::XMFLOAT4(0, 0, 1, 1),
		bool premultipliedAlpha = true,
		float opacity = 1.0f);
}