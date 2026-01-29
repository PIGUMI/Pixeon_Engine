// Layer.h
#pragma once
#include <vector>
#include <string>
#include <memory>
#include <d3d11.h>
#include <DirectXMath.h>
#include "GUI.h"
#include <nlohmann/json.hpp>

#define MAX_LAYER_COUNT (10)

enum class PostEffectType {
	NONE,
	BLOOM,
	BLUR,
	PIXELATE,
	COLOR_GRADING,
	VIGNETTE,
	CHROMATIC_ABERRATION,
	MAX
};

// ポストエフェクトの基底クラス
class PostEffectBase {
public:
	virtual ~PostEffectBase() = default;
	virtual void Apply(ID3D11ShaderResourceView* input,
		ID3D11RenderTargetView* output,
		int width, int height) = 0;
	virtual void DrawInspector() = 0;
	virtual std::string GetName() const = 0;
	virtual PostEffectType GetType() const = 0;
	virtual void SaveToJson(nlohmann::json& j) const = 0;
	virtual void LoadFromJson(const nlohmann::json& j) = 0;
	virtual PostEffectBase* Clone() const = 0;

	bool enabled = true;
	int priority = 0;
};

class Layer {
public:
	Layer();
	~Layer();

	int layerIndex = 0;
	std::string name = "Layer";
	bool visible = true;
	float opacity = 1.0f;

	// ポストエフェクト管理
	std::vector<std::shared_ptr<PostEffectBase>> postEffects;

	// 関数
	void AddPostEffect(PostEffectType type);
	void ApplyPostEffectsToScreen(ID3D11ShaderResourceView* input,
		int width, int height,
		float opacity);
	void RemovePostEffect(int index);
	void MovePostEffect(int fromIndex, int toIndex);
	void ApplyPostEffects(ID3D11ShaderResourceView* input,
		ID3D11RenderTargetView* output,
		int width, int height);
	void DrawInspector();
	void SaveToJson(nlohmann::json& j) const;
	void LoadFromJson(const nlohmann::json& j);

private:
	ID3D11Texture2D* tempTexture1_ = nullptr;
	ID3D11Texture2D* tempTexture2_ = nullptr;
	ID3D11RenderTargetView* tempRTV1_ = nullptr;
	ID3D11RenderTargetView* tempRTV2_ = nullptr;
	ID3D11ShaderResourceView* tempSRV1_ = nullptr;
	ID3D11ShaderResourceView* tempSRV2_ = nullptr;

	int bufferWidth_ = 0;
	int bufferHeight_ = 0;

	void CreateTempBuffers(int width, int height);
	void ReleaseTempBuffers();
	std::shared_ptr<PostEffectBase> CreatePostEffect(PostEffectType type);
};