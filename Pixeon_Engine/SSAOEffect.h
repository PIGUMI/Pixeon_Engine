#pragma once
#include "LayerSettings.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>

class SSAOEffect : public PostEffectBase
{
public:
	SSAOEffect();
	~SSAOEffect() = default;

	void Apply(
		ID3D11ShaderResourceView* input,
		ID3D11RenderTargetView* output,
		int width, int height) override;

	void DrawInspector() override;
	std::string    GetName()  const override { return "SSAO"; }
	PostEffectType GetType()  const override { return PostEffectType::SSAO; }
	void SaveToJson(nlohmann::json& j) const override;
	void LoadFromJson(const nlohmann::json& j) override;
	PostEffectBase* Clone() const override;

	void SetDepthSRV(ID3D11ShaderResourceView* depthSRV) { depthSRV_ = depthSRV; }
	void SetNormalSRV(ID3D11ShaderResourceView* normalSRV) { normalSRV_ = normalSRV; }
	void SetCameraMatrices(const DirectX::XMMATRIX& proj, const DirectX::XMMATRIX& invProj)
	{
		proj_ = proj;
		invProj_ = invProj;
	}

	void GenerateAO(int width, int height);

	ID3D11ShaderResourceView* GetAOSRV() const { return blurSRV_.Get(); }

	bool IsAOGenerated()    const { return aoGenerated_; }
	void ResetAOGenerated() { aoGenerated_ = false; }

	float radius = 0.5f;
	float bias = 0.025f;
	float power = 2.0f;
	float aoStrength = 0.8f;

private:
	struct SSAO_Params {
		DirectX::XMMATRIX proj;
		DirectX::XMMATRIX invProj;
		DirectX::XMFLOAT2 resolution;
		float radius;
		float bias;
		float power;
		DirectX::XMFLOAT3 _pad;
	};
	struct KernelCB {
		DirectX::XMFLOAT4 kernel[64];
	};
	struct BlurParams {
		DirectX::XMFLOAT2 texelSize;
		DirectX::XMFLOAT2 _pad;
	};
	struct CompositeParams {
		float aoStrength;
		DirectX::XMFLOAT3 _pad;
	};

	Microsoft::WRL::ComPtr<ID3D11Texture2D>          ssaoTex_;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   ssaoRTV_;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ssaoSRV_;
	Microsoft::WRL::ComPtr<ID3D11Texture2D>          blurTex_;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   blurRTV_;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> blurSRV_;

	Microsoft::WRL::ComPtr<ID3D11Texture2D>          noiseTex_;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noiseSRV_;

	Microsoft::WRL::ComPtr<ID3D11SamplerState>       pointClampSmp_;
	Microsoft::WRL::ComPtr<ID3D11SamplerState>       pointWrapSmp_;
	Microsoft::WRL::ComPtr<ID3D11SamplerState>       linearSmp_;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  dsOff_;

	Microsoft::WRL::ComPtr<ID3D11Buffer>             ssaoCB_;
	Microsoft::WRL::ComPtr<ID3D11Buffer>             kernelCB_;
	Microsoft::WRL::ComPtr<ID3D11Buffer>             blurCB_;
	Microsoft::WRL::ComPtr<ID3D11Buffer>             compositeCB_;

	std::vector<DirectX::XMFLOAT4> kernel_;

	ID3D11ShaderResourceView* depthSRV_ = nullptr;
	ID3D11ShaderResourceView* normalSRV_ = nullptr;
	bool aoGenerated_ = false;

	DirectX::XMMATRIX proj_ = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX invProj_ = DirectX::XMMatrixIdentity();

	int cachedWidth_ = 0;
	int cachedHeight_ = 0;

	bool EnsureRenderTargets(int width, int height);
	void EnsureKernel();
	void EnsureNoise();
	void EnsureSamplers();
	void EnsureDepthStencilState();
	void EnsureConstantBuffers();

	void DrawFullscreen(
		ID3D11PixelShader* ps,
		ID3D11ShaderResourceView* const* srvs,
		UINT                              srvCount,
		UINT                              srvSlot,
		ID3D11Buffer* const* psCBs,
		UINT                              cbCount,
		UINT                              cbSlot,
		ID3D11RenderTargetView* rtv,
		int width, int height
	);
};