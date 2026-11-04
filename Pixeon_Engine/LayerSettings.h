// Layer.h
#pragma once
#include <vector>
#include <string>
#include <memory>
#include <DirectXMath.h>
#include "RHI/RHI.h"
#include "GUI.h"
#include <nlohmann/json.hpp>

#define MAX_LAYER_COUNT (10)

enum class PostEffectType {
	NONE,
	BLOOM,
	PIXELATE,
	COLOR_GRADING,
	SSAO,
	ATMOSPHERIC_FOG,
	MAX
};

class PostEffectBase {
public:
	virtual ~PostEffectBase() = default;
	// input を読み、output（レンダーターゲット）へ結果を書き込む
	virtual void Apply(RHI::Texture* input,
		RHI::Texture* output,
		int width, int height) = 0;
	virtual void DrawInspector() = 0;
	virtual std::string GetName() const = 0;
	virtual PostEffectType GetType() const = 0;
	virtual void SaveToJson(nlohmann::json& j) const = 0;
	virtual void LoadFromJson(const nlohmann::json& j) = 0;
	virtual PostEffectBase* Clone() const = 0;

	bool enabled = true;
	int priority = 0;

protected:
	// 描画先を output に切り替え、スコープを抜けると元の描画先に戻す
	class ScopedOutput {
	public:
		ScopedOutput(RHI::Texture* output, int width, int height);
		~ScopedOutput();
	private:
		RHI::RenderTargetState m_str_Saved;
		RHI::Viewport m_str_SavedViewport;
	};
};

class Layer {
public:
	Layer();
	~Layer();

	int layerIndex = 0;
	std::string name = "Layer";
	bool visible = true;
	float opacity = 1.0f;

	std::vector<std::shared_ptr<PostEffectBase>> postEffects;

	void AddPostEffect(PostEffectType type);

	// ★ invView と cameraPos を追加
	void ApplyPostEffectsToScreen(
		RHI::Texture* input,
		int width, int height,
		float opacity,
		RHI::Texture* depth = nullptr,
		const DirectX::XMMATRIX* proj = nullptr,
		const DirectX::XMMATRIX* invProj = nullptr,
		const DirectX::XMMATRIX* invView = nullptr,   // ★追加
		const DirectX::XMFLOAT3* cameraPos = nullptr    // ★追加
	);

	void RemovePostEffect(int index);
	void MovePostEffect(int fromIndex, int toIndex);
	void DrawInspector();
	void SaveToJson(nlohmann::json& j) const;
	void LoadFromJson(const nlohmann::json& j);

private:
	// エフェクトを連続適用するためのピンポンバッファ
	RHI::TextureRef tempTexture1_;
	RHI::TextureRef tempTexture2_;

	int bufferWidth_ = 0;
	int bufferHeight_ = 0;

	void CreateTempBuffers(int width, int height);
	void ReleaseTempBuffers();
	std::shared_ptr<PostEffectBase> CreatePostEffect(PostEffectType type);
};
