// Layer.cpp
#include "LayerSettings.h"
#include "System.h"
#include "ImageUtils.h"
#include <algorithm>

#include "PixelateEffect.h"
#include "BloomEffect.h"
#include "ColorGradingEffect.h"
#include "SSAOEffect.h"
#include "AtmosphericFogEffect.h"   // ★追加

Layer::Layer() {
}

Layer::~Layer() {
	ReleaseTempBuffers();
}

void Layer::AddPostEffect(PostEffectType type) {
	auto effect = CreatePostEffect(type);
	if (effect) {
		postEffects.push_back(effect);

		std::sort(postEffects.begin(), postEffects.end(),
			[](const std::shared_ptr<PostEffectBase>& a,
				const std::shared_ptr<PostEffectBase>& b) {
					return a->priority < b->priority;
			});
	}
}

PostEffectBase::ScopedOutput::ScopedOutput(RHI::Texture* output, int width, int height)
{
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	m_str_Saved = dev->RenderTargetsGet();
	m_str_SavedViewport = dev->ViewportGet();
	dev->RenderTargetsSet(1, &output, nullptr);
	RHI::Viewport vp;
	vp.f_Width = (float)width;
	vp.f_Height = (float)height;
	dev->ViewportSet(vp);
}

PostEffectBase::ScopedOutput::~ScopedOutput()
{
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	dev->TextureSet(RHI::ShaderStage::Pixel, 0, nullptr);
	dev->RenderTargetsSet(m_str_Saved.i_Count, m_str_Saved.p_Colors, m_str_Saved.p_Depth);
	dev->ViewportSet(m_str_SavedViewport);
}

void Layer::ApplyPostEffectsToScreen(
	RHI::Texture* input,
	int width, int height,
	float opacity,
	RHI::Texture* depth,
	const DirectX::XMMATRIX* proj,
	const DirectX::XMMATRIX* invProj,
	const DirectX::XMMATRIX* invView,
	const DirectX::XMFLOAT3* cameraPos
) {
	if (!input) return;
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	if (!dev) return;

	// 有効なエフェクトを抽出
	std::vector<std::shared_ptr<PostEffectBase>> activeEffects;
	for (auto& effect : postEffects) {
		if (effect && effect->enabled) {
			activeEffects.push_back(effect);
		}
	}

	// 中間バッファを準備
	if (!activeEffects.empty()) CreateTempBuffers(width, height);
	if (activeEffects.empty() || !tempTexture1_ || !tempTexture2_) {
		// エフェクトなし（またはバッファ作成失敗）: 通常描画
		ImageUtils::DrawSRV(input, 0, 0, (float)width, (float)height,
			DirectX::XMFLOAT4(1, 1, 1, 1),
			DirectX::XMFLOAT4(0, 0, 1, 1),
			true, opacity);
		return;
	}

	// 現在のレンダーターゲット（最終出力先）を保存
	const RHI::RenderTargetState saved = dev->RenderTargetsGet();
	RHI::Texture* screen = saved.i_Count > 0 ? saved.p_Colors[0] : nullptr;

	// 入力ソース
	RHI::Texture* currentInput = input;

	// エフェクトを順次適用
	for (size_t i = 0; i < activeEffects.size(); i++) {
		const bool isLastEffect = (i == activeEffects.size() - 1);

		// ---- SSAO: Deferred実行済みならスキップ ----
		if (auto* ssao = dynamic_cast<SSAOEffect*>(activeEffects[i].get()))
		{
			if (ssao->IsAOGenerated())
			{
				ssao->ResetAOGenerated();
				if (isLastEffect && currentInput && screen)
				{
					GraphicsSystem::GetInstance()->RenderTargetsSet(1, &screen, nullptr);
					ImageUtils::DrawSRV(currentInput, 0.0f, 0.0f, (float)width, (float)height,
						DirectX::XMFLOAT4(1, 1, 1, 1), DirectX::XMFLOAT4(0, 0, 1, 1), true, 1.0f);
				}
				continue;
			}
			if (depth)               ssao->SetDepthTexture(depth);
			if (proj && invProj)     ssao->SetCameraMatrices(*proj, *invProj);
		}

		if (auto* fog = dynamic_cast<AtmosphericFogEffect*>(activeEffects[i].get()))
		{
			if (depth) fog->SetDepthTexture(depth);
			if (proj && invProj && invView && cameraPos)
			{
				fog->SetCameraMatrices(*proj, *invProj, *invView, *cameraPos);
			}
		}

		if (isLastEffect) {
			// 最後のエフェクトは現在のレンダーターゲット（画面）に直接描画
			activeEffects[i]->Apply(currentInput, screen, width, height);
		}
		else {
			// 中間バッファに描画
			RHI::Texture* tempTarget = (i % 2 == 0) ? tempTexture1_.get() : tempTexture2_.get();
			const float clearColor[4] = { 0, 0, 0, 0 };
			dev->RenderTargetClear(tempTarget, clearColor);
			activeEffects[i]->Apply(currentInput, tempTarget, width, height);
			currentInput = tempTarget;
		}
	}

	// レンダーターゲットを復元
	dev->RenderTargetsSet(saved.i_Count, saved.p_Colors, saved.p_Depth);
}

void Layer::RemovePostEffect(int index) {
	if (index >= 0 && index < (int)postEffects.size()) {
		postEffects.erase(postEffects.begin() + index);
	}
}

void Layer::MovePostEffect(int fromIndex, int toIndex) {
	if (fromIndex < 0 || fromIndex >= (int)postEffects.size() ||
		toIndex < 0 || toIndex >= (int)postEffects.size()) {
		return;
	}

	auto effect = postEffects[fromIndex];
	postEffects.erase(postEffects.begin() + fromIndex);
	postEffects.insert(postEffects.begin() + toIndex, effect);

	for (size_t i = 0; i < postEffects.size(); i++) {
		postEffects[i]->priority = (int)i;
	}
}

void Layer::CreateTempBuffers(int width, int height) {
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	if (!dev) return;
	if (tempTexture1_ && bufferWidth_ == width && bufferHeight_ == height) {
		return;
	}

	ReleaseTempBuffers();
	bufferWidth_ = width;
	bufferHeight_ = height;

	RHI::TextureDesc td;
	td.i_Width = (uint32_t)width;
	td.i_Height = (uint32_t)height;
	td.e_Format = RHI::Format::R16G16B16A16_Float;
	td.i_BindFlags = RHI::Bind_RenderTarget | RHI::Bind_ShaderResource;
	td.c_DebugName = "Layer.PostEffectTemp";
	tempTexture1_ = dev->TextureCreate(td, nullptr);
	tempTexture2_ = dev->TextureCreate(td, nullptr);
}

void Layer::ReleaseTempBuffers() {
	tempTexture1_.reset();
	tempTexture2_.reset();
	bufferWidth_ = 0;
	bufferHeight_ = 0;
}

void Layer::DrawInspector() {
	ImGui::PushID(this);

	char nameBuf[128];
	strcpy_s(nameBuf, name.c_str());
	if (ImGui::InputText("名前", nameBuf, sizeof(nameBuf))) {
		name = nameBuf;
	}

	ImGui::Checkbox("表示", &visible);
	ImGui::DragFloat("不透明度", &opacity, 0.01f, 0.0f, 1.0f);

	ImGui::Separator();
	ImGui::Text("ポストエフェクト (%d個)", (int)postEffects.size());

	int removeIndex = -1;
	int moveFromIndex = -1;
	int moveToIndex = -1;

	for (size_t i = 0; i < postEffects.size(); i++) {
		auto& effect = postEffects[i];
		if (!effect) continue;

		ImGui::PushID((int)i);

		std::string header = effect->GetName() + " ##header" + std::to_string(i);
		bool headerOpen = ImGui::CollapsingHeader(header.c_str());

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
			ImGui::SetDragDropPayload("POSTEFFECT_REORDER", &i, sizeof(int));
			ImGui::Text("%s", effect->GetName().c_str());
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("POSTEFFECT_REORDER")) {
				int draggedIndex = *(int*)payload->Data;
				moveFromIndex = draggedIndex;
				moveToIndex = (int)i;
			}
			ImGui::EndDragDropTarget();
		}

		if (headerOpen) {
			ImGui::Indent();

			ImGui::Checkbox("有効", &effect->enabled);
			ImGui::DragInt("優先度", &effect->priority);

			ImGui::Separator();

			effect->DrawInspector();

			ImGui::Separator();

			if (ImGui::Button("削除", ImVec2(100, 0))) {
				removeIndex = (int)i;
			}

			ImGui::Unindent();
		}

		ImGui::PopID();
	}

	if (removeIndex >= 0) {
		RemovePostEffect(removeIndex);
	}

	if (moveFromIndex >= 0 && moveToIndex >= 0) {
		MovePostEffect(moveFromIndex, moveToIndex);
	}

	ImGui::Separator();

	static const char* effectNames[] = {
		"Bloom",
		"Pixelate",
		"Color Grading",
		"SSAO",
		"Atmospheric Fog",
	};
	static int currentEffect = 0;

	ImGui::Combo("##EffectType", &currentEffect, effectNames, IM_ARRAYSIZE(effectNames));
	ImGui::SameLine();
	if (ImGui::Button("エフェクト追加", ImVec2(120, 0))) {
		AddPostEffect((PostEffectType)(currentEffect + 1));
	}

	ImGui::PopID();
}

// JSONに保存
void Layer::SaveToJson(nlohmann::json& j) const {
	j["layerIndex"] = layerIndex;
	j["name"] = name;
	j["visible"] = visible;
	j["opacity"] = opacity;

	nlohmann::json effectsArray = nlohmann::json::array();
	for (const auto& effect : postEffects) {
		if (effect) {
			nlohmann::json effectJson;
			effectJson["type"] = (int)effect->GetType();
			effectJson["enabled"] = effect->enabled;
			effectJson["priority"] = effect->priority;

			nlohmann::json params;
			effect->SaveToJson(params);
			effectJson["params"] = params;

			effectsArray.push_back(effectJson);
		}
	}
	j["postEffects"] = effectsArray;
}

// JSONから読み込み
void Layer::LoadFromJson(const nlohmann::json& j) {
	layerIndex = j.value("layerIndex", 0);
	name = j.value("name", "Layer");
	visible = j.value("visible", true);
	opacity = j.value("opacity", 1.0f);

	postEffects.clear();
	if (j.contains("postEffects") && j["postEffects"].is_array()) {
		for (const auto& effectJson : j["postEffects"]) {
			PostEffectType type = (PostEffectType)effectJson.value("type", 0);
			auto effect = CreatePostEffect(type);

			if (effect) {
				effect->enabled = effectJson.value("enabled", true);
				effect->priority = effectJson.value("priority", 0);

				if (effectJson.contains("params")) {
					effect->LoadFromJson(effectJson["params"]);
				}

				postEffects.push_back(effect);
			}
		}
	}
}

// ポストエフェクトを生成
std::shared_ptr<PostEffectBase> Layer::CreatePostEffect(PostEffectType type) {
	switch (type) {
	case PostEffectType::PIXELATE:        return std::make_shared<PixelateEffect>();
	case PostEffectType::BLOOM:           return std::make_shared<BloomEffect>();
	case PostEffectType::COLOR_GRADING:   return std::make_shared<ColorGradingEffect>();
	case PostEffectType::SSAO:            return std::make_shared<SSAOEffect>();
	case PostEffectType::ATMOSPHERIC_FOG: return std::make_shared<AtmosphericFogEffect>();
	default: return nullptr;
	}
}
