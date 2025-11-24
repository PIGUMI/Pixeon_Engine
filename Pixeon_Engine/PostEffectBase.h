#pragma once
#include <d3d11.h>
#include <string>

// ポストエフェクトの抽象基底クラス
class PostEffectBase
{
public:
	virtual ~PostEffectBase() {}

	// 初期化（リソース確保など）
	virtual void Init(ID3D11Device* device, int width, int height) = 0;
	// 解放
	virtual void UnInit() = 0;

	// 適用（SRVからRTVへエフェクトをかける）
	virtual void Apply(
		ID3D11DeviceContext* context,
		ID3D11ShaderResourceView* inputSRV,
		ID3D11RenderTargetView* outputRTV,
		int viewX, int viewY, int viewWidth, int viewHeight
	) = 0;

	// デバッグUI（ImGuiなど）
	virtual void DrawDebugUI() {}

	// エフェクト名（識別用）
	virtual std::string GetEffectName() const = 0;
};