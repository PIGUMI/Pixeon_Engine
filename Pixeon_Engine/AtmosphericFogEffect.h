#pragma once
#include "LayerSettings.h"
#include <DirectXMath.h>

class AtmosphericFogEffect : public PostEffectBase
{
public:
	AtmosphericFogEffect();
	~AtmosphericFogEffect() override;

	void Apply(
		RHI::Texture* input,
		RHI::Texture* output,
		int width, int height) override;

	void DrawInspector() override;
	std::string    GetName()  const override { return "Atmospheric Fog"; }
	PostEffectType GetType()  const override { return PostEffectType::ATMOSPHERIC_FOG; }
	void SaveToJson(nlohmann::json& j)        const override;
	void LoadFromJson(const nlohmann::json& j)      override;
	PostEffectBase* Clone()   const override;

	void SetDepthTexture(RHI::Texture* depth) { m_depth = depth; }

	void SetCameraMatrices(
		const DirectX::XMMATRIX& proj,
		const DirectX::XMMATRIX& invProj,
		const DirectX::XMMATRIX& invView,
		const DirectX::XMFLOAT3& cameraPos)
	{
		m_proj = proj;
		m_invProj = invProj;
		m_invView = invView;
		m_cameraPos = cameraPos;
	}

	DirectX::XMFLOAT3 fogColor{ 0.7f, 0.8f, 0.9f };
	float fogDensity = 0.02f;
	float fogStart = 10.0f;
	float fogEnd = 200.0f;
	float fogHeight = 0.0f;
	float heightFalloff = 0.1f;

	enum class FogMode { Linear = 0, Exponential = 1, ExponentialSquared = 2 };
	FogMode fogMode = FogMode::Exponential;

private:
	bool EnsureResources();
	void ReleaseResources();

	RHI::Shader* m_vs = nullptr;   // ShaderManager が所有
	RHI::Shader* m_ps = nullptr;

	RHI::BufferRef  m_cb;
	RHI::SamplerRef m_pointClamp;

	RHI::Texture* m_depth = nullptr;
	DirectX::XMMATRIX m_proj = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX m_invProj = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX m_invView = DirectX::XMMatrixIdentity();
	DirectX::XMFLOAT3 m_cameraPos = { 0.f, 0.f, 0.f };

	bool m_resourcesReady = false;

	struct alignas(16) FogCB
	{
		DirectX::XMMATRIX invProj;
		DirectX::XMMATRIX invView;
		DirectX::XMFLOAT3 fogColor;
		float fogDensity;
		float fogStart;
		float fogEnd;
		float fogHeight;
		float heightFalloff;
		float resolutionX;
		float resolutionY;
		int fogMode;
		float _padFog;
		DirectX::XMFLOAT3 cameraPos;
		float _pad;
	};
	static_assert(sizeof(FogCB) % 16 == 0, "FogCB must be 16-byte aligned");
};
