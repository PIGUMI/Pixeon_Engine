#pragma once
#include "LayerSettings.h"
#include "ShaderManager.h"

class BloomEffect : public PostEffectBase {
public:
	float threshold = 0.8f;
	float intensity = 0.5f;
	float blurSize = 2.0f;
	DirectX::XMFLOAT4 tint = DirectX::XMFLOAT4(1, 1, 1, 1);

	void Apply(RHI::Texture* input,
		RHI::Texture* output,
		int width, int height) override;

	void DrawInspector() override;
	std::string GetName() const override { return "Bloom"; }
	PostEffectType GetType() const override { return PostEffectType::BLOOM; }

	void SaveToJson(nlohmann::json& j) const override;
	void LoadFromJson(const nlohmann::json& j) override;
	PostEffectBase* Clone() const override;

private:
	RHI::TextureRef tempTexture_;
	int bufferWidth_ = 0;
	int bufferHeight_ = 0;

	void CreateTempBuffer(int width, int height);
	void ReleaseTempBuffer();
};
