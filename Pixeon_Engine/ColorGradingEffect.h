// ColorGradingEffect.h
#pragma once
#include "LayerSettings.h"

class ColorGradingEffect : public PostEffectBase {
public:
	// 明度調整 (-1.0 ~ 1.0)
	float brightness = 0.0f;

	// コントラスト調整 (0.0 ~ 2.0)
	float contrast = 1.0f;

	// 彩度調整 (0.0 ~ 2.0)
	float saturation = 1.0f;

	// 色相シフト (0.0 ~ 360.0 度)
	float hueShift = 0.0f;

	// 色温度 (-1.0 ~ 1.0)
	float temperature = 0.0f;

	// 色合い (-1.0 ~ 1.0)
	float tint = 0.0f;

	// ガンマ補正 (0.1 ~ 3.0)
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