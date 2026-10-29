#pragma once
#include "LayerSettings.h"
#include <DirectXMath.h>
#include <vector>

class SSAOEffect : public PostEffectBase
{
public:
	SSAOEffect();
	~SSAOEffect() = default;

	void Apply(
		RHI::Texture* input,
		RHI::Texture* output,
		int width, int height) override;

	void DrawInspector() override;
	std::string    GetName()  const override { return "SSAO"; }
	PostEffectType GetType()  const override { return PostEffectType::SSAO; }
	void SaveToJson(nlohmann::json& j) const override;
	void LoadFromJson(const nlohmann::json& j) override;
	PostEffectBase* Clone() const override;

	void SetDepthTexture(RHI::Texture* depth) { depth_ = depth; }
	void SetNormalTexture(RHI::Texture* normal) { normal_ = normal; }
	void SetCameraMatrices(const DirectX::XMMATRIX& proj, const DirectX::XMMATRIX& invProj)
	{
		proj_ = proj;
		invProj_ = invProj;
	}

	void GenerateAO(int width, int height);

	RHI::Texture* GetAOTexture() const { return blurTex_.get(); }

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

	RHI::TextureRef ssaoTex_;
	RHI::TextureRef blurTex_;
	RHI::TextureRef noiseTex_;

	RHI::SamplerRef pointClampSmp_;
	RHI::SamplerRef pointWrapSmp_;
	RHI::SamplerRef linearSmp_;

	RHI::BufferRef ssaoCB_;
	RHI::BufferRef kernelCB_;
	RHI::BufferRef blurCB_;
	RHI::BufferRef compositeCB_;

	std::vector<DirectX::XMFLOAT4> kernel_;

	RHI::Texture* depth_ = nullptr;
	RHI::Texture* normal_ = nullptr;
	bool aoGenerated_ = false;

	DirectX::XMMATRIX proj_ = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX invProj_ = DirectX::XMMatrixIdentity();

	int cachedWidth_ = 0;
	int cachedHeight_ = 0;

	bool EnsureRenderTargets(int width, int height);
	void EnsureKernel();
	void EnsureNoise();
	void EnsureSamplers();
	void EnsureConstantBuffers();

	// AO 生成（SSAO → ブラー）。normal があれば Deferred 版のシェーダーを使う
	bool AOPassRun(int width, int height, RHI::Texture* normal);
	// 入力をそのまま出力へ描く（シェーダーや深度が無い場合のフォールバック）
	void PassThrough(RHI::Texture* input, RHI::Texture* output, int width, int height);
	void FullscreenDraw(RHI::Shader* ps, RHI::Texture* target, int width, int height);
};
