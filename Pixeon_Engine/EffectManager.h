/*
* ファイル名 EffectManager.h
* 概要      Effekseerエフェクト管理クラス (EffekseerForCpp 1.7.2.0)
*           描画は RHI のバックエンドに合わせて EffekseerRendererDX11 / DX12 を使い分ける
*           Vulkan 用の Effekseer レンダラーは同梱していないため、Vulkan ではエフェクトを描画しない
*/
#ifndef EFFECTMANAGER_H
#define EFFECTMANAGER_H

#include "Effekseer/Effekseer.h"
#include "RHI/RHI.h"

#include <unordered_map>
#include <string>
#include <vector>

#include <DirectXMath.h>

namespace EffekseerRenderer { class Renderer; class CommandList; class SingleFrameMemoryPool; }

class EffectManager
{
public:
	static EffectManager* Instance();
	static void DeleteInstance();

	// 初期化・終了
	bool Init(RHI::Device* device, int maxParticles = 8000);
	void UnInit();

	// エフェクトリソース管理
	Effekseer::EffectRef LoadEffect(const std::string& path);
	void UnloadEffect(const std::string& path);
	void ClearAllEffects();

	// エフェクト再生
	Effekseer::Handle PlayEffect(const std::string& path, const DirectX::XMFLOAT3& position);
	void StopEffect(Effekseer::Handle handle);
	void StopAllEffects();
	bool IsPlaying(Effekseer::Handle handle);

	// エフェクトパラメータ設定
	void SetEffectPosition(Effekseer::Handle handle, const DirectX::XMFLOAT3& position);
	void SetEffectRotation(Effekseer::Handle handle, const DirectX::XMFLOAT3& rotation);
	void SetEffectScale(Effekseer::Handle handle, const DirectX::XMFLOAT3& scale);
	void SetEffectSpeed(Effekseer::Handle handle, float speed);

	// カメラ設定
	void SetCamera(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection);

	// 更新・描画
	void Update(float deltaTime = 1.0f / 60.0f);
	void Draw();

	// デバッグ
	void DrawDebugGUI();
	int GetPlayingEffectsCount() const;

	Effekseer::ManagerRef GetManager() const { return _manager; }

private:
	EffectManager() = default;
	~EffectManager();

	// ローダー（AssetManager経由）
	class EffectLoader;
	class TextureLoader;
	class ModelLoader;

	static std::u16string Utf8ToUtf16(const std::string& s);
	static std::string Utf16ToUtf8(const char16_t* s);

	static EffectManager* _instance;

	bool RendererCreate();
	Effekseer::Backend::TextureRef BackendTextureCreate(RHI::Texture* texture);

	// Effekseer
	Effekseer::ManagerRef _manager;
	Effekseer::RefPtr<EffekseerRenderer::Renderer> _renderer;
	Effekseer::RefPtr<EffekseerRenderer::CommandList> _commandList;          // DX12 のみ
	Effekseer::RefPtr<EffekseerRenderer::SingleFrameMemoryPool> _memoryPool; // DX12 のみ

	RHI::Device* _device = nullptr;
	RHI::BlendStateRef _blendState;

	// キャッシュ
	std::unordered_map<std::string, Effekseer::EffectRef> _effectCache;

	struct TextureCacheEntry
	{
		Effekseer::TextureRef tex;  // Effekseer::Texture（内部にBackend::Texture）
		RHI::TextureRef texture;    // 実体保持
		int w = 0;
		int h = 0;
		uint64_t bytes = 0;
	};
	std::unordered_map<std::string, TextureCacheEntry> _textureCache;
	std::vector<TextureCacheEntry> _memoryTextures;   // メモリから読み込んだテクスチャ（エフェクト埋め込み）

	int _maxParticles = 8000;
	bool _initialized = false;
};

#endif // EFFECTMANAGER_H
