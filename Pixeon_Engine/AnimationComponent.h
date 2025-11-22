#pragma once
#include "Component.h"
#include "ModelManager.h"
#include "ModelRender.h"
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <map>

// ボーン変換データ
struct BoneTransform {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 rotation; // クォータニオン
    DirectX::XMFLOAT3 scale;
    bool isValid = true;
};

// アニメーションキーフレーム
struct AnimationKeyframe {
    float time;
    BoneTransform transform;
};

// アニメーションチャンネル（拡張版）
struct AnimationChannelRuntime {
    int nodeIndex = -1;
    std::map<float, BoneTransform> timeline;
};

// アニメーションクリップランタイムデータ
struct AnimationClipRuntime {
    std::string name;
    double duration = 0.0;
    double ticksPerSecond = 25.0;
    std::vector<AnimationChannelRuntime> channels;
    bool isLoaded = false;
};

class AnimationComponent : public Component
{
public:
    // ブレンドモード
    enum class BlendMode {
        Instant,  // 即座に切り替え
        Fast,     // 高速ブレンド (0.1秒)
        Normal,   // 通常ブレンド (0.3秒)
        Smooth,   // 滑らかブレンド (0.5秒)
        Slow      // ゆっくりブレンド (1.0秒)
    };

    AnimationComponent() = default;
    ~AnimationComponent() = default;

    void Init(Object* owner) override;
    void BeginPlay() override;
    void InGameUpdate() override;
    void DrawInspector() override;

    // === 基本再生制御 ===
    void Play();
    void Pause();
    void Resume();
    void Stop();
    void Restart();

    // === アニメーション設定 ===
    bool SetAnimationClip(int clipIndex);
    int GetCurrentClipIndex() const { return m_currentClipIndex; }
    void SetPlaybackSpeed(float speed) { m_playbackSpeed = speed; }
    void SetLoop(bool loop) { m_loop = loop; }
    void SetBlendMode(BlendMode mode);

    // === 状態取得 ===
    bool IsPlaying() const;
    bool IsPaused() const;
    bool IsFinished() const;
    float GetAnimationProgress() const;
    float GetCurrentTime() const { return m_currentTime; }
    float GetDuration() const;

    // === ブレンド制御 ===
    void PlayBlend(int targetClipIndex, float blendDuration = 0.3f);
    bool IsBlending() const { return m_isBlending; }
    float GetBlendProgress() const;

    // === セーブ/ロード ===
    void SaveToFile(std::ostream& out) override;
    void LoadFromFile(std::istream& in) override;

private:
    // === 内部処理 ===
    void UpdateAnimation(float deltaTime);
    void UpdateBlending(float deltaTime);
    void CalculateBoneMatrices();
    void ApplyBoneMatricesToModel();

    // === 補間計算 ===
    BoneTransform InterpolateTransform(const AnimationChannelRuntime& channel, float time);
    DirectX::XMMATRIX BuildMatrixFromTransform(const BoneTransform& transform);
    DirectX::XMMATRIX CalculateLocalMatrix(int nodeIndex, float time);
    void CalculateWorldMatrices(int nodeIndex, const DirectX::XMMATRIX& parentWorld);

    // === 検証 ===
    bool IsValidMatrix(const DirectX::XMMATRIX& mat) const;
    bool IsValidBoneTransform(const BoneTransform& transform) const;

    // === モデル取得 ===
    ModelRenderComponent* GetModelRenderComponent();
    std::shared_ptr<ModelSharedResource> GetModelResource();
private:
    // === 既存の関数宣言... ===

    // === 補間用ヘルパー関数（追加） ===
    DirectX::XMFLOAT3 InterpolatePosition(const AnimationChannel& channel, float time);
    DirectX::XMFLOAT4 InterpolateRotation(const AnimationChannel& channel, float time);
    DirectX::XMFLOAT3 InterpolateScale(const AnimationChannel& channel, float time);

    // === ボーン行列計算（追加） ===
    void CalculateBoneMatricesForClip(int clipIndex, float time,
        std::vector<DirectX::XMFLOAT4X4>& outMatrices);
    void CalculateWorldMatricesRecursive(int clipIndex, int nodeIndex,
        const DirectX::XMMATRIX& parentWorld,
        const std::vector<DirectX::XMMATRIX>& localMatrices,
        std::vector<DirectX::XMFLOAT4X4>& outMatrices);

private:
    // === モデル参照 ===
    ModelRenderComponent* m_modelRender = nullptr;
    std::shared_ptr<ModelSharedResource> m_modelResource;

    // === アニメーションデータ ===
    std::vector<AnimationClipRuntime> m_clips;
    int m_currentClipIndex = -1;

    // === 再生状態 ===
    bool m_isPlaying = false;
    bool m_isPaused = false;
    bool m_loop = true;
    float m_playbackSpeed = 1.0f;
    float m_currentTime = 0.0f;

    // === ブレンド状態 ===
    bool m_isBlending = false;
    int m_blendTargetIndex = -1;
    float m_blendTimer = 0.0f;
    float m_blendDuration = 0.3f;
    BlendMode m_blendMode = BlendMode::Normal;

    // === ボーン行列キャッシュ ===
    std::vector<DirectX::XMFLOAT4X4> m_boneMatrices;
    std::vector<DirectX::XMFLOAT4X4> m_blendSourceMatrices;
    std::vector<DirectX::XMFLOAT4X4> m_blendTargetMatrices;

    // === デバッグ ===
    bool m_debugMode = false;
    std::string m_lastError;
};