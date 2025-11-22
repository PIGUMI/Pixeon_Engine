#define NOMINMAX
#include "AnimationComponent.h"
#include "ModelRender.h"
#include "ErrorLog.h"
#include "System.h"
#include "IMGUI/imgui.h"
#include <algorithm>
#include <set>

void AnimationComponent::Init(Object* owner) {
    _Parent = owner;
    _ComponentName = "Animation";
    _Type = ComponentManager::COMPONENT_TYPE::ANIMATION;
}

void AnimationComponent::BeginPlay() {
    // ModelRenderComponentを取得
    m_modelRender = GetModelRenderComponent();
    if (!m_modelRender) {
        ErrorLogger::Instance().LogError("AnimationComponent",
            "ModelRenderComponent not found on " + _Parent->GetObjectName());
        return;
    }

    // モデルリソースを取得
    m_modelResource = GetModelResource();
    if (!m_modelResource) {
        ErrorLogger::Instance().LogError("AnimationComponent",
            "ModelSharedResource not found");
        return;
    }

    // アニメーションクリップをロード
    m_clips.clear();
    for (auto& clip : m_modelResource->clips) {
        AnimationClipRuntime runtime;
        runtime.name = clip.name;
        runtime.duration = clip.duration / clip.tps; // 秒単位に変換
        runtime.ticksPerSecond = clip.tps;
        runtime.isLoaded = true;

        // チャンネルをランタイム形式に変換
        for (auto& channel : clip.channels) {
            AnimationChannelRuntime runtimeChannel;
            runtimeChannel.nodeIndex = channel.nodeIndex;

            // キーフレームをタイムライン形式に統合
            std::set<float> allTimes;
            for (auto& key : channel.positionKeys) allTimes.insert(key.first);
            for (auto& key : channel.rotationKeys) allTimes.insert(key.first);
            for (auto& key : channel.scaleKeys) allTimes.insert(key.first);

            for (float time : allTimes) {
                BoneTransform transform;
                transform.position = InterpolatePosition(channel, time);
                transform.rotation = InterpolateRotation(channel, time);
                transform.scale = InterpolateScale(channel, time);
                transform.isValid = true;

                runtimeChannel.timeline[time] = transform;
            }

            runtime.channels.push_back(runtimeChannel);
        }

        m_clips.push_back(runtime);
    }

    // ボーン行列の初期化
    if (m_modelResource->bones.size() > 0) {
        m_boneMatrices.resize(m_modelResource->bones.size());
        m_blendSourceMatrices.resize(m_modelResource->bones.size());
        m_blendTargetMatrices.resize(m_modelResource->bones.size());

        for (auto& mat : m_boneMatrices) {
            DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixIdentity());
        }
    }

    EditrGUI::GetInstance()->WriteLog("=== Animation Debug Info ===");

    // ボーン情報
    EditrGUI::GetInstance()->WriteLog("Model bones: " + std::to_string(m_modelResource->bones.size()));
    for (size_t i = 0; i < std::min(size_t(10), m_modelResource->bones.size()); ++i) {
        EditrGUI::GetInstance()->WriteLog("  Bone[" + std::to_string(i) + "]: " + m_modelResource->bones[i].name);
    }

    // アニメーションクリップ情報
    EditrGUI::GetInstance()->WriteLog("Animation clips: " + std::to_string(m_clips.size()));

    if (!m_clips.empty() && !m_modelResource->clips.empty()) {
        auto& clip = m_clips[0];
        auto& sourceClip = m_modelResource->clips[0];

        EditrGUI::GetInstance()->WriteLog("Clip[0]: " + clip.name +
            " duration=" + std::to_string(clip.duration) +
            " channels=" + std::to_string(clip.channels.size()));

        // ノード階層の情報
        EditrGUI::GetInstance()->WriteLog("Node hierarchy: " + std::to_string(sourceClip.nodeHierarchy.size()) + " nodes");

        // 最初の10ノードを出力
        for (size_t i = 0; i < std::min(size_t(10), sourceClip.nodeHierarchy.size()); ++i) {
            auto& node = sourceClip.nodeHierarchy[i];
            EditrGUI::GetInstance()->WriteLog("  Node[" + std::to_string(i) + "]: " + node.name +
                " parent=" + std::to_string(node.parentIndex) +
                " children=" + std::to_string(node.children.size()));
        }

        // チャンネルとノードのマッピング確認
        EditrGUI::GetInstance()->WriteLog("Channel mapping:");
        for (size_t i = 0; i < std::min(size_t(10), clip.channels.size()); ++i) {
            auto& ch = clip.channels[i];
            std::string nodeName = (ch.nodeIndex >= 0 && ch.nodeIndex < (int)sourceClip.nodeHierarchy.size())
                ? sourceClip.nodeHierarchy[ch.nodeIndex].name
                : "INVALID";
            EditrGUI::GetInstance()->WriteLog("  Channel[" + std::to_string(i) + "]: NodeIdx=" +
                std::to_string(ch.nodeIndex) + " (" + nodeName + ") Keys=" +
                std::to_string(ch.timeline.size()));
        }

        EditrGUI::GetInstance()->WriteLog("Bone-Node matching:");
        for (size_t i = 0; i < std::min(size_t(10), m_modelResource->bones.size()); ++i) {
            std::string boneName = m_modelResource->bones[i].name;
            bool foundNode = false;
            int nodeIdx = -1;

            for (size_t j = 0; j < sourceClip.nodeHierarchy.size(); ++j) {
                std::string nodeName = sourceClip.nodeHierarchy[j].name;

                // 小文字に変換して比較
                std::string boneLower = boneName;
                std::string nodeLower = nodeName;
                std::transform(boneLower.begin(), boneLower.end(), boneLower.begin(), ::tolower);
                std::transform(nodeLower.begin(), nodeLower.end(), nodeLower.begin(), ::tolower);

                if (boneLower == nodeLower) {
                    foundNode = true;
                    nodeIdx = (int)j;
                    break;
                }
            }

            if (foundNode) {
                EditrGUI::GetInstance()->WriteLog("  Bone[" + std::to_string(i) + "]: " + boneName +
                    " -> Node[" + std::to_string(nodeIdx) + "] MATCHED");
            }
            else {
                EditrGUI::GetInstance()->WriteLog("  Bone[" + std::to_string(i) + "]: " + boneName +
                    " -> NO MATCH FOUND!");
            }
        }
    }

    EditrGUI::GetInstance()->WriteLog("Bone matrices allocated: " + std::to_string(m_boneMatrices.size()));
    EditrGUI::GetInstance()->WriteLog("=== End Debug Info ===");

    if (m_debugMode) {
        ErrorLogger::Instance().LogError("AnimationComponent",
            "Loaded " + std::to_string(m_clips.size()) + " animation clips", false, 5);
    }
}

void AnimationComponent::InGameUpdate() {
    if (!m_isPlaying || m_isPaused) return;
    if (m_currentClipIndex < 0 || m_currentClipIndex >= (int)m_clips.size()) return;

    float deltaTime = 0.017f;

    if (m_isBlending) {
        UpdateBlending(deltaTime);
    }
    else {
        UpdateAnimation(deltaTime);
    }

    CalculateBoneMatrices();

   
    static float totalTime = 0.0f;
    totalTime += deltaTime;
    if (totalTime < 5.0f) {
        static int frameCount = 0;
        if (++frameCount % 30 == 0) { // 30フレームごと（約0.5秒）
            EditrGUI::GetInstance()->WriteLog("[Animation] Time: " +
                std::to_string(m_currentTime) + " / " +
                std::to_string(m_clips[m_currentClipIndex].duration));

            for (int i = 0; i < std::min(3, (int)m_boneMatrices.size()); ++i) {
                DirectX::XMFLOAT4X4& mat = m_boneMatrices[i];

                // 移動成分を直接取得
                EditrGUI::GetInstance()->WriteLog("  Bone[" + std::to_string(i) +
                    "] pos: (" + std::to_string(mat._41) + ", " +
                    std::to_string(mat._42) + ", " + std::to_string(mat._43) + ")" +
                    " | m11=" + std::to_string(mat._11) +
                    " m22=" + std::to_string(mat._22) +
                    " m33=" + std::to_string(mat._33));
            }
        }
    }

    ApplyBoneMatricesToModel();
}

void AnimationComponent::UpdateAnimation(float deltaTime) {
    auto& clip = m_clips[m_currentClipIndex];

    m_currentTime += deltaTime * m_playbackSpeed;

    // ループ処理
    if (m_currentTime >= clip.duration) {
        if (m_loop) {
            m_currentTime = fmod(m_currentTime, clip.duration);
        }
        else {
            m_currentTime = clip.duration;
            m_isPlaying = false;
        }
    }
}

void AnimationComponent::UpdateBlending(float deltaTime) {
    m_blendTimer += deltaTime;

    float blendProgress = m_blendTimer / m_blendDuration;

    if (blendProgress >= 1.0f) {
        // ブレンド完了
        m_isBlending = false;
        m_currentClipIndex = m_blendTargetIndex;
        m_blendTimer = 0.0f;
        m_currentTime = 0.0f;

        if (m_debugMode) {
            ErrorLogger::Instance().LogError("AnimationComponent",
                "Blend completed to clip " + std::to_string(m_currentClipIndex), false, 5);
        }
    }
    else {
        // ターゲットアニメーションの時間を進める
        auto& targetClip = m_clips[m_blendTargetIndex];
        m_currentTime += deltaTime * m_playbackSpeed;
        if (m_currentTime >= targetClip.duration) {
            if (m_loop) {
                m_currentTime = fmod(m_currentTime, targetClip.duration);
            }
            else {
                m_currentTime = targetClip.duration;
            }
        }
    }
}

void AnimationComponent::CalculateBoneMatrices() {
    if (m_boneMatrices.empty()) return;

    if (m_isBlending) {
        // ブレンド中：2つのアニメーションを補間
        CalculateBoneMatricesForClip(m_currentClipIndex, m_currentTime, m_blendSourceMatrices);
        CalculateBoneMatricesForClip(m_blendTargetIndex, m_currentTime, m_blendTargetMatrices);

        float blendFactor = m_blendTimer / m_blendDuration;
        // スムーズステップ補間
        blendFactor = blendFactor * blendFactor * (3.0f - 2.0f * blendFactor);

        for (size_t i = 0; i < m_boneMatrices.size(); ++i) {
            DirectX::XMMATRIX src = DirectX::XMLoadFloat4x4(&m_blendSourceMatrices[i]);
            DirectX::XMMATRIX dst = DirectX::XMLoadFloat4x4(&m_blendTargetMatrices[i]);

            // 行列の線形補間（簡易版）
            DirectX::XMMATRIX blended;
            blended.r[0] = DirectX::XMVectorLerp(src.r[0], dst.r[0], blendFactor);
            blended.r[1] = DirectX::XMVectorLerp(src.r[1], dst.r[1], blendFactor);
            blended.r[2] = DirectX::XMVectorLerp(src.r[2], dst.r[2], blendFactor);
            blended.r[3] = DirectX::XMVectorLerp(src.r[3], dst.r[3], blendFactor);

            if (IsValidMatrix(blended)) {
                DirectX::XMStoreFloat4x4(&m_boneMatrices[i], blended);
            }
        }
    }
    else {
        // 通常再生
        CalculateBoneMatricesForClip(m_currentClipIndex, m_currentTime, m_boneMatrices);
    }
}

void AnimationComponent::CalculateBoneMatricesForClip(int clipIndex, float time,
    std::vector<DirectX::XMFLOAT4X4>& outMatrices) {
    if (clipIndex < 0 || clipIndex >= (int)m_clips.size()) return;
    if (outMatrices.empty()) return;
    if (clipIndex >= (int)m_modelResource->clips.size()) return;

    auto& clip = m_clips[clipIndex];
    auto& sourceClip = m_modelResource->clips[clipIndex];

    for (size_t i = 0; i < outMatrices.size(); ++i) {
        DirectX::XMStoreFloat4x4(&outMatrices[i], DirectX::XMMatrixIdentity());
    }

    // ノード数に合わせてローカル行列を確保
    size_t nodeCount = sourceClip.nodeHierarchy.size();
    std::vector<DirectX::XMMATRIX> localMatrices(nodeCount, DirectX::XMMatrixIdentity());

    // ===⭐ 重要: 全てのノードの初期変換を設定 ===
    for (size_t i = 0; i < nodeCount; ++i) {
        localMatrices[i] = sourceClip.nodeHierarchy[i].localTransform;
    }

    for (auto& channel : clip.channels) {
        if (channel.nodeIndex < 0 || channel.nodeIndex >= (int)nodeCount) continue;

        BoneTransform transform = InterpolateTransform(channel, time);
        localMatrices[channel.nodeIndex] = BuildMatrixFromTransform(transform);
    }

    // ノード階層に基づいてワールド行列を計算
    if (!sourceClip.nodeHierarchy.empty()) {
        // ===⭐ 修正: RootNodeから開始（インデックス0）
        CalculateWorldMatricesRecursive(clipIndex, 0, DirectX::XMMatrixIdentity(),
            localMatrices, outMatrices);
    }

    static bool firstCheck = true;
    if (firstCheck) {
        firstCheck = false;
        for (size_t i = 0; i < outMatrices.size(); ++i) {
            DirectX::XMMATRIX mat = DirectX::XMLoadFloat4x4(&outMatrices[i]);

            // 単位行列のままか確認
            DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();
            bool isIdentity = true;
            for (int r = 0; r < 4; r++) {
                for (int c = 0; c < 4; c++) {
                    float m = DirectX::XMVectorGetByIndex(mat.r[r], c);
                    float id = DirectX::XMVectorGetByIndex(identity.r[r], c);
                    if (std::abs(m - id) > 0.0001f) {
                        isIdentity = false;
                        break;
                    }
                }
                if (!isIdentity) break;
            }

            if (isIdentity && i < 20) {
                EditrGUI::GetInstance()->WriteLog("[WARNING] Bone[" + std::to_string(i) + "]=" +
                    m_modelResource->bones[i].name + " is still IDENTITY (no animation data)");
            }
        }
    }
}

void AnimationComponent::CalculateWorldMatricesRecursive(
    int clipIndex, int nodeIndex, const DirectX::XMMATRIX& parentWorld,
    const std::vector<DirectX::XMMATRIX>& localMatrices,
    std::vector<DirectX::XMFLOAT4X4>& outMatrices)
{
    if (clipIndex >= (int)m_modelResource->clips.size()) return;
    auto& nodeHierarchy = m_modelResource->clips[clipIndex].nodeHierarchy;
    if (nodeIndex >= (int)nodeHierarchy.size()) return;

    auto& node = nodeHierarchy[nodeIndex];

    DirectX::XMMATRIX localMat = (nodeIndex < (int)localMatrices.size())
        ? localMatrices[nodeIndex]
        : DirectX::XMMatrixIdentity();

    // ワールド行列 = ローカル行列 × 親のワールド行列
    DirectX::XMMATRIX worldMat = localMat * parentWorld;

    std::string nodeName = node.name;
    std::transform(nodeName.begin(), nodeName.end(), nodeName.begin(), ::tolower);

    for (size_t i = 0; i < m_modelResource->bones.size(); ++i) {
        std::string boneName = m_modelResource->bones[i].name;
        std::transform(boneName.begin(), boneName.end(),
            boneName.begin(), ::tolower);

        if (boneName == nodeName) {

            DirectX::XMMATRIX finalMat =
                m_modelResource->bones[i].invOffset * worldMat;

            if (IsValidMatrix(finalMat)) {
                DirectX::XMStoreFloat4x4(&outMatrices[i], finalMat);
            }

            // デバッグ出力
            static int debugFrameCount = 0;
            static bool firstTime = true;
            if (firstTime && i < 5 && debugFrameCount < 10) {
                DirectX::XMFLOAT4X4 debugMat;
                DirectX::XMStoreFloat4x4(&debugMat, finalMat);

                EditrGUI::GetInstance()->WriteLog(
                    "[AnimComp] Bone[" + std::to_string(i) + "]=" + boneName +
                    " finalMat pos: (" +
                    std::to_string(debugMat._41) + ", " +
                    std::to_string(debugMat._42) + ", " +
                    std::to_string(debugMat._43) + ")"
                );
            }
            debugFrameCount++;
            if (debugFrameCount > 60) firstTime = false;

            break;
        }
    }

    // 子ノードを再帰的に処理
    for (int childIndex : node.children) {
        CalculateWorldMatricesRecursive(clipIndex, childIndex, worldMat,
            localMatrices, outMatrices);
    }
}

BoneTransform AnimationComponent::InterpolateTransform(
    const AnimationChannelRuntime& channel, float time)
{
    if (channel.timeline.empty()) {
        BoneTransform identity;
        identity.position = { 0, 0, 0 };
        identity.rotation = { 0, 0, 0, 1 };
        identity.scale = { 1, 1, 1 };
        return identity;
    }

    // タイムラインから補間
    auto it = channel.timeline.lower_bound(time);

    if (it == channel.timeline.begin()) {
        return it->second;
    }
    if (it == channel.timeline.end()) {
        return channel.timeline.rbegin()->second;
    }

    auto nextIt = it;
    auto prevIt = --it;

    float t0 = prevIt->first;
    float t1 = nextIt->first;
    float factor = (time - t0) / (t1 - t0);

    BoneTransform result;
    const BoneTransform& a = prevIt->second;
    const BoneTransform& b = nextIt->second;

    // Position: 線形補間
    result.position.x = a.position.x + (b.position.x - a.position.x) * factor;
    result.position.y = a.position.y + (b.position.y - a.position.y) * factor;
    result.position.z = a.position.z + (b.position.z - a.position.z) * factor;

    // Rotation: Slerp
    DirectX::XMVECTOR qa = DirectX::XMLoadFloat4(&a.rotation);
    DirectX::XMVECTOR qb = DirectX::XMLoadFloat4(&b.rotation);
    DirectX::XMVECTOR qr = DirectX::XMQuaternionSlerp(qa, qb, factor);
    DirectX::XMStoreFloat4(&result.rotation, DirectX::XMQuaternionNormalize(qr));

    // Scale: 線形補間
    result.scale.x = a.scale.x + (b.scale.x - a.scale.x) * factor;
    result.scale.y = a.scale.y + (b.scale.y - a.scale.y) * factor;
    result.scale.z = a.scale.z + (b.scale.z - a.scale.z) * factor;

    result.isValid = true;
    return result;
}

DirectX::XMMATRIX AnimationComponent::BuildMatrixFromTransform(const BoneTransform& transform) {
    DirectX::XMVECTOR scaleVec = DirectX::XMLoadFloat3(&transform.scale);
    DirectX::XMVECTOR rotVec = DirectX::XMLoadFloat4(&transform.rotation);
    DirectX::XMVECTOR posVec = DirectX::XMLoadFloat3(&transform.position);

    rotVec = DirectX::XMQuaternionNormalize(rotVec);

    DirectX::XMMATRIX S = DirectX::XMMatrixScalingFromVector(scaleVec);
    DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(rotVec);
    DirectX::XMMATRIX T = DirectX::XMMatrixTranslationFromVector(posVec);

    return S * R * T;
}

void AnimationComponent::ApplyBoneMatricesToModel() {
    if (!m_modelRender || m_boneMatrices.empty()) return;
    m_modelRender->SetBoneMatrices(m_boneMatrices);
}

bool AnimationComponent::IsValidMatrix(const DirectX::XMMATRIX& mat) const {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float val = DirectX::XMVectorGetByIndex(mat.r[i], j);
            if (!std::isfinite(val) || std::isnan(val)) {
                return false;
            }
        }
    }
    return true;
}

bool AnimationComponent::IsValidBoneTransform(const BoneTransform& transform) const {
    auto isFiniteVec3 = [](const DirectX::XMFLOAT3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        };
    auto isFiniteVec4 = [](const DirectX::XMFLOAT4& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) &&
            std::isfinite(v.z) && std::isfinite(v.w);
        };

    return isFiniteVec3(transform.position) &&
        isFiniteVec4(transform.rotation) &&
        isFiniteVec3(transform.scale);
}

// === 再生制御 ===
void AnimationComponent::Play() {
    if (m_currentClipIndex < 0 || m_currentClipIndex >= (int)m_clips.size()) {
        if (!m_clips.empty()) {
            m_currentClipIndex = 0;
        }
        else {
            return;
        }
    }
    m_isPlaying = true;
    m_isPaused = false;
}

void AnimationComponent::Pause() {
    m_isPaused = true;
}

void AnimationComponent::Resume() {
    m_isPaused = false;
}

void AnimationComponent::Stop() {
    m_isPlaying = false;
    m_isPaused = false;
    m_currentTime = 0.0f;
    m_isBlending = false;
}

void AnimationComponent::Restart() {
    m_currentTime = 0.0f;
    m_isPlaying = true;
    m_isPaused = false;
}

// === アニメーション設定 ===
bool AnimationComponent::SetAnimationClip(int clipIndex) {
    if (clipIndex < 0 || clipIndex >= (int)m_clips.size()) return false;

    m_currentClipIndex = clipIndex;
    m_currentTime = 0.0f;
    m_isBlending = false;
    return true;
}

void AnimationComponent::SetBlendMode(BlendMode mode) {
    m_blendMode = mode;
    switch (mode) {
    case BlendMode::Instant: m_blendDuration = 0.0f; break;
    case BlendMode::Fast:    m_blendDuration = 0.1f; break;
    case BlendMode::Normal:  m_blendDuration = 0.3f; break;
    case BlendMode::Smooth:  m_blendDuration = 0.5f; break;
    case BlendMode::Slow:    m_blendDuration = 1.0f; break;
    }
}

void AnimationComponent::PlayBlend(int targetClipIndex, float blendDuration) {
    if (targetClipIndex < 0 || targetClipIndex >= (int)m_clips.size()) return;
    if (targetClipIndex == m_currentClipIndex) return;

    m_isBlending = true;
    m_blendTargetIndex = targetClipIndex;
    m_blendDuration = blendDuration;
    m_blendTimer = 0.0f;

    // ソース行列を現在の状態で保存
    m_blendSourceMatrices = m_boneMatrices;
}

// === 状態取得 ===
bool AnimationComponent::IsPlaying() const {
    return m_isPlaying && !m_isPaused;
}

bool AnimationComponent::IsPaused() const {
    return m_isPaused;
}

bool AnimationComponent::IsFinished() const {
    if (m_loop || m_currentClipIndex < 0) return false;
    if (m_currentClipIndex >= (int)m_clips.size()) return true;
    return m_currentTime >= m_clips[m_currentClipIndex].duration;
}

float AnimationComponent::GetAnimationProgress() const {
    if (m_currentClipIndex < 0 || m_currentClipIndex >= (int)m_clips.size()) return 0.0f;
    float duration = m_clips[m_currentClipIndex].duration;
    return (duration > 0.0f) ? (m_currentTime / duration) : 0.0f;
}

float AnimationComponent::GetDuration() const {
    if (m_currentClipIndex < 0 || m_currentClipIndex >= (int)m_clips.size()) return 0.0f;
    return m_clips[m_currentClipIndex].duration;
}

float AnimationComponent::GetBlendProgress() const {
    return m_isBlending ? (m_blendTimer / m_blendDuration) : 0.0f;
}

// === ヘルパー関数 ===
ModelRenderComponent* AnimationComponent::GetModelRenderComponent() {
    return _Parent->GetComponent<ModelRenderComponent>();
}

std::shared_ptr<ModelSharedResource> AnimationComponent::GetModelResource() {
    if (!m_modelRender) return nullptr;
    return ModelManager::Instance()->LoadOrGet(m_modelRender->GetModelPath());
}

// === セーブ/ロード ===
void AnimationComponent::SaveToFile(std::ostream& out) {
    out << m_currentClipIndex << "\n";
    out << (m_loop ? 1 : 0) << "\n";
    out << m_playbackSpeed << "\n";
    out << static_cast<int>(m_blendMode) << "\n";
}

void AnimationComponent::LoadFromFile(std::istream& in) {
    in >> m_currentClipIndex;
    int loopInt;
    in >> loopInt;
    m_loop = (loopInt != 0);
    in >> m_playbackSpeed;
    int blendModeInt;
    in >> blendModeInt;
    m_blendMode = static_cast<BlendMode>(blendModeInt);
}

// === ImGui インスペクター表示 ===
void AnimationComponent::DrawInspector() {
    if (!ImGui::CollapsingHeader("AnimationComponent", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // === アニメーションクリップ選択 ===
    ImGui::Text("Animation Clips: %zu", m_clips.size());

    if (!m_clips.empty()) {
        std::vector<const char*> clipNames;
        for (auto& clip : m_clips) {
            clipNames.push_back(clip.name.c_str());
        }

        int currentIndex = m_currentClipIndex;
        if (ImGui::Combo("Current Clip", &currentIndex, clipNames.data(),
            static_cast<int>(clipNames.size()))) {
            SetAnimationClip(currentIndex);
        }

        if (m_currentClipIndex >= 0 && m_currentClipIndex < (int)m_clips.size()) {
            auto& clip = m_clips[m_currentClipIndex];
            ImGui::Text("Duration: %.2f sec", clip.duration);
            ImGui::Text("Channels: %zu", clip.channels.size());
        }
    }
    else {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No animation clips available");
    }

    ImGui::Separator();

    // === 再生制御 ===
    ImGui::Text("Playback Control");

    if (ImGui::Button(m_isPlaying && !m_isPaused ? "Pause" : "Play")) {
        if (m_isPlaying && !m_isPaused) {
            Pause();
        }
        else if (m_isPaused) {
            Resume();
        }
        else {
            Play();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        Stop();
    }
    ImGui::SameLine();
    if (ImGui::Button("Restart")) {
        Restart();
    }

    // === 再生状態表示 ===
    ImGui::Text("Status: %s",
        m_isPlaying ? (m_isPaused ? "Paused" : "Playing") : "Stopped");

    if (m_isBlending) {
        ImGui::TextColored(ImVec4(0, 1, 1, 1), "Blending: %.1f%%",
            GetBlendProgress() * 100.0f);
    }

    // === 再生時間とプログレスバー ===
    if (m_currentClipIndex >= 0 && m_currentClipIndex < (int)m_clips.size()) {
        float duration = m_clips[m_currentClipIndex].duration;
        float progress = GetAnimationProgress();

        ImGui::Text("Time: %.2f / %.2f sec", m_currentTime, duration);
        ImGui::ProgressBar(progress, ImVec2(-1, 0), "");

        // タイムスライダー
        float sliderTime = m_currentTime;
        if (ImGui::SliderFloat("##TimeSlider", &sliderTime, 0.0f, duration, "")) {
            m_currentTime = sliderTime;
        }
    }

    ImGui::Separator();

    // === 再生設定 ===
    ImGui::Text("Playback Settings");

    ImGui::Checkbox("Loop", &m_loop);

    float speed = m_playbackSpeed;
    if (ImGui::SliderFloat("Speed", &speed, 0.1f, 3.0f, "%.2f")) {
        m_playbackSpeed = speed;
    }

    ImGui::Separator();

    // === ブレンド設定 ===
    ImGui::Text("Blend Settings");

    const char* blendModeNames[] = { "Instant", "Fast", "Normal", "Smooth", "Slow" };
    int blendModeIndex = static_cast<int>(m_blendMode);
    if (ImGui::Combo("Blend Mode", &blendModeIndex, blendModeNames, 5)) {
        SetBlendMode(static_cast<BlendMode>(blendModeIndex));
    }

    ImGui::Text("Blend Duration: %.2f sec", m_blendDuration);

    // === ブレンド実行 ===
    if (!m_clips.empty() && m_currentClipIndex >= 0) {
        ImGui::Text("Blend To:");
        ImGui::BeginChild("BlendTargetList", ImVec2(0, 100), true);

        for (int i = 0; i < (int)m_clips.size(); ++i) {
            if (i == m_currentClipIndex) continue;

            if (ImGui::Button(m_clips[i].name.c_str(), ImVec2(-1, 0))) {
                PlayBlend(i, m_blendDuration);
            }
        }

        ImGui::EndChild();
    }

    ImGui::Separator();

    // === デバッグ情報 ===
    if (ImGui::TreeNode("Debug Info")) {
        ImGui::Checkbox("Debug Mode", &m_debugMode);

        ImGui::Text("Bone Matrices: %zu", m_boneMatrices.size());
        ImGui::Text("Model Bones: %zu",
            m_modelResource ? m_modelResource->bones.size() : 0);

        if (m_currentClipIndex >= 0 && m_currentClipIndex < (int)m_clips.size()) {
            auto& clip = m_clips[m_currentClipIndex];
            ImGui::Text("Active Channels: %zu", clip.channels.size());

            if (ImGui::TreeNode("Channel Details")) {
                for (size_t i = 0; i < clip.channels.size(); ++i) {
                    auto& ch = clip.channels[i];
                    ImGui::Text("[%zu] NodeIdx=%d, Keys=%zu",
                        i, ch.nodeIndex, ch.timeline.size());
                }
                ImGui::TreePop();
            }
        }

        if (!m_lastError.empty()) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Last Error: %s",
                m_lastError.c_str());
        }

        ImGui::TreePop();
    }
}

// === 補間用ヘルパー関数（AssetTypes.hのAnimationChannelに対応） ===
DirectX::XMFLOAT3 AnimationComponent::InterpolatePosition(
    const AnimationChannel& channel, float time)
{
    if (channel.positionKeys.empty()) {
        return { 0, 0, 0 };
    }
    if (channel.positionKeys.size() == 1) {
        return channel.positionKeys[0].second;
    }

    // 時間に対応するキーフレームを検索
    size_t nextIndex = 0;
    for (size_t i = 0; i < channel.positionKeys.size(); ++i) {
        if (channel.positionKeys[i].first > time) {
            nextIndex = i;
            break;
        }
    }

    if (nextIndex == 0) {
        return channel.positionKeys[0].second;
    }
    if (nextIndex >= channel.positionKeys.size()) {
        return channel.positionKeys.back().second;
    }

    size_t prevIndex = nextIndex - 1;
    float t0 = channel.positionKeys[prevIndex].first;
    float t1 = channel.positionKeys[nextIndex].first;
    float factor = (time - t0) / (t1 - t0);

    const auto& a = channel.positionKeys[prevIndex].second;
    const auto& b = channel.positionKeys[nextIndex].second;

    DirectX::XMFLOAT3 result;
    result.x = a.x + (b.x - a.x) * factor;
    result.y = a.y + (b.y - a.y) * factor;
    result.z = a.z + (b.z - a.z) * factor;

    return result;
}

DirectX::XMFLOAT4 AnimationComponent::InterpolateRotation(
    const AnimationChannel& channel, float time)
{
    if (channel.rotationKeys.empty()) {
        return { 0, 0, 0, 1 }; // 単位クォータニオン
    }
    if (channel.rotationKeys.size() == 1) {
        return channel.rotationKeys[0].second;
    }

    // 時間に対応するキーフレームを検索
    size_t nextIndex = 0;
    for (size_t i = 0; i < channel.rotationKeys.size(); ++i) {
        if (channel.rotationKeys[i].first > time) {
            nextIndex = i;
            break;
        }
    }

    if (nextIndex == 0) {
        return channel.rotationKeys[0].second;
    }
    if (nextIndex >= channel.rotationKeys.size()) {
        return channel.rotationKeys.back().second;
    }

    size_t prevIndex = nextIndex - 1;
    float t0 = channel.rotationKeys[prevIndex].first;
    float t1 = channel.rotationKeys[nextIndex].first;
    float factor = (time - t0) / (t1 - t0);

    const auto& a = channel.rotationKeys[prevIndex].second;
    const auto& b = channel.rotationKeys[nextIndex].second;

    // Slerp補間
    DirectX::XMVECTOR qa = DirectX::XMLoadFloat4(&a);
    DirectX::XMVECTOR qb = DirectX::XMLoadFloat4(&b);
    DirectX::XMVECTOR qr = DirectX::XMQuaternionSlerp(qa, qb, factor);

    DirectX::XMFLOAT4 result;
    DirectX::XMStoreFloat4(&result, DirectX::XMQuaternionNormalize(qr));
    return result;
}

DirectX::XMFLOAT3 AnimationComponent::InterpolateScale(
    const AnimationChannel& channel, float time)
{
    if (channel.scaleKeys.empty()) {
        return { 1, 1, 1 };
    }
    if (channel.scaleKeys.size() == 1) {
        return channel.scaleKeys[0].second;
    }

    // 時間に対応するキーフレームを検索
    size_t nextIndex = 0;
    for (size_t i = 0; i < channel.scaleKeys.size(); ++i) {
        if (channel.scaleKeys[i].first > time) {
            nextIndex = i;
            break;
        }
    }

    if (nextIndex == 0) {
        return channel.scaleKeys[0].second;
    }
    if (nextIndex >= channel.scaleKeys.size()) {
        return channel.scaleKeys.back().second;
    }

    size_t prevIndex = nextIndex - 1;
    float t0 = channel.scaleKeys[prevIndex].first;
    float t1 = channel.scaleKeys[nextIndex].first;
    float factor = (time - t0) / (t1 - t0);

    const auto& a = channel.scaleKeys[prevIndex].second;
    const auto& b = channel.scaleKeys[nextIndex].second;

    DirectX::XMFLOAT3 result;
    result.x = a.x + (b.x - a.x) * factor;
    result.y = a.y + (b.y - a.y) * factor;
    result.z = a.z + (b.z - a.z) * factor;

    // スケール値の安全性チェック
    const float MIN_SCALE = 0.01f;
    const float MAX_SCALE = 100.0f;
    result.x = std::max(MIN_SCALE, std::min(MAX_SCALE, result.x));
    result.y = std::max(MIN_SCALE, std::min(MAX_SCALE, result.y));
    result.z = std::max(MIN_SCALE, std::min(MAX_SCALE, result.z));

    return result;
}