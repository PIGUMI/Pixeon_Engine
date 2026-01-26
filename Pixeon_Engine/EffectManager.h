/*
* ファイル名 EffectManager.h
* 概要      Effekseerエフェクト管理クラス
*/
#ifndef EFFECTMANAGER_H
#define EFFECTMANAGER_H

#include "Effekseer/Effekseer.h"
#include "Effekseer/include/EffekseerRendererDX11/EffekseerRendererDX11.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <DirectXMath.h>

class EffectManager
{
public:
    static EffectManager* Instance();
    static void DeleteInstance();

    // 初期化・終了処理
    bool Init(ID3D11Device* device, ID3D11DeviceContext* context, int maxParticles = 8000);
    void UnInit();

    // エフェクトリソース管理
    Effekseer::EffectRef LoadEffect(const std::string& path);
    void UnloadEffect(const std::string& path);
    void ClearAllEffects();

    // エフェクト再生制御
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

    // デバッグ用
    void DrawDebugGUI();

    // ゲッター
    Effekseer::ManagerRef GetManager() const { return _manager; }
    int GetPlayingEffectsCount() const;

private:
    EffectManager() = default;
    ~EffectManager();

    // Effekseerカスタムローダー (AssetManager経由でロード)
    class EffectLoader;
    class TextureLoader;
    class ModelLoader;

    static EffectManager* _instance;

    // Effekseerコアオブジェクト
    Effekseer::ManagerRef _manager;
    EffekseerRendererDX11::RendererRef _renderer;

    // DirectXデバイス参照
    ID3D11Device* _device = nullptr;
    ID3D11DeviceContext* _context = nullptr;

    // ロードされたエフェクトのキャッシュ
    std::unordered_map<std::string, Effekseer::EffectRef> _effectCache;

    // 設定
    int _maxParticles = 8000;
    bool _initialized = false;
};

#endif // EFFECTMANAGER_H