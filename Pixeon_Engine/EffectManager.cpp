/*
* ファイル名 EffectManager.cpp
* 概要      Effekseerエフェクト管理クラス (EffekseerForCpp 1.7.2.0 / DX11)
*/
#include "EffectManager.h"
#include "AssetManager.h"
#include "IMGUI/imgui.h"

#include <Windows.h>
#include <algorithm>

// DirectXTex
#include "DirectXTex/TextureLoad.h"

EffectManager* EffectManager::_instance = nullptr;

//============================================================
// UTF変換
//============================================================
std::u16string EffectManager::Utf8ToUtf16(const std::string& s)
{
    if (s.empty()) return {};
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return {};
    std::wstring ws((size_t)wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws.data(), wlen);

    std::u16string out;
    out.resize(ws.size());
    for (size_t i = 0; i < ws.size(); i++) out[i] = (char16_t)ws[i];
    return out;
}

std::string EffectManager::Utf16ToUtf8(const char16_t* s)
{
    if (!s) return {};
    // Effekseerのヘルパもあるが、Windows APIで確実にUTF-8へ
    auto ws = reinterpret_cast<const wchar_t*>(s);
    int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out((size_t)len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws, -1, out.data(), len, nullptr, nullptr);
    return out;
}

//============================================================
// ローダー
//============================================================

class EffectManager::EffectLoader : public Effekseer::EffectLoader
{
public:
    bool Load(const char16_t* path, void*& data, int32_t& size) override
    {
        std::string utf8Path = EffectManager::Utf16ToUtf8(path);

        std::vector<uint8_t> fileData;
        if (!AssetManager::Instance()->LoadAsset(utf8Path, fileData)) {
            return false;
        }

        size = (int32_t)fileData.size();
        data = malloc(size);
        if (!data) return false;

        memcpy(data, fileData.data(), size);
        return true;
    }

    void Unload(void* data, int32_t /*size*/) override
    {
        if (data) free(data);
    }
};

class EffectManager::ModelLoader : public Effekseer::ModelLoader
{
public:
    Effekseer::ModelRef Load(const char16_t* path) override
    {
        std::string utf8Path = EffectManager::Utf16ToUtf8(path);

        std::vector<uint8_t> fileData;
        if (!AssetManager::Instance()->LoadAsset(utf8Path, fileData)) {
            return nullptr;
        }

        return Load(fileData.data(), (int32_t)fileData.size());
    }

    Effekseer::ModelRef Load(const void* data, int32_t size) override
    {
        return Effekseer::MakeRefPtr<Effekseer::Model>(data, size);
    }

    void Unload(Effekseer::ModelRef /*data*/) override
    {
        // RefPtrなので不要
    }
};

class EffectManager::TextureLoader : public Effekseer::TextureLoader
{
public:
    explicit TextureLoader(EffectManager* owner) : _owner(owner) {}

    Effekseer::TextureRef Load(const char16_t* path, Effekseer::TextureType /*textureType*/) override
    {
        if (!_owner || !_owner->_initialized) return nullptr;

        std::string utf8Path = EffectManager::Utf16ToUtf8(path);

        // Effekseerは "_NoMip" でミップ無効などの運用がある（Effekseer::TextureLoaderHelper参照）
        const bool mipEnabled = Effekseer::TextureLoaderHelper::GetIsMipmapEnabled(std::u16string(path));

        // キャッシュ
        auto it = _owner->_textureCache.find(utf8Path);
        if (it != _owner->_textureCache.end()) {
            return it->second.tex;
        }

        std::vector<uint8_t> fileData;
        if (!AssetManager::Instance()->LoadAsset(utf8Path, fileData)) {
            return nullptr;
        }

        int w = 0, h = 0;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        if (!CreateSRVFromMemory(fileData.data(), (int32_t)fileData.size(), mipEnabled, srv, w, h)) {
            return nullptr;
        }

        // ---- ここが EffekseerForCpp 1.7.2.0 / DX11 の正解 ----
        // SRV -> Effekseer::Backend::TextureRef を生成
        auto gd = _owner->_renderer->GetGraphicsDevice();
        Effekseer::Backend::TextureRef backendTex =
            EffekseerRendererDX11::CreateTexture(gd, srv.Get(), nullptr, nullptr);

        if (backendTex == nullptr) {
            return nullptr;
        }

        // Backend::Texture を Effekseer::Texture に包む
        Effekseer::TextureRef effTex = Effekseer::MakeRefPtr<Effekseer::Texture>();
        effTex->SetBackend(backendTex);

        // キャッシュ登録（SRVも保持しておくと安全）
        EffectManager::TextureCacheEntry e;
        e.tex = effTex;
        e.srv = srv;
        e.w = w;
        e.h = h;
        e.bytes = (uint64_t)w * (uint64_t)h * 4; // 目安

        _owner->_textureCache.emplace(utf8Path, std::move(e));
        return effTex;
    }

    Effekseer::TextureRef Load(const void* data, int32_t size, Effekseer::TextureType /*textureType*/, bool isMipMapEnabled) override
    {
        // パス無しロードはキャッシュできないので都度生成
        if (!_owner || !_owner->_initialized) return nullptr;

        int w = 0, h = 0;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        if (!CreateSRVFromMemory(data, size, isMipMapEnabled, srv, w, h)) {
            return nullptr;
        }

        auto gd = _owner->_renderer->GetGraphicsDevice();
        Effekseer::Backend::TextureRef backendTex =
            EffekseerRendererDX11::CreateTexture(gd, srv.Get(), nullptr, nullptr);

        if (backendTex == nullptr) return nullptr;

        Effekseer::TextureRef effTex = Effekseer::MakeRefPtr<Effekseer::Texture>();
        effTex->SetBackend(backendTex);

        // srvの寿命はここで終わるが、backendTexが内部で参照保持する実装が通常
        // 不安ならここもキャッシュ構造に入れる設計にする
        return effTex;
    }

    void Unload(Effekseer::TextureRef /*data*/) override
    {
        // Effekseer側からのUnload呼びは基本は参照カウント任せ
        // キャッシュ解放はEffectManager側で行う
    }

private:
    bool CreateSRVFromMemory(const void* data, int32_t size, bool mipEnabled,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV,
        int& outW, int& outH)
    {
        if (!_owner || !_owner->_device || !data || size <= 0) return false;

        DirectX::ScratchImage scratch;
        DirectX::TexMetadata meta{};

        HRESULT hr = DirectX::LoadFromDDSMemory(data, (size_t)size, DirectX::DDS_FLAGS_NONE, &meta, scratch);
        if (FAILED(hr))
        {
            hr = DirectX::LoadFromWICMemory(data, (size_t)size, DirectX::WIC_FLAGS_IGNORE_SRGB, &meta, scratch);
            if (FAILED(hr)) return false;

            const DXGI_FORMAT targetFmt = DXGI_FORMAT_R8G8B8A8_UNORM;

            if (meta.format != targetFmt)
            {
                DirectX::ScratchImage converted;
                hr = DirectX::Convert(
                    scratch.GetImages(),
                    scratch.GetImageCount(),
                    scratch.GetMetadata(),
                    targetFmt,
                    DirectX::TEX_FILTER_DEFAULT,
                    DirectX::TEX_THRESHOLD_DEFAULT,
                    converted);

                if (FAILED(hr)) return false;

                scratch = std::move(converted);
                meta = scratch.GetMetadata();
            }

            if (DirectX::HasAlpha(meta.format))
            {
                DirectX::ScratchImage premultiplied;
                hr = DirectX::PremultiplyAlpha(
                    scratch.GetImages(),
                    scratch.GetImageCount(),
                    meta,
                    DirectX::TEX_PMALPHA_DEFAULT,
                    premultiplied);

                if (SUCCEEDED(hr))
                {
                    scratch = std::move(premultiplied);
                    meta = scratch.GetMetadata();
                }
            }
        }

        // ミップ生成（必要なら）
        if (mipEnabled)
        {
            if (meta.mipLevels <= 1)
            {
                DirectX::ScratchImage mipChain;
                hr = DirectX::GenerateMipMaps(
                    scratch.GetImages(),
                    scratch.GetImageCount(),
                    scratch.GetMetadata(),
                    DirectX::TEX_FILTER_DEFAULT,
                    0,
                    mipChain);

                if (SUCCEEDED(hr))
                {
                    scratch = std::move(mipChain);
                    meta = scratch.GetMetadata();
                }
            }
        }

        outW = (int)meta.width;
        outH = (int)meta.height;

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        hr = DirectX::CreateShaderResourceView(
            _owner->_device,
            scratch.GetImages(),
            scratch.GetImageCount(),
            meta,
            srv.GetAddressOf());

        if (FAILED(hr) || !srv) return false;

        outSRV = srv;
        return true;
    }

private:
    EffectManager* _owner = nullptr;
};

//============================================================
// シングルトン
//============================================================
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

//============================================================
// 初期化・終了
//============================================================
bool EffectManager::Init(ID3D11Device* device, ID3D11DeviceContext* context, int maxParticles)
{
    if (_initialized) return true;
    if (!device || !context) return false;

    _device = device;
    _context = context;
    _maxParticles = maxParticles;

    _manager = Effekseer::Manager::Create(maxParticles);
    if (_manager == nullptr) return false;

    // 既存コードと同じCreateでOK（内部でGraphicsDeviceも作られる）
    _renderer = EffekseerRendererDX11::Renderer::Create(device, context, maxParticles);
    if (_renderer == nullptr) return false;
	

    // ローダ設定（AssetManager経由）
    _manager->GetSetting()->SetEffectLoader(Effekseer::MakeRefPtr<EffectLoader>());
    _manager->GetSetting()->SetTextureLoader(Effekseer::MakeRefPtr<TextureLoader>(this));
    _manager->GetSetting()->SetModelLoader(Effekseer::MakeRefPtr<ModelLoader>());

    // レンダラ設定
    _manager->SetSpriteRenderer(_renderer->CreateSpriteRenderer());
    _manager->SetRibbonRenderer(_renderer->CreateRibbonRenderer());
    _manager->SetRingRenderer(_renderer->CreateRingRenderer());
    _manager->SetTrackRenderer(_renderer->CreateTrackRenderer());
    _manager->SetModelRenderer(_renderer->CreateModelRenderer());

    _manager->SetCoordinateSystem(Effekseer::CoordinateSystem::LH);

    _initialized = true;
    return true;
}

void EffectManager::UnInit()
{
    if (!_initialized) return;

    StopAllEffects();
    ClearAllEffects();

    _textureCache.clear();

    if (_manager != nullptr) _manager.Reset();
    if (_renderer != nullptr) _renderer.Reset();

    _device = nullptr;
    _context = nullptr;
    _initialized = false;
}

//============================================================
// エフェクト管理
//============================================================
Effekseer::EffectRef EffectManager::LoadEffect(const std::string& path)
{
    if (!_initialized) return nullptr;

    auto it = _effectCache.find(path);
    if (it != _effectCache.end()) return it->second;

    std::vector<uint8_t> data;
    if (!AssetManager::Instance()->LoadAsset(path, data)) {
        return nullptr;
    }

    std::string baseDir = path;
    auto pos = baseDir.find_last_of("/\\");
    if (pos != std::string::npos) baseDir = baseDir.substr(0, pos + 1);
    else baseDir.clear();

    std::u16string baseDir16 = Utf8ToUtf16(baseDir);

    Effekseer::EffectRef effect = Effekseer::Effect::Create(
        _manager,
        data.data(),
        (int32_t)data.size(),
        1.0f,
        baseDir16.empty() ? nullptr : baseDir16.c_str());

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

//============================================================
// 再生
//============================================================
Effekseer::Handle EffectManager::PlayEffect(const std::string& path, const DirectX::XMFLOAT3& position)
{
    if (!_initialized) return -1;

    auto effect = LoadEffect(path);
    if (effect == nullptr) return -1;

    return _manager->Play(effect, position.x, position.y, position.z);
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

//============================================================
// パラメータ
//============================================================
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

//============================================================
// カメラ
//============================================================
void EffectManager::SetCamera(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection)
{
    if (!_initialized) return;

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

//============================================================
// 更新・描画
//============================================================
void EffectManager::Update(float deltaTime)
{
    if (!_initialized) return;

    _manager->Update(deltaTime * 60.0f);
}

void EffectManager::Draw()
{
    if (!_initialized) return;

    ID3D11BlendState* prevBlendState = nullptr;
    FLOAT prevBlendFactor[4];
    UINT prevSampleMask;
    _context->OMGetBlendState(&prevBlendState, prevBlendFactor, &prevSampleMask);

    // **修正: Effekseer用のアルファブレンドを明示的に設定**
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;          // 修正
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;     // 修正
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;           // 修正
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;// 修正
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    ID3D11BlendState* effekseerBlend = nullptr;
    _device->CreateBlendState(&blendDesc, &effekseerBlend);

    FLOAT blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    _context->OMSetBlendState(effekseerBlend, blendFactor, 0xffffffff);

    // Effekseer描画
    _renderer->BeginRendering();
    _manager->Draw();
    _renderer->EndRendering();

    // ブレンドステートを復元
    _context->OMSetBlendState(prevBlendState, prevBlendFactor, prevSampleMask);

    // リソース解放
    if (effekseerBlend) effekseerBlend->Release();
    if (prevBlendState) prevBlendState->Release();
}

//============================================================
// デバッグ
//============================================================
int EffectManager::GetPlayingEffectsCount() const
{
    if (!_initialized) return 0;
    return _manager->GetRestInstancesCount();
}

void EffectManager::DrawDebugGUI()
{
    ImGui::TextUnformatted("EffectManager (EffekseerForCpp 1.7.2.0 / DX11)");
    ImGui::Separator();

    ImGui::Text("Initialized: %s", _initialized ? "Yes" : "No");
    ImGui::Text("Max Particles: %d", _maxParticles);
    ImGui::Text("Loaded Effects: %zu", _effectCache.size());
    ImGui::Text("Loaded Textures: %zu", _textureCache.size());

    uint64_t texBytes = 0;
    for (auto& kv : _textureCache) texBytes += kv.second.bytes;
    ImGui::Text("Texture Memory (approx): %.2f MB", (double)texBytes / (1024.0 * 1024.0));

    ImGui::Separator();
    if (ImGui::Button("Stop All Effects")) {
        StopAllEffects();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Effect Cache")) {
        ClearAllEffects();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Texture Cache")) {
        _textureCache.clear();
    }
}