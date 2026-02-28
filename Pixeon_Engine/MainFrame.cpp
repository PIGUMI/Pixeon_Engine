/*
* MainFrame.cpp
*/

#include "MainFrame.h"
#include "System.h"
#include "GameRenderTarget.h"
#include "GBuffer.h"
#include "LightingPass.h"
#include "GUI.h"
#include "AssetManager.h"
#include "ModelManager.h"
#include "TextureManager.h"
#include "SettingManager.h"
#include "ShaderManager.h"
#include "SoundManager.h"
#include "EffectManager.h"
#include "ComponentManager.h"
#include "ScriptManager.h"
#include "ResourceService.h"
#include "Animator2DManager.h"
#include "SceneManger.h"
#include "Input.h"
#include "EngineFrame.h"
#include "Animator2DFrame.h"
#include "ImageUtils.h"
#include "LayerSettings.h"
#include "Scene.h"
#include "IZANAGI.h"
#include <crtdbg.h>

MainFrame* MainFrame::instance_ = nullptr;

MainFrame* MainFrame::GetInstance() {
    if (instance_ == nullptr) instance_ = new MainFrame();
    return instance_;
}
void MainFrame::DeleteInstance() {
    if (instance_ != nullptr) { delete instance_; instance_ = nullptr; }
}

int MainFrame::Init(const EngineConfig& InPut)
{
    _targetFrameTime = 1000.0f / 1000.0f;
    _lastUpdateTime = timeGetTime();
    _wnd = InPut.wnd;
    _updateDraw = false;
    _engineConfig = InPut;

    if (!IZANAGI::GetInstance()->Initialize("SceneRoot/Tool/IZANAGI/qwen2.5-7b-instruct-q3_k_m.gguf", 8192, 4, 2))
        MessageBox(nullptr, "IZANAGIエンジンの初期化に失敗しました。", "エラー", MB_OK | MB_ICONERROR);

    SettingManager::GetInstance()->LoadConfig();
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) return -1;

    hr = DirectX11::GetInstance()->Init(InPut.wnd, InPut.screenWidth, InPut.screenHeight, InPut.fullscreen);
    if (FAILED(hr)) { CoUninitialize(); return -1; }

    DirectX11::GetInstance()->InitializeHDRPipeline(InPut.screenWidth, InPut.screenHeight);
    AssetManager::Instance()->SetRoot(SettingManager::GetInstance()->GetAssetsFilePath());
    AssetManager::Instance()->SetLoadMode(AssetManager::LoadMode::FromSource);
    AssetManager::Instance()->StartAutoSync(std::chrono::milliseconds(1000), true);
    EffectManager::Instance()->Init(DirectX11::GetInstance()->GetDevice(), DirectX11::GetInstance()->GetContext());

    for (int layer = 0; layer < MAX_LAYER_COUNT; layer++)
    {
        GameRenderTarget* layerRT = new GameRenderTarget();
        layerRT->InitWithDepthSRV(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);
        layerRT->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());
        _layerRenderTargets.push_back(layerRT);

        GBuffer* gb = new GBuffer();
        if (!gb->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight))
        {
            delete gb; gb = nullptr;
        }
        _gBuffers.push_back(gb);

        LightingPass* lp = new LightingPass();
        if (!lp->Init(DirectX11::GetInstance()->GetDevice()))
        {
            delete lp; lp = nullptr;
        }
        _lightingPasses.push_back(lp);
    }

    _finalRenderTarget = new GameRenderTarget();
    _finalRenderTarget->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);
    _finalRenderTarget->SetRenderZBuffer(false);

    GUI::GetInstance()->Init();
    ShaderManager::GetInstance()->Initialize(DirectX11::GetInstance()->GetDevice());
    ComponentManager::GetInstance()->Init();
    ScriptManager::Instance().RegisterAllScripts();
    InitInput();
    EngineFrame::GetInstance()->Init();
    Animator2DFrame::GetInstance()->Init();
    return 0;
}

void MainFrame::Update()
{
    DWORD current_Time = timeGetTime();
    float delta_Time = static_cast<float>(current_Time - _lastUpdateTime);

    if (delta_Time >= _targetFrameTime) {
        _deltaTime = delta_Time * 0.001f;
        UpdateInput(GetWindowHandle());

        switch (_softwareMode)
        {
        case SoftWareMode::ENGINE:
            EngineFrame::GetInstance()->Update();
            if (auto scene = SceneManger::GetInstance()->GetCurrentScene())
                scene->UpdateEffects(_deltaTime);
            break;
        case SoftWareMode::ANIMTOR2D:
            Animator2DFrame::GetInstance()->Update();
            break;
        }
        SetMouseFreeze(_fixedMouseCursorFlag);
        _lastUpdateTime = current_Time;
        _updateDraw = true;
    }
}

// ============================================================
// DrawGeometryPass
// ============================================================
void MainFrame::DrawGeometryPass(int layerIndex, GBuffer* gbuffer)
{
    if (!gbuffer) return;
    ID3D11DeviceContext* ctx = DirectX11::GetInstance()->GetContext();

    gbuffer->BeginGeometryPass(ctx);

    if (_softwareMode == SoftWareMode::ENGINE)
    {
        if (auto scene = SceneManger::GetInstance()->GetCurrentScene())
            scene->DrawForGBuffer(layerIndex);
    }

    gbuffer->EndGeometryPass(ctx);
}

GBuffer* MainFrame::GetGBuffer(int layerIndex) const
{
    if (layerIndex < 0 || layerIndex >= (int)_gBuffers.size()) return nullptr;
    return _gBuffers[layerIndex];
}

// ============================================================
// Draw
// ============================================================
void MainFrame::Draw()
{
    if (!_updateDraw) return;

    auto* dx11 = DirectX11::GetInstance();
    ID3D11DeviceContext* ctx = dx11->GetContext();

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ctx->ClearRenderTargetView(dx11->GetHDRRTV(), clearColor);
    ctx->ClearDepthStencilView(dx11->GetDefaultDSV()->GetView(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    int  Layer_Index = 0;
    auto layerRTIt = _layerRenderTargets.begin();
    int  lpIdx = 0;

    for (; layerRTIt != _layerRenderTargets.end() && Layer_Index < MAX_LAYER_COUNT;
        ++layerRTIt, ++Layer_Index, ++lpIdx)
    {
        auto layerRT = *layerRTIt;

        Layer* layerSettings = nullptr;
        if (_softwareMode == SoftWareMode::ENGINE)
            if (auto scene = SceneManger::GetInstance()->GetCurrentScene())
                layerSettings = scene->GetLayer(Layer_Index);

        if (layerSettings && !layerSettings->visible) continue;

        if (_softwareMode == SoftWareMode::ENGINE)
        {
            auto scene = SceneManger::GetInstance()->GetCurrentScene();
            if (scene)
            {
                GBuffer* gb = GetGBuffer(Layer_Index);
                LightingPass* lp = (lpIdx < (int)_lightingPasses.size()) ? _lightingPasses[lpIdx] : nullptr;

                scene->PrepareShadowAndLights(Layer_Index);

                DrawGeometryPass(Layer_Index, gb);

                if (lp && gb)
                {
                    auto cam = scene->GetMainCamera();

                    layerRT->SetBlend(true);
                    layerRT->SetRenderZBuffer(false);
                    layerRT->Begin(ctx);

                    if (cam)
                    {
                        lp->Execute(
                            ctx,
                            gb,
                            layerRT->GetRenderTargetView(),
                            scene->GetShadowMapSRV(),
                            _engineConfig.screenWidth,
                            _engineConfig.screenHeight,
                            cam->GetProjection(),
                            cam->GetView(),
                            cam->GetWorldPosition()
                        );
                    }

                    layerRT->SetBlend(true);
                    ID3D11RenderTargetView* layerRTV = layerRT->GetRenderTargetView();
                    ctx->OMSetRenderTargets(1, &layerRTV, gb->GetDSV());
                    ctx->OMSetDepthStencilState(nullptr, 0);
                    dx11->SetBlendMode(BLEND_ALPHA);
                    if (cam)
                        scene->DrawEffects(Layer_Index, cam);

                    layerRT->End();
                }
            }
        }
        else if (_softwareMode == SoftWareMode::ANIMTOR2D)
        {
            layerRT->SetBlend(true);
            layerRT->SetRenderZBuffer(false);
            layerRT->Begin(ctx);
            if (Layer_Index == 0) Animator2DFrame::GetInstance()->Draw();
            layerRT->End();
        }
    }

    _finalRenderTarget->SetRenderZBuffer(false);
    _finalRenderTarget->Begin(ctx);
    dx11->SetBlendMode(BLEND_ALPHA);
    dx11->ApplyToneMappingPass();

    Layer_Index = 0;
    layerRTIt = _layerRenderTargets.begin();

    for (; layerRTIt != _layerRenderTargets.end() && Layer_Index < MAX_LAYER_COUNT;
        ++layerRTIt, ++Layer_Index)
    {
        auto layerRT = *layerRTIt;

        Layer* layerSettings = nullptr;
        if (_softwareMode == SoftWareMode::ENGINE)
            if (auto scene = SceneManger::GetInstance()->GetCurrentScene())
                layerSettings = scene->GetLayer(Layer_Index);

        if (layerSettings && !layerSettings->visible) continue;

        float opacity = layerSettings ? layerSettings->opacity : 1.0f;

        if (layerSettings && !layerSettings->postEffects.empty())
        {
            ID3D11ShaderResourceView* depthSRV = layerRT->GetDepthShaderResourceView();
            DirectX::XMMATRIX proj = DirectX::XMMatrixIdentity();
            DirectX::XMMATRIX invProj = DirectX::XMMatrixIdentity();
            if (_softwareMode == SoftWareMode::ENGINE)
                if (auto scene = SceneManger::GetInstance()->GetCurrentScene())
                    if (auto cam = scene->GetMainCamera())
                    {
                        proj = cam->GetProjection();
                        invProj = DirectX::XMMatrixInverse(nullptr, proj);
                    }
            layerSettings->ApplyPostEffectsToScreen(
                layerRT->GetShaderResourceView(),
                _engineConfig.screenWidth, _engineConfig.screenHeight,
                opacity, depthSRV, &proj, &invProj);
        }
        else
        {
            ImageUtils::DrawSRV(
                layerRT->GetShaderResourceView(),
                0.0f, 0.0f,
                (float)_engineConfig.screenWidth, (float)_engineConfig.screenHeight,
                DirectX::XMFLOAT4(1, 1, 1, 1),
                DirectX::XMFLOAT4(0, 0, 1, 1),
                true, opacity);
        }
    }

    _finalRenderTarget->End();

    dx11->BeginDraw();
    GUI::GetInstance()->BeginDraw();
    switch (_softwareMode)
    {
    case SoftWareMode::ENGINE:    EngineFrame::GetInstance()->DrawGUI(); break;
    case SoftWareMode::ANIMTOR2D: Animator2DFrame::GetInstance()->DrawGUI(); break;
    }
    GUI::GetInstance()->EndDraw();
    dx11->EndDraw();

    _updateDraw = false;
}

void MainFrame::UnInit()
{
    for (auto lp : _lightingPasses)
    {
        if (lp) { lp->Release(); delete lp; }
    }
    _lightingPasses.clear();

    for (auto gb : _gBuffers)
    {
        if (gb) { gb->Release(); delete gb; }
    }
    _gBuffers.clear();

    for (auto layerRT : _layerRenderTargets) delete layerRT;
    _layerRenderTargets.clear();

    if (_finalRenderTarget) { delete _finalRenderTarget; _finalRenderTarget = nullptr; }

    Animator2DFrame::GetInstance()->UnInit();
    Animator2DFrame::DestroyInstance();
    EngineFrame::GetInstance()->UnInit();
    EngineFrame::DestroyInstance();
    UninitInput();
    AssetManager::Instance()->StopAutoSync();
    SettingManager::GetInstance()->SaveConfig();
    GUI::DestroyInstance();
    ComponentManager::DestroyInstance();
    SettingManager::DestroyInstance();
    ScriptManager::Release();
    ShaderManager::DestroyInstance();
    Animator2DManager::DestroyInstance();
    AssetManager::DeleteInstance();
    ModelManager::DeleteInstance();
    TextureManager::DeleteInstance();
    SoundManager::DeleteInstance();
    ResourceService::DeleteInstance();
    DirectX11::DestroyInstance();
    IZANAGI::GetInstance()->Shutdown();
    IZANAGI::DestroyInstance();
    CoUninitialize();
}

ID3D11ShaderResourceView* MainFrame::GetFinalRenderTargetSRV()
{
    if (_finalRenderTarget) return _finalRenderTarget->GetShaderResourceView();
    return nullptr;
}