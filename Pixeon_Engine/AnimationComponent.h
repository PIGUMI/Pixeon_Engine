#pragma once
#include "Component.h"
#include "ModelManager.h"
#include "ModelRender.h"
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <map>

// 単純なキー補間用構造体
struct BoneTransform {
    DirectX::XMFLOAT3 position{ 0,0,0 };
    DirectX::XMFLOAT4 rotation{ 0,0,0,1 };
    DirectX::XMFLOAT3 scale{ 1,1,1 };
    bool isValid = true;
};

// チャンネル（nodeIndex と 時刻→Transform のマップ）
struct AnimationChannelRuntime {
    int nodeIndex = -1;
    std::map<float, BoneTransform> timeline;
};

// クリップランタイム
struct AnimationClipRuntime {
    std::string name;
    double duration = 0.0;
    double tps = 25.0;
    std::vector<AnimationChannelRuntime> channels;
    bool isLoaded = false;
};

class AnimationComponent : public Component {
public:
    void Init(Object* owner) override;
    void BeginPlay() override;
	void EditUpdate() override;
    void InGameUpdate() override;
    void DrawInspector() override;

    // 再生制御
    void Play();
    void Pause();
    void Resume();
    void Stop();
    void Restart();

    bool SetAnimationClip(int clipIndex);
    void SetPlaybackSpeed(float s) { m_speed = s; }
    void SetLoop(bool b) { m_loop = b; }

    bool IsPlaying() const;
    bool IsPaused()  const;
    bool IsFinished() const;
    float GetAnimationProgress() const;

    void SaveToFile(std::ostream& out) override;
    void LoadFromFile(std::istream& in) override;

private:
    void UpdateAnimation(float dt);
    void RebuildBoneMatrices();
    void ApplyToModel();

    BoneTransform InterpChannel(const AnimationChannelRuntime& ch, float t) const;
    DirectX::XMMATRIX BuildMatrix(const BoneTransform& bt) const;

    // 最終ポーズ構築（公式式: final = InverseBindPose * CurrentGlobal）
    void BuildClipPose(int clipIndex, float time, std::vector<DirectX::XMFLOAT4X4>& outFinal);

    bool IsValidMatrix(const DirectX::XMMATRIX& m) const;

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

    std::vector<DirectX::XMFLOAT4X4> m_boneMatrices;

    std::unordered_map<std::string, int> m_nodeToBone;
};