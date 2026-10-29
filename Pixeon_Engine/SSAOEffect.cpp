// SSAOEffect.cpp
#include "SSAOEffect.h"
#include "System.h"
#include "ShaderManager.h"
#include "GUI.h"
#include "IMGUI/imgui.h"
#include "ImageUtils.h"
#include <random>

using namespace DirectX;

SSAOEffect::SSAOEffect()
{
	EnsureKernel();
	EnsureNoise();
}

PostEffectBase* SSAOEffect::Clone() const
{
	SSAOEffect* c = new SSAOEffect();
	c->radius = radius;
	c->bias = bias;
	c->power = power;
	c->aoStrength = aoStrength;
	c->enabled = enabled;
	c->priority = priority;
	return c;
}

/*
* 関数名　PassThrough
* 引　数　input / output：入出力　width / height：解像度
* 戻り値　なし
* 説　明　入力をそのまま出力へ描く
*/
void SSAOEffect::PassThrough(RHI::Texture* input, RHI::Texture* output, int width, int height)
{
	if (!input || !output) return;
	ScopedOutput scope(output, width, height);
	ImageUtils::DrawSRV(input, 0.0f, 0.0f, (float)width, (float)height,
		XMFLOAT4(1, 1, 1, 1), XMFLOAT4(0, 0, 1, 1), true, 1.0f);
}

/*
* 関数名　FullscreenDraw
* 引　数　ps：ピクセルシェーダー　target：描画先　width / height：解像度
* 戻り値　なし
* 説　明　VS_Fullscreen の三角形 1 枚で全画面を描く（VS・トポロジ等は呼び出し側で設定済み）
*/
void SSAOEffect::FullscreenDraw(RHI::Shader* ps, RHI::Texture* target, int width, int height)
{
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	dev->ShaderSet(RHI::ShaderStage::Pixel, ps);
	dev->RenderTargetsSet(1, &target, nullptr);
	RHI::Viewport vp;
	vp.f_Width = (float)width;
	vp.f_Height = (float)height;
	dev->ViewportSet(vp);
	dev->Draw(3, 0);
}

/*
* 関数名　AOPassRun
* 引　数　width / height：解像度　normal：GBuffer 法線（無ければ深度から復元）
* 戻り値　成功なら true（結果は blurTex_）
* 説　明　SSAO の計算とブラーを行う。描画先・ビューポート・深度ステートは元に戻す
*/
bool SSAOEffect::AOPassRun(int width, int height, RHI::Texture* normal)
{
	GraphicsSystem* gs = GraphicsSystem::GetInstance();
	RHI::Device* dev = gs->GetDevice();
	auto* sm = ShaderManager::GetInstance();
	RHI::Shader* fullscreenVS = sm->GetVertexShader("VS_Fullscreen");
	RHI::Shader* ssaoPS = sm->GetPixelShader("PS_SSAO");
	RHI::Shader* blurPS = sm->GetPixelShader("PS_SSAOBlur");
	if (normal) {
		if (RHI::Shader* deferredPS = sm->GetPixelShader("PS_SSAO_Deferred")) ssaoPS = deferredPS;
	}
	if (!depth_ || !fullscreenVS || !ssaoPS || !blurPS || !EnsureRenderTargets(width, height)) return false;
	EnsureSamplers();
	EnsureConstantBuffers();

	const RHI::RenderTargetState oldTargets = dev->RenderTargetsGet();
	const RHI::Viewport oldVP = dev->ViewportGet();
	RHI::DepthStencilState* oldDepth = dev->DepthStencilStateGet();

	gs->SetDepthMode(DEPTH_OFF);
	dev->ShaderSet(RHI::ShaderStage::Vertex, fullscreenVS);
	dev->InputLayoutSet(nullptr);
	dev->TopologySet(RHI::Topology::TriangleList);

	// ---- SSAO ----
	SSAO_Params cb{};
	cb.proj = XMMatrixTranspose(proj_);
	cb.invProj = XMMatrixTranspose(invProj_);
	cb.resolution = { (float)width, (float)height };
	cb.radius = radius;
	cb.bias = bias;
	cb.power = power;
	dev->BufferUpdate(ssaoCB_.get(), &cb, sizeof(cb));
	KernelCB kcb{};
	for (int k = 0; k < 64; ++k) kcb.kernel[k] = kernel_[k];
	dev->BufferUpdate(kernelCB_.get(), &kcb, sizeof(kcb));
	dev->ConstantBufferSet(RHI::ShaderStage::Pixel, 1, ssaoCB_.get());
	dev->ConstantBufferSet(RHI::ShaderStage::Pixel, 2, kernelCB_.get());
	dev->TextureSet(RHI::ShaderStage::Pixel, 0, normal);
	dev->TextureSet(RHI::ShaderStage::Pixel, 1, depth_);
	dev->TextureSet(RHI::ShaderStage::Pixel, 2, noiseTex_.get());
	dev->SamplerSet(RHI::ShaderStage::Pixel, 0, pointClampSmp_.get());
	dev->SamplerSet(RHI::ShaderStage::Pixel, 1, pointWrapSmp_.get());
	FullscreenDraw(ssaoPS, ssaoTex_.get(), width, height);
	for (uint32_t i = 0; i < 3; ++i) dev->TextureSet(RHI::ShaderStage::Pixel, i, nullptr);

	// ---- ブラー ----
	BlurParams bcb{};
	bcb.texelSize = { 1.0f / width, 1.0f / height };
	dev->BufferUpdate(blurCB_.get(), &bcb, sizeof(bcb));
	dev->ConstantBufferSet(RHI::ShaderStage::Pixel, 1, blurCB_.get());
	dev->TextureSet(RHI::ShaderStage::Pixel, 0, ssaoTex_.get());
	dev->SamplerSet(RHI::ShaderStage::Pixel, 0, pointClampSmp_.get());
	FullscreenDraw(blurPS, blurTex_.get(), width, height);
	dev->TextureSet(RHI::ShaderStage::Pixel, 0, nullptr);

	dev->DepthStencilStateSet(oldDepth);
	dev->RenderTargetsSet(oldTargets.i_Count, oldTargets.p_Colors, oldTargets.p_Depth);
	dev->ViewportSet(oldVP);
	return true;
}

void SSAOEffect::Apply(
	RHI::Texture* input,
	RHI::Texture* output,
	int width, int height)
{
	if (aoGenerated_) {
		// Deferred パスで生成済み（ライティングで使われる）なら素通し
		aoGenerated_ = false; depth_ = nullptr; normal_ = nullptr;
		PassThrough(input, output, width, height);
		return;
	}
	RHI::Shader* compositePS = ShaderManager::GetInstance()->GetPixelShader("PS_SSAOComposite");
	if (!input || !output || !compositePS || !AOPassRun(width, height, nullptr)) {
		PassThrough(input, output, width, height);
		depth_ = nullptr;
		return;
	}

	// ---- 合成 ----
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	RHI::DepthStencilState* oldDepth = dev->DepthStencilStateGet();
	GraphicsSystem::GetInstance()->SetDepthMode(DEPTH_OFF);
	{
		ScopedOutput scope(output, width, height);
		CompositeParams ccb{};
		ccb.aoStrength = aoStrength;
		dev->BufferUpdate(compositeCB_.get(), &ccb, sizeof(ccb));
		dev->ConstantBufferSet(RHI::ShaderStage::Pixel, 1, compositeCB_.get());
		dev->TextureSet(RHI::ShaderStage::Pixel, 0, input);
		dev->TextureSet(RHI::ShaderStage::Pixel, 1, blurTex_.get());
		dev->SamplerSet(RHI::ShaderStage::Pixel, 0, linearSmp_.get());
		dev->ShaderSet(RHI::ShaderStage::Vertex, ShaderManager::GetInstance()->GetVertexShader("VS_Fullscreen"));
		dev->ShaderSet(RHI::ShaderStage::Pixel, compositePS);
		dev->InputLayoutSet(nullptr);
		dev->TopologySet(RHI::Topology::TriangleList);
		dev->Draw(3, 0);
		dev->TextureSet(RHI::ShaderStage::Pixel, 1, nullptr);
	}
	dev->DepthStencilStateSet(oldDepth);
	depth_ = nullptr;
}

/*
* 関数名　GenerateAO
* 引　数　width / height：解像度
* 戻り値　なし
* 説　明　Deferred 用に GBuffer の法線と深度から AO を生成する（LightingPass が参照する）
*/
void SSAOEffect::GenerateAO(int width, int height)
{
	if (!depth_) return;
	const bool ok = AOPassRun(width, height, normal_);
	depth_ = nullptr;
	normal_ = nullptr;
	aoGenerated_ = ok;
}

bool SSAOEffect::EnsureRenderTargets(int width, int height)
{
	if (cachedWidth_ == width && cachedHeight_ == height && ssaoTex_ && blurTex_) return true;
	cachedWidth_ = width;
	cachedHeight_ = height;
	RHI::TextureDesc td;
	td.i_Width = (uint32_t)width;
	td.i_Height = (uint32_t)height;
	td.e_Format = RHI::Format::R8_UNorm;
	td.i_BindFlags = RHI::Bind_RenderTarget | RHI::Bind_ShaderResource;
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	td.c_DebugName = "SSAO.AO";
	ssaoTex_ = dev->TextureCreate(td, nullptr);
	td.c_DebugName = "SSAO.Blur";
	blurTex_ = dev->TextureCreate(td, nullptr);
	return ssaoTex_ && blurTex_;
}

void SSAOEffect::EnsureKernel()
{
	if (!kernel_.empty()) return;

	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	std::default_random_engine rng(42);

	kernel_.resize(64);
	for (int i = 0; i < 64; ++i)
	{
		XMFLOAT3 s(dist(rng) * 2.0f - 1.0f,
			dist(rng) * 2.0f - 1.0f,
			dist(rng));
		XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&s));
		v = XMVectorScale(v, dist(rng));

		float scale = (float)i / 64.0f;
		scale = 0.1f + scale * scale * 0.9f;
		v = XMVectorScale(v, scale);

		XMFLOAT3 r;
		XMStoreFloat3(&r, v);
		kernel_[i] = { r.x, r.y, r.z, 0.0f };
	}
}

void SSAOEffect::EnsureNoise()
{
	if (noiseTex_) return;

	std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
	std::default_random_engine rng(123);

	uint8_t data[4 * 4 * 4];
	for (int i = 0; i < 16; ++i)
	{
		float x = dist(rng), y = dist(rng);
		float len = sqrtf(x * x + y * y);
		if (len > 0.0001f) { x /= len; y /= len; }
		data[i * 4 + 0] = (uint8_t)((x * 0.5f + 0.5f) * 255.0f);
		data[i * 4 + 1] = (uint8_t)((y * 0.5f + 0.5f) * 255.0f);
		data[i * 4 + 2] = 0;
		data[i * 4 + 3] = 255;
	}

	RHI::TextureDesc td;
	td.i_Width = 4;
	td.i_Height = 4;
	td.e_Format = RHI::Format::R8G8B8A8_UNorm;
	td.e_Usage = RHI::Usage::Immutable;
	const RHI::SubresourceData init{ data, 4 * 4, 4 * 4 * 4 };
	noiseTex_ = GraphicsSystem::GetInstance()->GetDevice()->TextureCreate(td, &init);
}

void SSAOEffect::EnsureSamplers()
{
	if (pointClampSmp_) return;
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	RHI::SamplerDesc sd;
	sd.e_Filter = RHI::Filter::Point;
	sd.e_Compare = RHI::CompareFunc::Always;
	sd.e_AddressU = sd.e_AddressV = sd.e_AddressW = RHI::AddressMode::Clamp;
	pointClampSmp_ = dev->SamplerCreate(sd);
	sd.e_AddressU = sd.e_AddressV = sd.e_AddressW = RHI::AddressMode::Wrap;
	pointWrapSmp_ = dev->SamplerCreate(sd);
	sd.e_Filter = RHI::Filter::Linear;
	sd.e_AddressU = sd.e_AddressV = sd.e_AddressW = RHI::AddressMode::Clamp;
	linearSmp_ = dev->SamplerCreate(sd);
}

void SSAOEffect::EnsureConstantBuffers()
{
	if (ssaoCB_) return;
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	RHI::BufferDesc bd;
	bd.i_BindFlags = RHI::Bind_ConstantBuffer;
	bd.i_Size = sizeof(SSAO_Params);     ssaoCB_ = dev->BufferCreate(bd);
	bd.i_Size = sizeof(KernelCB);        kernelCB_ = dev->BufferCreate(bd);
	bd.i_Size = sizeof(BlurParams);      blurCB_ = dev->BufferCreate(bd);
	bd.i_Size = sizeof(CompositeParams); compositeCB_ = dev->BufferCreate(bd);
}

void SSAOEffect::DrawInspector()
{
	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("半径").c_str(), &radius, 0.01f, 0.05f, 2.0f, "%.3f");
	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("バイアス").c_str(), &bias, 0.001f, 0.001f, 0.1f, "%.4f");
	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("強度").c_str(), &power, 0.05f, 0.5f, 5.0f, "%.2f");
	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("適用強度").c_str(), &aoStrength, 0.01f, 0.0f, 1.0f, "%.2f");
}

void SSAOEffect::SaveToJson(nlohmann::json& j) const
{
	j["radius"] = radius;
	j["bias"] = bias;
	j["power"] = power;
	j["aoStrength"] = aoStrength;
}

void SSAOEffect::LoadFromJson(const nlohmann::json& j)
{
	radius = j.value("radius", 0.5f);
	bias = j.value("bias", 0.025f);
	power = j.value("power", 2.0f);
	aoStrength = j.value("aoStrength", 0.8f);
}
