// ColorGradingEffect.h
#pragma once
#include "LayerSettings.h"

class ColorGradingEffect : public PostEffectBase {
public:
	float brightness = 0.0f;

	float contrast = 1.0f;

	float saturation = 1.0f;

	float hueShift = 0.0f;

	float temperature = 0.0f;

	float tint = 0.0f;

	float gamma = 1.0f;

	void Apply(ID3D11ShaderResourceView* input,
		ID3D11RenderTargetView* output,
		int width, int height) override;

	void DrawInspector() override;
	std::string GetName() const override { return "Color Grading"; }
	PostEffectType GetType() const override { return PostEffectType::COLOR_GRADING; }

	void SaveToJson(nlohmann::json& j) const override;
	void LoadFromJson(const nlohmann::json& j) override;
	PostEffectBase* Clone() const override;
};