// PixelateEffect.h
#pragma once
#include "LayerSettings.h"

class PixelateEffect : public PostEffectBase {
public:
	float pixelSize = 8.0f;
	float intensity = 1.0f;
	DirectX::XMFLOAT4 color = DirectX::XMFLOAT4(1, 1, 1, 1);

	void Apply(ID3D11ShaderResourceView* input,
		ID3D11RenderTargetView* output,
		int width, int height) override;

	void DrawInspector() override;
	std::string GetName() const override { return "Pixelate"; }
	PostEffectType GetType() const override { return PostEffectType::PIXELATE; }

	void SaveToJson(nlohmann::json& j) const override;
	void LoadFromJson(const nlohmann::json& j) override;
	PostEffectBase* Clone() const override;
};