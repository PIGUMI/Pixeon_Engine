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

void Layer::ApplyPostEffectsToScreen(
	ID3D11ShaderResourceView* input,
	int width, int height,
	float opacity,
	ID3D11ShaderResourceView* depthSRV,
	const DirectX::XMMATRIX* proj,
	const DirectX::XMMATRIX* invProj,
	const DirectX::XMMATRIX* invView,
	const DirectX::XMFLOAT3* cameraPos
) {
	if (!input) return;

	auto* dx = DirectX11::GetInstance();
	if (!dx) return;

	ID3D11DeviceContext* ctx = dx->GetContext();
	if (!ctx) return;

	// 有効なエフェクトを抽出
	std::vector<std::shared_ptr<PostEffectBase>> activeEffects;
	for (auto& effect : postEffects) {
		if (effect && effect->enabled) {
			activeEffects.push_back(effect);
		}
	}

	if (activeEffects.empty()) {
		// エフェクトなし: 通常描画
		ImageUtils::DrawSRV(input, 0, 0, (float)width, (float)height,
			DirectX::XMFLOAT4(1, 1, 1, 1),
			DirectX::XMFLOAT4(0, 0, 1, 1),
			true, opacity);
		return;
	}

	// 中間バッファを準備
	CreateTempBuffers(width, height);

	if (!tempRTV1_ || !tempSRV1_) {
		// バッファ作成失敗: 通常描画にフォールバック
		ImageUtils::DrawSRV(input, 0, 0, (float)width, (float)height,
			DirectX::XMFLOAT4(1, 1, 1, 1),
			DirectX::XMFLOAT4(0, 0, 1, 1),
			true, opacity);
		return;
	}

	// 現在のレンダーターゲットを保存
	ID3D11RenderTargetView* oldRTV = nullptr;
	ID3D11DepthStencilView* oldDSV = nullptr;
	ctx->OMGetRenderTargets(1, &oldRTV, &oldDSV);

	// 入力ソース
	ID3D11ShaderResourceView* currentInput = input;

	// エフェクトを順次適用
	for (size_t i = 0; i < activeEffects.size(); i++) {
		// ---- SSAO: Deferred実行済みならスキップ ----
		if (auto* ssao = dynamic_cast<SSAOEffect*>(activeEffects[i].get()))
		{
			if (ssao->IsAOGenerated())
			{
				ssao->ResetAOGenerated();
				bool isLast = (i == activeEffects.size() - 1);
				if (isLast && currentInput && oldRTV)
				{
					ctx->OMSetRenderTargets(1, &oldRTV, nullptr);
					D3D11_VIEWPORT vp{}; vp.Width = (float)width; vp.Height = (float)height; vp.MaxDepth = 1.0f;
					ctx->RSSetViewports(1, &vp);
					ImageUtils::DrawSRV(currentInput, 0.0f, 0.0f, (float)width, (float)height,
						DirectX::XMFLOAT4(1, 1, 1, 1), DirectX::XMFLOAT4(0, 0, 1, 1), true, 1.0f);
				}
				continue;
			}
			if (depthSRV)            ssao->SetDepthSRV(depthSRV);
			if (proj && invProj)     ssao->SetCameraMatrices(*proj, *invProj);
		}

		if (auto* fog = dynamic_cast<AtmosphericFogEffect*>(activeEffects[i].get()))
		{
			if (depthSRV) fog->SetDepthSRV(depthSRV);
			if (proj && invProj && invView && cameraPos)
			{
				fog->SetCameraMatrices(*proj, *invProj, *invView, *cameraPos);
			}
		}

		bool isLastEffect = (i == activeEffects.size() - 1);

		if (isLastEffect) {
			// 最後のエフェクトは現在のレンダーターゲット（画面）に直接描画
			activeEffects[i]->Apply(currentInput, oldRTV, width, height);
		}
		else {
			// 中間バッファに描画
			ID3D11RenderTargetView* tempTarget = (i % 2 == 0) ? tempRTV1_ : tempRTV2_;
			ID3D11ShaderResourceView* tempSource = (i % 2 == 0) ? tempSRV1_ : tempSRV2_;

			float clearColor[4] = { 0, 0, 0, 0 };
			ctx->ClearRenderTargetView(tempTarget, clearColor);

			activeEffects[i]->Apply(currentInput, tempTarget, width, height);

			// SRVバインド解除
			ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
			ctx->PSSetShaderResources(0, 1, nullSRV);

			// 次の入力を設定
			currentInput = tempSource;
		}
	}

	// レンダーターゲットを復元
	ctx->OMSetRenderTargets(1, oldRTV ? &oldRTV : nullptr, oldDSV);

	if (oldRTV) oldRTV->Release();
	if (oldDSV) oldDSV->Release();
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
	auto* dx = DirectX11::GetInstance();
	if (!dx) return;

	ID3D11Device* device = dx->GetDevice();
	if (!device) return;

	if (tempTexture1_ && bufferWidth_ == width && bufferHeight_ == height) {
		return;
	}

	ReleaseTempBuffers();

	bufferWidth_ = width;
	bufferHeight_ = height;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;

	HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &tempTexture1_);
	if (SUCCEEDED(hr)) {
		device->CreateRenderTargetView(tempTexture1_, nullptr, &tempRTV1_);
		device->CreateShaderResourceView(tempTexture1_, nullptr, &tempSRV1_);
	}

	hr = device->CreateTexture2D(&texDesc, nullptr, &tempTexture2_);
	if (SUCCEEDED(hr)) {
		device->CreateRenderTargetView(tempTexture2_, nullptr, &tempRTV2_);
		device->CreateShaderResourceView(tempTexture2_, nullptr, &tempSRV2_);
	}
}

void Layer::ReleaseTempBuffers() {
	if (tempSRV1_) { tempSRV1_->Release(); tempSRV1_ = nullptr; }
	if (tempSRV2_) { tempSRV2_->Release(); tempSRV2_ = nullptr; }
	if (tempRTV1_) { tempRTV1_->Release(); tempRTV1_ = nullptr; }
	if (tempRTV2_) { tempRTV2_->Release(); tempRTV2_ = nullptr; }
	if (tempTexture1_) { tempTexture1_->Release(); tempTexture1_ = nullptr; }
	if (tempTexture2_) { tempTexture2_->Release(); tempTexture2_ = nullptr; }

	bufferWidth_ = 0;
	bufferHeight_ = 0;
}

void Layer::ApplyPostEffects(ID3D11ShaderResourceView* input,
	ID3D11RenderTargetView* output,
	int width, int height) {
	if (postEffects.empty() || !input || !output) {
		return;
	}

	auto* dx = DirectX11::GetInstance();
	if (!dx) return;

	ID3D11DeviceContext* ctx = dx->GetContext();
	if (!ctx) return;

	CreateTempBuffers(width, height);

	if (!tempRTV1_ || !tempRTV2_ || !tempSRV1_ || !tempSRV2_) {
		return;
	}

	std::vector<std::shared_ptr<PostEffectBase>> activeEffects;
	for (auto& effect : postEffects) {
		if (effect && effect->enabled) {
			activeEffects.push_back(effect);
		}
	}

	if (activeEffects.empty()) {
		return;
	}

	ID3D11RenderTargetView* oldRTV = nullptr;
	ID3D11DepthStencilView* oldDSV = nullptr;
	ctx->OMGetRenderTargets(1, &oldRTV, &oldDSV);

	ID3D11ShaderResourceView* currentInput = input;
	ID3D11RenderTargetView* currentOutput = nullptr;
	ID3D11ShaderResourceView* nextInput = nullptr;

	for (size_t i = 0; i < activeEffects.size(); i++) {
		bool isLastEffect = (i == activeEffects.size() - 1);

		if (isLastEffect) {
			currentOutput = output;
		}
		else {
			currentOutput = (i % 2 == 0) ? tempRTV1_ : tempRTV2_;
			nextInput = (i % 2 == 0) ? tempSRV1_ : tempSRV2_;
		}

		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		ctx->ClearRenderTargetView(currentOutput, clearColor);

		activeEffects[i]->Apply(currentInput, currentOutput, width, height);

		ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
		ctx->PSSetShaderResources(0, 1, nullSRV);

		if (!isLastEffect) {
			currentInput = nextInput;
		}
	}

	ctx->OMSetRenderTargets(1, oldRTV ? &oldRTV : nullptr, oldDSV);

	if (oldRTV) oldRTV->Release();
	if (oldDSV) oldDSV->Release();
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