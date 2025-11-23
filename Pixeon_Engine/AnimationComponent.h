#pragma once
#include "Component.h"
#include "ModelManager.h"
#include <DirectXMath.h>
#include <memory>

class ModelRenderComponent; // 前方宣言

class AnimationComponentRebuild : public Component {
public:
    void Init(Object* owner) override;
    void BeginPlay() override;
    void InGameUpdate() override;

    void Play(int clipIndex, bool loop = true);
    void PlayBlend(int newClip, float duration);
    void SetSpeed(float s) { m_speed = s; }
    void SetLoop(bool l) { m_loop = l; }

private:
    struct RuntimeChannel {
        int nodeIndex;
        std::map<float, AnimeTransform>* timeline;
    };
    struct RuntimeClip {
        std::string name;
        float duration;
        float tps;
        std::vector<RuntimeChannel> channels;
    };

    BoneInfo* GetBone(size_t i) {
        if (!m_resource || i >= m_resource->bones.size()) return nullptr;
        return &m_resource->bones[i];
    }

    AnimeTransform SampleChannel(const RuntimeChannel& ch, float t) const;
    DirectX::XMMATRIX BuildLocalMatrix(const AnimeTransform& tr) const;
    void BuildPoseSingle(int clipIndex, float time, std::vector<DirectX::XMFLOAT4X4>& outFinal);
    void BuildPoseBlended(int clipA, float timeA, int clipB, float timeB, float blend,
        std::vector<DirectX::XMFLOAT4X4>& outFinal);

    void ApplyToRenderer();

private:
    ModelRenderComponent* m_renderer = nullptr;
    std::shared_ptr<ModelSharedResourceNew> m_resource;
    std::vector<RuntimeClip> m_clips;

    int m_currentClip = -1;
    int m_blendTarget = -1;
    float m_blendTimer = 0;
    float m_blendDuration = 0;
    bool m_blending = false;

    bool m_loop = true;
    float m_speed = 1.0f;
    float m_time = 0.0f;

    std::vector<DirectX::XMFLOAT4X4> m_boneFinal; // 転置済み最終行列

};