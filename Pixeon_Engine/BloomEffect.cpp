// BloomEffect.cpp
#include "BloomEffect.h"
#include "ImageUtils.h"
#include "System.h"
#include "GUI.h"
#include "ShaderManager.h"

namespace {
	struct BloomParams {
		float threshold;
		float intensity;
		float blurRadius;
		float screenWidth;
		float screenHeight;
		int passType;
		float pad[2];
	};
}

/*
* 関数名　Apply
* 引　数　input：入力　output：出力先　width / height：解像度
* 戻り値　なし
* 説　明　pass1: 高輝度部分を一時バッファへ抽出 → pass2: 元画像の上にブラーを加算合成
*/
void BloomEffect::Apply(RHI::Texture* input,
	RHI::Texture* output,
	int width, int height) {
	if (!input || !output) return;
	GraphicsSystem* gs = GraphicsSystem::GetInstance();
	RHI::Device* dev = gs->GetDevice();
	ShaderManager* sm = ShaderManager::GetInstance();

	if (!sm->GetVertexShader("VS_Bloom") || !sm->GetPixelShader("PS_Bloom")) {
		ScopedOutput scope(output, width, height);
		ImageUtils::DrawSRV(input, 0.0f, 0.0f, (float)width, (float)height);
		return;
	}

	CreateTempBuffer(width, height);
	if (!tempTexture_) return;

	RHI::BlendState* oldBlend = dev->BlendStateGet();
	RHI::DepthStencilState* oldDepth = dev->DepthStencilStateGet();

	BloomParams params{};
	params.threshold = threshold;
	params.intensity = intensity;
	params.blurRadius = blurSize;
	params.screenWidth = (float)width;
	params.screenHeight = (float)height;

	ImageUtils::QuadDesc quad;
	quad.vsName = "VS_Bloom";
	quad.psName = "PS_Bloom";
	quad.width = (float)width;
	quad.height = (float)height;
	quad.screenWidth = (float)width;
	quad.screenHeight = (float)height;
	quad.psConstants = &params;
	quad.psConstantsSize = sizeof(params);
	quad.psConstantsSlot = 1;

	// pass1: 高輝度抽出（ブレンドは呼び出し元の設定を引き継ぐ）
	{
		ScopedOutput scope(tempTexture_.get(), width, height);
		const float clearColor[4] = { 0, 0, 0, 0 };
		dev->RenderTargetClear(tempTexture_.get(), clearColor);
		params.passType = 1;
		quad.texture = input;
		ImageUtils::DrawQuad(quad);
	}

	// pass2: 元画像 + ブラーの加算合成
	{
		ScopedOutput scope(output, width, height);
		ImageUtils::DrawSRV(input, 0.0f, 0.0f, (float)width, (float)height,
			DirectX::XMFLOAT4(1, 1, 1, 1), DirectX::XMFLOAT4(0, 0, 1, 1), true, 1.0f);
		params.passType = 2;
		quad.texture = tempTexture_.get();
		quad.color = tint;
		quad.color.w *= intensity;
		quad.blendMode = BLEND_ADD_ALL;
		ImageUtils::DrawQuad(quad);
	}

	dev->DepthStencilStateSet(oldDepth);
	dev->BlendStateSet(oldBlend);
}

void BloomEffect::DrawInspector() {
	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("輝度閾値").c_str(),
		&threshold, 0.01f, 0.0f, 2.0f);
	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("強度").c_str(),
		&intensity, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("ブラーサイズ").c_str(),
		&blurSize, 0.1f, 0.5f, 10.0f);
	ImGui::ColorEdit4(GUI::GetInstance()->ShiftJISToUTF8("色調整").c_str(), &tint.x);
}

void BloomEffect::SaveToJson(nlohmann::json& j) const {
	j["threshold"] = threshold;
	j["intensity"] = intensity;
	j["blurSize"] = blurSize;
	j["tint"] = { tint.x, tint.y, tint.z, tint.w };
}

void BloomEffect::LoadFromJson(const nlohmann::json& j) {
	threshold = j.value("threshold", 0.8f);
	intensity = j.value("intensity", 0.5f);
	blurSize = j.value("blurSize", 2.0f);

	if (j.contains("tint") && j["tint"].is_array() && j["tint"].size() == 4) {
		tint.x = j["tint"][0];
		tint.y = j["tint"][1];
		tint.z = j["tint"][2];
		tint.w = j["tint"][3];
	}
}

PostEffectBase* BloomEffect::Clone() const {
	BloomEffect* clone = new BloomEffect();
	clone->threshold = threshold;
	clone->intensity = intensity;
	clone->blurSize = blurSize;
	clone->tint = tint;
	clone->enabled = enabled;
	clone->priority = priority;
	return clone;
}

void BloomEffect::CreateTempBuffer(int width, int height) {
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	if (!dev) return;
	if (tempTexture_ && bufferWidth_ == width && bufferHeight_ == height) {
		return;
	}

	ReleaseTempBuffer();
	bufferWidth_ = width;
	bufferHeight_ = height;

	RHI::TextureDesc td;
	td.i_Width = (uint32_t)width;
	td.i_Height = (uint32_t)height;
	td.e_Format = RHI::Format::R16G16B16A16_Float;
	td.i_BindFlags = RHI::Bind_RenderTarget | RHI::Bind_ShaderResource;
	td.c_DebugName = "Bloom.Temp";
	tempTexture_ = dev->TextureCreate(td, nullptr);
}

void BloomEffect::ReleaseTempBuffer() {
	tempTexture_.reset();
	bufferWidth_ = 0;
	bufferHeight_ = 0;
}
