#pragma once
#include "Component.h"
#include "ModelManager.h"
#include "ModelRender.h"
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <map>

struct BoneTransform {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 rotation;
    DirectX::XMFLOAT3 scale;
    bool isValid = true;
};

struct AnimationChannelRuntime {
    int nodeIndex = -1;
    std::map<float, BoneTransform> timeline;
};

struct AnimationClipRuntime {
    std::string name;
    double duration = 0.0;
    double tps = 25.0;
    std::vector<AnimationChannelRuntime> channels;
    bool isLoaded = false;
};

class AnimationComponent : public Component {
public:
    enum class BlendMode { Instant, Fast, Normal, Smooth, Slow };

    void Init(Object* owner) override;
    void BeginPlay() override;
    void InGameUpdate() override;
    void DrawInspector() override;

    void Play();
    void Pause();
    void Resume();
    void Stop();
    void Restart();

    bool SetAnimationClip(int clipIndex);
    void SetPlaybackSpeed(float s) { m_speed = s; }
    void SetLoop(bool b) { m_loop = b; }
    void SetBlendMode(BlendMode m);

    bool IsPlaying() const;
    bool IsPaused()  const;
    bool IsFinished() const;
    float GetAnimationProgress() const;

    void PlayBlend(int targetClip, float duration = 0.3f);

    void SaveToFile(std::ostream& out) override;
    void LoadFromFile(std::istream& in) override;

private:
    void UpdateAnimation(float dt);
    void UpdateBlend(float dt);
    void RebuildBoneMatrices();
    void ApplyToModel();

    BoneTransform InterpChannel(const AnimationChannelRuntime& ch, float t) const;
    DirectX::XMMATRIX BuildMatrix(const BoneTransform& bt) const;

    void BuildClipPose(int clipIndex, float time,
        std::vector<DirectX::XMFLOAT4X4>& outFinal);

    bool IsValidMatrix(const DirectX::XMMATRIX& m) const;

    // ブレンド用: 行列分解
    DirectX::XMMATRIX BlendBoneMatrix(const DirectX::XMMATRIX& A,
        const DirectX::XMMATRIX& B,
        float f) const;

    ModelRenderComponent* GetRenderer();
    std::shared_ptr<ModelSharedResource> GetResource();

private:
    ModelRenderComponent* m_renderer = nullptr;
    std::shared_ptr<ModelSharedResource> m_resource;

    std::vector<AnimationClipRuntime> m_clips;
    int   m_currentClip = -1;
    bool  m_playing = false;
    bool  m_paused = false;
    bool  m_loop = true;
    float m_speed = 1.0f;
    float m_time = 0.0f;

    // ブレンド
    bool  m_blending = false;
    int   m_blendTarget = -1;
    float m_blendTimer = 0.0f;
    float m_blendDuration = 0.3f;
    BlendMode m_blendMode = BlendMode::Normal;

    std::vector<DirectX::XMFLOAT4X4> m_boneMatrices;
    std::vector<DirectX::XMFLOAT4X4> m_sourceBlend;
    std::vector<DirectX::XMFLOAT4X4> m_targetBlend;

    // 高速化: nodeName→boneIndex
    std::unordered_map<std::string, int> m_nodeToBone;
};