#pragma once
#include "Component.h"
#include "ModelManager.h"
#include "ModelRender.h"
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>

// キーフレーム一体型チャネル
struct ACV2_Channel {
    int nodeIndex = -1;
    // 位置 / 回転(Quat) / スケール 各キー (time秒単位)
    std::vector<std::pair<float, DirectX::XMFLOAT3>> posKeys;
    std::vector<std::pair<float, DirectX::XMFLOAT4>> rotKeys;
    std::vector<std::pair<float, DirectX::XMFLOAT3>> sclKeys;
};

struct ACV2_Node {
    std::string name;
    int parentIndex = -1;
    DirectX::XMMATRIX bindLocal;
};

struct ACV2_Clip {
    std::string name;
    float duration = 0.f;       // 秒
    float ticksPerSecond = 25.f;
    std::vector<ACV2_Node> nodes;
    std::vector<ACV2_Channel> channels;
};

struct ACV2_Bone {
    std::string name;
    int parentIndex = -1;     // ボーン親（必要なら使用）
    int nodeIndex = -1;     // ノード階層への対応
    DirectX::XMMATRIX inverseBind; // aiBone::mOffsetMatrix
};

class AnimationComponentV2 : public Component {
public:
    enum class BlendState { None, Blending };

    void Init(Object* owner) override;
    void BeginPlay() override;
    void InGameUpdate() override;
    void DrawInspector() override;

    // 制御
    void Play(int clipIndex);
    void Stop();
    void Pause();
    void Resume();
    void Restart();
    void SetLoop(bool b) { m_loop = b; }
    void SetSpeed(float s) { m_speed = s; }

    // ブレンド開始
    void StartBlend(int targetClip, float duration);
    bool IsBlending() const { return m_blendState == BlendState::Blending; }

    // デバッグ
    void ValidateBindPose();
    void DumpBoneMap();
    void CheckPaletteValidity();

private:
    // 内部処理
    void BuildRuntimeFromResource();
    void UpdatePlayback(float dt);
    void BuildPose(float t, int clipIdx, std::vector<DirectX::XMFLOAT4X4>& outPalette);
    DirectX::XMMATRIX InterpChannel(const ACV2_Channel& ch, float t, float clipDuration) const;

    // 行列組み立て支援
    void ComputeGlobals(const ACV2_Clip& clip,
        const std::vector<DirectX::XMMATRIX>& locals,
        std::vector<DirectX::XMMATRIX>& globals) const;

    // VS へ転送
    void ApplyToRenderer();

private:
    ModelRenderComponent* m_renderer = nullptr;
    std::shared_ptr<ModelSharedResource> m_resource;

    std::vector<ACV2_Bone> m_bones;
    std::vector<ACV2_Clip> m_clips;

    int   m_currentClip = -1;
    float m_time = 0.f;
    float m_speed = 1.f;
    bool  m_loop = true;
    bool  m_paused = false;

    // ブレンド
    BlendState m_blendState = BlendState::None;
    int    m_blendTarget = -1;
    float  m_blendDuration = 0.3f;
    float  m_blendTimer = 0.f;
    std::vector<DirectX::XMFLOAT4X4> m_paletteSource;
    std::vector<DirectX::XMFLOAT4X4> m_paletteTarget;

    // 最終ボーンパレット
    std::vector<DirectX::XMFLOAT4X4> m_bonePalette;

    // Inspector デバッグトグル
    bool m_debugAutoBindCheck = false;
    bool m_debugTransposeForShader = false; // VS が mul(pos, M) の場合 ON
    bool m_debugShowBoneMapOnce = false;
    bool m_debugValidateBindPoseOnce = false;
    bool m_debugCheckPaletteValidityOnce = false;
};