/*
* ファイル名 EffectManager.h
* 概要      Effekseerエフェクト管理クラス (EffekseerForCpp 1.7.2.0 / DX11)
*/
#ifndef EFFECTMANAGER_H
#define EFFECTMANAGER_H

#include "Effekseer/Effekseer.h"
#include "Effekseer/include/EffekseerRendererDX11/EffekseerRendererDX11.h"

#include <unordered_map>
#include <string>
#include <vector>

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

class EffectManager
{
public:
	static EffectManager* Instance();
	static void DeleteInstance();

	// 初期化・終了
	bool Init(ID3D11Device* device, ID3D11DeviceContext* context, int maxParticles = 8000);
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

	// Effekseer
	Effekseer::ManagerRef _manager;
	EffekseerRendererDX11::RendererRef _renderer;

	// DX11
	ID3D11Device* _device = nullptr;
	ID3D11DeviceContext* _context = nullptr;

	// キャッシュ
	std::unordered_map<std::string, Effekseer::EffectRef> _effectCache;

	struct TextureCacheEntry
	{
		Effekseer::TextureRef tex; // Effekseer::Texture（内部にBackend::Texture）
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv; // 実体保持（安全のため）
		int w = 0;
		int h = 0;
		uint64_t bytes = 0;
	};
	std::unordered_map<std::string, TextureCacheEntry> _textureCache;

	int _maxParticles = 8000;
	bool _initialized = false;
};

#endif // EFFECTMANAGER_H