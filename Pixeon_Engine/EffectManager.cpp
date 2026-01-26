/*
* ファイル名 EffectManager.cpp
* 概要      Effekseerエフェクト管理クラス実装
*/
#include "EffectManager.h"
#include "AssetManager.h"
#include "IMGUI/imgui.h"
#include <vector>

EffectManager* EffectManager::_instance = nullptr;

// ========================================
// カスタムローダー群 (AssetManager利用)
// ========================================

class EffectManager::EffectLoader : public Effekseer::EffectLoader
{
public:
    bool Load(const char16_t* path, void*& data, int32_t& size) override
    {
        // UTF-16 から UTF-8 へ変換
        std::string utf8Path;
        const char16_t* p = path;
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
public:
    TextureLoader() {}

    Effekseer::TextureRef Load(const char16_t* path, Effekseer::TextureType textureType) override
    {
        // UTF-16 から UTF-8 へ変換
        std::string utf8Path;
        const char16_t* p = path;
        while (*p) {
            if (*p < 0x80) utf8Path.push_back((char)*p);
            else utf8Path.push_back('?');
            p++;
        }

        std::vector<uint8_t> fileData;
        if (!AssetManager::Instance()->LoadAsset(utf8Path, fileData)) {
            return nullptr;
        }

        // メモリから直接読み込む
        return Load(fileData.data(), (int32_t)fileData.size(), textureType, true);
    }

    Effekseer::TextureRef Load(const void* data, int32_t size, Effekseer::TextureType textureType, bool isMipMapEnabled) override
    {
        // 実際にはここでテクスチャを生成する必要がありますが、
        // Effekseer 1.7では独自実装が必要です
        // 簡易実装として、Effekseerのデフォルト処理に委譲
        return nullptr; // TODO: 実装が必要
    }

    void Unload(Effekseer::TextureRef data) override
    {
        // RefPtrは自動的に解放されるため何もしない
    }
};

class EffectManager::ModelLoader : public Effekseer::ModelLoader
{
public:
    ModelLoader() {}

    Effekseer::ModelRef Load(const char16_t* path) override
    {
        // UTF-16 から UTF-8 へ変換
        std::string utf8Path;
        const char16_t* p = path;
        while (*p) {
            if (*p < 0x80) utf8Path.push_back((char)*p);
            else utf8Path.push_back('?');
            p++;
        }

        std::vector<uint8_t> fileData;
        if (!AssetManager::Instance()->LoadAsset(utf8Path, fileData)) {
            return nullptr;
        }

        return Load(fileData.data(), (int32_t)fileData.size());
    }

    Effekseer::ModelRef Load(const void* data, int32_t size) override
    {
        // メモリからモデルを作成
        return Effekseer::MakeRefPtr<Effekseer::Model>(data, size);
    }

    void Unload(Effekseer::ModelRef data) override
    {
        // RefPtrは自動的に解放されるため何もしない
    }
};

// ========================================
// シングルトン管理
// ========================================

EffectManager* EffectManager::Instance()
{
    if (_instance == nullptr) {
        _instance = new EffectManager();
    }
    return _instance;
}

void EffectManager::DeleteInstance()
{
    if (_instance != nullptr) {
        _instance->UnInit();
        delete _instance;
        _instance = nullptr;
    }
}

EffectManager::~EffectManager()
{
    UnInit();
}

// ========================================
// 初期化・終了処理
// ========================================

bool EffectManager::Init(ID3D11Device* device, ID3D11DeviceContext* context, int maxParticles)
{
    if (_initialized) return true;

    _device = device;
    _context = context;
    _maxParticles = maxParticles;

    // Effekseer Manager 作成
    _manager = Effekseer::Manager::Create(maxParticles);
    if (_manager == nullptr) return false;

    // Renderer 作成
    _renderer = EffekseerRendererDX11::Renderer::Create(device, context, maxParticles);
    if (_renderer == nullptr) return false;

    // カスタムローダー設定
    _manager->GetSetting()->SetEffectLoader(Effekseer::MakeRefPtr<EffectLoader>());
    _manager->GetSetting()->SetTextureLoader(Effekseer::MakeRefPtr<TextureLoader>());
    _manager->GetSetting()->SetModelLoader(Effekseer::MakeRefPtr<ModelLoader>());

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

void EffectManager::UnInit()
{
    if (!_initialized) return;

    StopAllEffects();
    ClearAllEffects();

    if (_manager != nullptr) {
        _manager.Reset();
    }
    if (_renderer != nullptr) {
        _renderer.Reset();
    }

    _device = nullptr;
    _context = nullptr;
    _initialized = false;
}

// ========================================
// エフェクトリソース管理
// ========================================

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
        path16.push_back((char16_t)(unsigned char)c);
    }

    Effekseer::EffectRef effect = Effekseer::Effect::Create(
        _manager,
        (void*)data.data(),
        (int32_t)data.size(),
        1.0f,
        path16.c_str()
    );

    if (effect != nullptr) {
        _effectCache[path] = effect;
    }

    return effect;
}

void EffectManager::UnloadEffect(const std::string& path)
{
    auto it = _effectCache.find(path);
    if (it != _effectCache.end()) {
        _effectCache.erase(it);
    }
}

void EffectManager::ClearAllEffects()
{
    _effectCache.clear();
}

// ========================================
// エフェクト再生制御
// ========================================

Effekseer::Handle EffectManager::PlayEffect(const std::string& path, const DirectX::XMFLOAT3& position)
{
    if (!_initialized) return -1;

    Effekseer::EffectRef effect = LoadEffect(path);
    if (effect == nullptr) return -1;

    Effekseer::Handle handle = _manager->Play(effect, position.x, position.y, position.z);
    return handle;
}

void EffectManager::StopEffect(Effekseer::Handle handle)
{
    if (!_initialized || handle < 0) return;
    _manager->StopEffect(handle);
}

void EffectManager::StopAllEffects()
{
    if (!_initialized) return;
    _manager->StopAllEffects();
}

bool EffectManager::IsPlaying(Effekseer::Handle handle)
{
    if (!_initialized || handle < 0) return false;
    return _manager->Exists(handle);
}

// ========================================
// エフェクトパラメータ設定
// ========================================

void EffectManager::SetEffectPosition(Effekseer::Handle handle, const DirectX::XMFLOAT3& position)
{
    if (!_initialized || handle < 0) return;
    _manager->SetLocation(handle, position.x, position.y, position.z);
}

void EffectManager::SetEffectRotation(Effekseer::Handle handle, const DirectX::XMFLOAT3& rotation)
{
    if (!_initialized || handle < 0) return;
    _manager->SetRotation(handle, rotation.x, rotation.y, rotation.z);
}

void EffectManager::SetEffectScale(Effekseer::Handle handle, const DirectX::XMFLOAT3& scale)
{
    if (!_initialized || handle < 0) return;
    _manager->SetScale(handle, scale.x, scale.y, scale.z);
}

void EffectManager::SetEffectSpeed(Effekseer::Handle handle, float speed)
{
    if (!_initialized || handle < 0) return;
    _manager->SetSpeed(handle, speed);
}

// ========================================
// カメラ設定
// ========================================

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

void EffectManager::Update(float deltaTime)
{
    if (!_initialized) return;

    // Effekseerは60FPSで更新回数を指定
    float updateCount = deltaTime * 60.0f;
    _manager->Update(updateCount);
}

void EffectManager::Draw()
{
    if (!_initialized) return;

    _renderer->BeginRendering();
    _manager->Draw();
    _renderer->EndRendering();
}

// ========================================
// デバッグ用
// ========================================

int EffectManager::GetPlayingEffectsCount() const
{
    if (!_initialized) return 0;
    return _manager->GetRestInstancesCount();
}

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