#pragma once
#include "LayerSettings.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

class AtmosphericFogEffect : public PostEffectBase
{
public:
	AtmosphericFogEffect();
	~AtmosphericFogEffect() override;

	void Apply(
		ID3D11ShaderResourceView* input,
		ID3D11RenderTargetView* output,
		int width, int height) override;

	void DrawInspector() override;
	std::string    GetName()  const override { return "Atmospheric Fog"; }
	PostEffectType GetType()  const override { return PostEffectType::ATMOSPHERIC_FOG; }
	void SaveToJson(nlohmann::json& j)        const override;
	void LoadFromJson(const nlohmann::json& j)      override;
	PostEffectBase* Clone()   const override;

	void SetDepthSRV(ID3D11ShaderResourceView* srv) { m_depthSRV = srv; }

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

	ID3D11VertexShader* m_vs = nullptr;
	ID3D11PixelShader* m_ps = nullptr;

	Microsoft::WRL::ComPtr<ID3D11Buffer>            m_cb;
	Microsoft::WRL::ComPtr<ID3D11SamplerState>      m_pointClamp;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_dsOff;

	ID3D11ShaderResourceView* m_depthSRV = nullptr;
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