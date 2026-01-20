// Layer.cpp
#include "LayerSettings.h"
#include "System.h"
#include "ImageUtils.h"
#include <algorithm>

Layer::Layer() {
}

Layer::~Layer() {
    ReleaseTempBuffers();
}

// ポストエフェクトを追加
void Layer::AddPostEffect(PostEffectType type) {
    auto effect = CreatePostEffect(type);
    if (effect) {
        postEffects.push_back(effect);

        // 優先度順にソート
        std::sort(postEffects.begin(), postEffects.end(),
            [](const std::shared_ptr<PostEffectBase>& a,
                const std::shared_ptr<PostEffectBase>& b) {
                    return a->priority < b->priority;
            });
    }
}

// ポストエフェクトを削除
void Layer::RemovePostEffect(int index) {
    if (index >= 0 && index < postEffects.size()) {
        postEffects.erase(postEffects.begin() + index);
    }
}

// ポストエフェクトの順序を変更
void Layer::MovePostEffect(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= postEffects.size() ||
        toIndex < 0 || toIndex >= postEffects.size()) {
        return;
    }

    auto effect = postEffects[fromIndex];
    postEffects.erase(postEffects.begin() + fromIndex);
    postEffects.insert(postEffects.begin() + toIndex, effect);

    // 優先度を更新
    for (size_t i = 0; i < postEffects.size(); i++) {
        postEffects[i]->priority = (int)i;
    }
}

// 中間バッファを作成
void Layer::CreateTempBuffers(int width, int height) {
    auto* dx = DirectX11::GetInstance();
    if (!dx) return;

    ID3D11Device* device = dx->GetDevice();
    if (!device) return;

    // すでに同じサイズのバッファがあれば何もしない
    if (tempTexture1_ && bufferWidth_ == width && bufferHeight_ == height) {
        return;
    }

    // 古いバッファを解放
    ReleaseTempBuffers();

    bufferWidth_ = width;
    bufferHeight_ = height;

    // テクスチャ作成用の設定
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // HDR対応
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;

    // バッファ1作成
    HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &tempTexture1_);
    if (SUCCEEDED(hr)) {
        device->CreateRenderTargetView(tempTexture1_, nullptr, &tempRTV1_);
        device->CreateShaderResourceView(tempTexture1_, nullptr, &tempSRV1_);
    }

    // バッファ2作成
    hr = device->CreateTexture2D(&texDesc, nullptr, &tempTexture2_);
    if (SUCCEEDED(hr)) {
        device->CreateRenderTargetView(tempTexture2_, nullptr, &tempRTV2_);
        device->CreateShaderResourceView(tempTexture2_, nullptr, &tempSRV2_);
    }
}

// 中間バッファを解放
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

// ポストエフェクトを適用
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

    // 中間バッファを準備
    CreateTempBuffers(width, height);

    if (!tempRTV1_ || !tempRTV2_ || !tempSRV1_ || !tempSRV2_) {
        return;
    }

    // 有効なエフェクトのみを抽出
    std::vector<std::shared_ptr<PostEffectBase>> activeEffects;
    for (auto& effect : postEffects) {
        if (effect && effect->enabled) {
            activeEffects.push_back(effect);
        }
    }

    if (activeEffects.empty()) {
        // エフェクトがない場合は入力をそのまま出力にコピー
        ImageUtils::DrawSRV(input, 0, 0, (float)width, (float)height,
            DirectX::XMFLOAT4(1, 1, 1, opacity));
        return;
    }

    // Ping-Pongバッファリングでエフェクトを順次適用
    ID3D11ShaderResourceView* currentInput = input;
    ID3D11RenderTargetView* currentOutput = nullptr;
    ID3D11ShaderResourceView* nextInput = nullptr;

    for (size_t i = 0; i < activeEffects.size(); i++) {
        bool isLastEffect = (i == activeEffects.size() - 1);

        if (isLastEffect) {
            // 最後のエフェクトは最終出力に描画
            currentOutput = output;
        }
        else {
            // 中間バッファにPing-Pong
            currentOutput = (i % 2 == 0) ? tempRTV1_ : tempRTV2_;
            nextInput = (i % 2 == 0) ? tempSRV1_ : tempSRV2_;
        }

        // レンダーターゲットをクリア
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        ctx->ClearRenderTargetView(currentOutput, clearColor);
        ctx->OMSetRenderTargets(1, &currentOutput, nullptr);

        // エフェクトを適用
        activeEffects[i]->Apply(currentInput, currentOutput, width, height);

        // 次のループの入力を設定
        if (!isLastEffect) {
            currentInput = nextInput;
        }
    }

    // レンダーターゲットをリセット
    ID3D11RenderTargetView* nullRTV = nullptr;
    ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
}

// Inspector用のGUIを描画
void Layer::DrawInspector() {
    ImGui::PushID(this);

    // 基本設定
    char nameBuf[128];
    strcpy_s(nameBuf, name.c_str());
    if (ImGui::InputText("名前", nameBuf, sizeof(nameBuf))) {
        name = nameBuf;
    }

    ImGui::Checkbox("表示", &visible);
    ImGui::DragFloat("不透明度", &opacity, 0.01f, 0.0f, 1.0f);

    ImGui::Separator();
    ImGui::Text("ポストエフェクト (%d個)", (int)postEffects.size());

    // ポストエフェクト一覧
    int removeIndex = -1;
    int moveFromIndex = -1;
    int moveToIndex = -1;

    for (size_t i = 0; i < postEffects.size(); i++) {
        auto& effect = postEffects[i];
        if (!effect) continue;

        ImGui::PushID((int)i);

        // エフェクト名とON/OFFトグル
        std::string header = effect->GetName() + " ##header" + std::to_string(i);
        bool headerOpen = ImGui::CollapsingHeader(header.c_str());

        // ドラッグ&ドロップで並び替え
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

            // エフェクト固有のパラメータ
            effect->DrawInspector();

            ImGui::Separator();

            // 削除ボタン
            if (ImGui::Button("削除", ImVec2(100, 0))) {
                removeIndex = (int)i;
            }

            ImGui::Unindent();
        }

        ImGui::PopID();
    }

    // 削除処理
    if (removeIndex >= 0) {
        RemovePostEffect(removeIndex);
    }

    // 移動処理
    if (moveFromIndex >= 0 && moveToIndex >= 0) {
        MovePostEffect(moveFromIndex, moveToIndex);
    }

    ImGui::Separator();

    // エフェクト追加UI
    static const char* effectNames[] = {
        "Bloom",
        "Blur",
        "Pixelate",
        "Color Grading",
        "Vignette",
        "Chromatic Aberration"
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
    default:
        return nullptr;
    }
}