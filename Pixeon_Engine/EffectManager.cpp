/*
* ファイル名　EffectManager.cpp
* 説　　　明　Effekseerエフェクト管理クラス実装
*/
#include "EffectManager.h"
#include "AssetManager.h"
#include "IMGUI/imgui.h"
#include <vector>

EffectManager* EffectManager::_instance = nullptr;

// ========================================
// カスタムローダー実装 (AssetManager連携)
// ========================================

class EffectManager::EffectLoader : public Effekseer::EffectLoader
{
public:
    bool Load(const char16_t* path, void*& data, int32_t& size) override
    {
        // UTF-16 から UTF-8 へ変換
        std::string utf8Path;
        char16_t* p = (char16_t*)path;
        while (*p) {
            if (*p < 0x80) utf8Path.push_back((char)*p);
            else utf8Path.push_back('?'); // 簡易変換
            p++;
        }

        std::vector<uint8_t> fileData;
        if (!AssetManager::Instance()->LoadAsset(utf8Path, fileData)) {
            return false;
        }

        size = (int32_t)fileData.size();
        data = malloc(size);
        if (data == nullptr) return false;

        memcpy(data, fileData.data(), size);
        return true;
    }

    void Unload(void* data, int32_t size) override
    {
        if (data) free(data);
    }
};

class EffectManager::TextureLoader : public Effekseer::TextureLoader
{
private:
    EffekseerRenderer::RendererRef _renderer;

public:
    TextureLoader(EffekseerRenderer::RendererRef renderer) : _renderer(renderer) {}

    Effekseer::TextureRef Load(const char16_t* path, Effekseer::TextureType textureType) override
    {
        // UTF-16 から UTF-8 へ変換
        std::string utf8Path;
        char16_t* p = (char16_t*)path;
        while (*p) {
            if (*p < 0x80) utf8Path.push_back((char)*p);
            else utf8Path.push_back('?');
            p++;
        }

        std::vector<uint8_t> fileData;
        if (!AssetManager::Instance()->LoadAsset(utf8Path, fileData)) {
            return nullptr;
        }

        // DX11Rendererのテクスチャローダーに委譲
        auto dx11Renderer = (EffekseerRendererDX11::Renderer*)_renderer.Get();
        return dx11Renderer->GetTextureLoader()->Load(fileData.data(), (int32_t)fileData.size(), textureType);
    }

    void Unload(Effekseer::TextureRef data) override
    {
        if (data) data->Release();
    }
};

class EffectManager::ModelLoader : public Effekseer::ModelLoader
{
private:
    EffekseerRenderer::RendererRef _renderer;

public:
    ModelLoader(EffekseerRenderer::RendererRef renderer) : _renderer(renderer) {}

    Effekseer::ModelRef Load(const char16_t* path) override
    {
        // UTF-16 から UTF-8 へ変換
        std::string utf8Path;
        char16_t* p = (char16_t*)path;
        while (*p) {
            if (*p < 0x80) utf8Path.push_back((char)*p);
            else utf8Path.push_back('?');
            p++;
        }

        std::vector<uint8_t> fileData;
        if (!AssetManager::Instance()->LoadAsset(utf8Path, fileData)) {
            return nullptr;
        }

        auto dx11Renderer = (EffekseerRendererDX11::Renderer*)_renderer.Get();
        return dx11Renderer->GetModelLoader()->Load(fileData.data(), (int32_t)fileData.size());
    }

    void Unload(Effekseer::ModelRef data) override
    {
        if (data) data->Release();
    }
};

// ========================================
// シングルトン管理
// ========================================

/*
* 関数名　Instance
* 引　数　なし
* 戻り値　インスタンスのポインタ
* 説　明　EffectManagerのシングルトンインスタンスを取得する
*/
EffectManager* EffectManager::Instance()
{
    if (!_instance) {
        _instance = new EffectManager();
    }
    return _instance;
}

/*
* 関数名　DeleteInstance
* 引　数　なし
* 戻り値　なし
* 説　明　EffectManagerのシングルトンインスタンスを破棄する
*/
void EffectManager::DeleteInstance()
{
    if (_instance) {
        _instance->UnInit();
        delete _instance;
        _instance = nullptr;
    }
}

/*
* 関数名　~EffectManager
* 引　数　なし
* 戻り値　なし
* 説　明　EffectManagerのデストラクタ
*/
EffectManager::~EffectManager()
{
    UnInit();
}

// ========================================
// 初期化・終了処理
// ========================================

/*
* 関数名　Init
* 引　数　device：DirectX11デバイス
*         context：DirectX11デバイスコンテキスト
*         maxParticles：最大パーティクル数
* 戻り値　成功した場合true、失敗した場合false
* 説　明　EffectManagerを初期化する
*/
bool EffectManager::Init(ID3D11Device* device, ID3D11DeviceContext* context, int maxParticles)
{
    if (_initialized) return true;

    _device = device;
    _context = context;
    _maxParticles = maxParticles;

    // Effekseer Manager 作成
    _manager = Effekseer::Manager::Create(maxParticles);
    if (!_manager) return false;

    // Renderer 作成
    _renderer = EffekseerRendererDX11::Renderer::Create(device, context, maxParticles);
    if (!_renderer) return false;

    // カスタムローダー設定
    _manager->GetSetting()->SetEffectLoader(Effekseer::MakeRefPtr<EffectLoader>());
    _manager->GetSetting()->SetTextureLoader(Effekseer::MakeRefPtr<TextureLoader>(_renderer));
    _manager->GetSetting()->SetModelLoader(Effekseer::MakeRefPtr<ModelLoader>(_renderer));

    // レンダラー設定
    _manager->SetSpriteRenderer(_renderer->CreateSpriteRenderer());
    _manager->SetRibbonRenderer(_renderer->CreateRibbonRenderer());
    _manager->SetRingRenderer(_renderer->CreateRingRenderer());
    _manager->SetTrackRenderer(_renderer->CreateTrackRenderer());
    _manager->SetModelRenderer(_renderer->CreateModelRenderer());

    // 座標系設定 (左手座標系)
    _manager->SetCoordinateSystem(Effekseer::CoordinateSystem::LH);

    _initialized = true;
    return true;
}

/*
* 関数名　UnInit
* 引　数　なし
* 戻り値　なし
* 説　明　EffectManagerの終了処理を行う
*/
void EffectManager::UnInit()
{
    if (!_initialized) return;

    StopAllEffects();
    ClearAllEffects();

    if (_manager) {
        _manager.Reset();
    }
    if (_renderer) {
        _renderer.Reset();
    }

    _device = nullptr;
    _context = nullptr;
    _initialized = false;
}

// ========================================
// エフェクトリソース管理
// ========================================

/*
* 関数名　LoadEffect
* 引　数　path：エフェクトファイルのパス
* 戻り値　ロードされたエフェクトの参照
* 説　明　エフェクトファイルをロードする
*/
Effekseer::EffectRef EffectManager::LoadEffect(const std::string& path)
{
    if (!_initialized) return nullptr;

    // キャッシュ確認
    auto it = _effectCache.find(path);
    if (it != _effectCache.end()) {
        return it->second;
    }

    // AssetManager経由でロード
    std::vector<uint8_t> data;
    if (!AssetManager::Instance()->LoadAsset(path, data)) {
        return nullptr;
    }

    // UTF-8 から UTF-16 へ変換
    std::u16string path16;
    for (char c : path) {
        path16.push_back((char16_t)c);
    }

    Effekseer::EffectRef effect = Effekseer::Effect::Create(_manager, (void*)data.data(), (int32_t)data.size(), 1.0f, path16.c_str());
    if (effect) {
        _effectCache[path] = effect;
    }

    return effect;
}

/*
* 関数名　UnloadEffect
* 引　数　path：エフェクトファイルのパス
* 戻り値　なし
* 説　明　エフェクトをアンロードする
*/
void EffectManager::UnloadEffect(const std::string& path)
{
    auto it = _effectCache.find(path);
    if (it != _effectCache.end()) {
        _effectCache.erase(it);
    }
}

/*
* 関数名　ClearAllEffects
* 引　数　なし
* 戻り値　なし
* 説　明　すべてのエフェクトをクリアする
*/
void EffectManager::ClearAllEffects()
{
    _effectCache.clear();
}

// ========================================
// エフェクト再生制御
// ========================================

/*
* 関数名　PlayEffect
* 引　数　path：エフェクトファイルのパス
*         position：再生位置
* 戻り値　エフェクトハンドル
* 説　明　エフェクトを再生する
*/
Effekseer::Handle EffectManager::PlayEffect(const std::string& path, const DirectX::XMFLOAT3& position)
{
    if (!_initialized) return -1;

    Effekseer::EffectRef effect = LoadEffect(path);
    if (!effect) return -1;

    Effekseer::Handle handle = _manager->Play(effect, position.x, position.y, position.z);
    return handle;
}

/*
* 関数名　StopEffect
* 引　数　handle：エフェクトハンドル
* 戻り値　なし
* 説　明　エフェクトを停止する
*/
void EffectManager::StopEffect(Effekseer::Handle handle)
{
    if (!_initialized || handle < 0) return;
    _manager->StopEffect(handle);
}

/*
* 関数名　StopAllEffects
* 引　数　なし
* 戻り値　なし
* 説　明　すべてのエフェクトを停止する
*/
void EffectManager::StopAllEffects()
{
    if (!_initialized) return;
    _manager->StopAllEffects();
}

/*
* 関数名　IsPlaying
* 引　数　handle：エフェクトハンドル
* 戻り値　再生中の場合true、停止している場合false
* 説　明　エフェクトが再生中かどうかを確認する
*/
bool EffectManager::IsPlaying(Effekseer::Handle handle)
{
    if (!_initialized || handle < 0) return false;
    return _manager->Exists(handle);
}

// ========================================
// エフェクトパラメータ設定
// ========================================

/*
* 関数名　SetEffectPosition
* 引　数　handle：エフェクトハンドル
*         position：位置
* 戻り値　なし
* 説　明　エフェクトの位置を設定する
*/
void EffectManager::SetEffectPosition(Effekseer::Handle handle, const DirectX::XMFLOAT3& position)
{
    if (!_initialized || handle < 0) return;
    _manager->SetLocation(handle, position.x, position.y, position.z);
}

/*
* 関数名　SetEffectRotation
* 引　数　handle：エフェクトハンドル
*         rotation：回転（ラジアン）
* 戻り値　なし
* 説　明　エフェクトの回転を設定する
*/
void EffectManager::SetEffectRotation(Effekseer::Handle handle, const DirectX::XMFLOAT3& rotation)
{
    if (!_initialized || handle < 0) return;
    _manager->SetRotation(handle, rotation.x, rotation.y, rotation.z);
}

/*
* 関数名　SetEffectScale
* 引　数　handle：エフェクトハンドル
*         scale：スケール
* 戻り値　なし
* 説　明　エフェクトのスケールを設定する
*/
void EffectManager::SetEffectScale(Effekseer::Handle handle, const DirectX::XMFLOAT3& scale)
{
    if (!_initialized || handle < 0) return;
    _manager->SetScale(handle, scale.x, scale.y, scale.z);
}

/*
* 関数名　SetEffectSpeed
* 引　数　handle：エフェクトハンドル
*         speed：再生速度
* 戻り値　なし
* 説　明　エフェクトの再生速度を設定する
*/
void EffectManager::SetEffectSpeed(Effekseer::Handle handle, float speed)
{
    if (!_initialized || handle < 0) return;
    _manager->SetSpeed(handle, speed);
}

// ========================================
// カメラ設定
// ========================================

/*
* 関数名　SetCamera
* 引　数　view：ビュー行列
*         projection：プロジェクション行列
* 戻り値　なし
* 説　明　カメラの設定を行う
*/
void EffectManager::SetCamera(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection)
{
    if (!_initialized) return;

    // DirectXMath行列からEffekseer行列へ変換
    Effekseer::Matrix44 effView, effProj;
    DirectX::XMFLOAT4X4 viewF, projF;
    DirectX::XMStoreFloat4x4(&viewF, view);
    DirectX::XMStoreFloat4x4(&projF, projection);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            effView.Values[i][j] = viewF.m[i][j];
            effProj.Values[i][j] = projF.m[i][j];
        }
    }

    _renderer->SetCameraMatrix(effView);
    _renderer->SetProjectionMatrix(effProj);
}

// ========================================
// 更新・描画
// ========================================

/*
* 関数名　Update
* 引　数　deltaTime：経過時間（秒）
* 戻り値　なし
* 説　明　エフェクトを更新する
*/
void EffectManager::Update(float deltaTime)
{
    if (!_initialized) return;

    // Effekseerは60FPS基準で更新回数を指定
    float updateCount = deltaTime * 60.0f;
    _manager->Update(updateCount);
}

/*
* 関数名　Draw
* 引　数　なし
* 戻り値　なし
* 説　明　エフェクトを描画する
*/
void EffectManager::Draw()
{
    if (!_initialized) return;

    _renderer->BeginRendering();
    _manager->Draw();
    _renderer->EndRendering();
}

// ========================================
// デバッグ情報
// ========================================

/*
* 関数名　GetPlayingEffectsCount
* 引　数　なし
* 戻り値　再生中のエフェクト数
* 説　明　再生中のエフェクト数を取得する
*/
int EffectManager::GetPlayingEffectsCount() const
{
    if (!_initialized) return 0;
    return _manager->GetRestInstancesCount();
}

/*
* 関数名　DrawDebugGUI
* 引　数　なし
* 戻り値　なし
* 説　明　デバッグGUIを描画する
*/
void EffectManager::DrawDebugGUI()
{
    ImGui::TextUnformatted("EffectManager (Effekseer)");
    ImGui::Separator();
    ImGui::Text("Initialized: %s", _initialized ? "Yes" : "No");
    ImGui::Text("Max Particles: %d", _maxParticles);
    ImGui::Text("Playing Effects: %d", GetPlayingEffectsCount());
    ImGui::Text("Loaded Effects: %zu", _effectCache.size());

    ImGui::Separator();
    if (ImGui::Button("Stop All Effects")) {
        StopAllEffects();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Cache")) {
        ClearAllEffects();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Loaded Effects:");
    ImGui::BeginChild("EffectList", ImVec2(0, 120), true);
    for (auto& kv : _effectCache) {
        ImGui::Text("%s", kv.first.c_str());
    }
    ImGui::EndChild();
}