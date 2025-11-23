#define NOMINMAX
#include "AnimationComponent.h"
#include "ErrorLog.h"
#include "IMGUI/imgui.h"
#include <algorithm>
#include <set>
#include <cmath>

using namespace DirectX;

void AnimationComponent::Init(Object* owner) {
    _Parent = owner;
    _ComponentName = "Animation";
    _Type = ComponentManager::COMPONENT_TYPE::ANIMATION;
}

ModelRenderComponent* AnimationComponent::GetRenderer() {
    return _Parent->GetComponent<ModelRenderComponent>();
}
std::shared_ptr<ModelSharedResource> AnimationComponent::GetResource() {
    return m_renderer ? ModelManager::Instance()->LoadOrGet(m_renderer->GetModelPath()) : nullptr;
}

void AnimationComponent::BeginPlay() {
    m_renderer = GetRenderer();
    if (!m_renderer) { ErrorLogger::Instance().LogError("Animation", "Renderer not found"); return; }
    m_resource = GetResource();
    if (!m_resource) { ErrorLogger::Instance().LogError("Animation", "Resource null"); return; }

    // クリップ展開
    m_clips.clear();
    for (auto& c : m_resource->clips) {
        AnimationClipRuntime rt;
        rt.name = c.name;
        rt.duration = c.duration / c.tps;
        rt.tps = c.tps;
        rt.isLoaded = true;

        for (auto& ch : c.channels) {
            AnimationChannelRuntime rch;
            rch.nodeIndex = ch.nodeIndex;
            // 位置/回転/スケール キー統合
            std::set<float> times;
            for (auto& k : ch.positionKeys) times.insert(k.first);
            for (auto& k : ch.rotationKeys) times.insert(k.first);
            for (auto& k : ch.scaleKeys)    times.insert(k.first);

            for (float t : times) {
                BoneTransform bt;
                // 位置
                if (!ch.positionKeys.empty()) {
                    auto it = std::lower_bound(ch.positionKeys.begin(), ch.positionKeys.end(), std::pair<float, XMFLOAT3>(t, {}),
                        [](auto& a, auto& b) {return a.first < b.first; });
                    if (it == ch.positionKeys.begin()) bt.position = it->second;
                    else if (it == ch.positionKeys.end()) bt.position = ch.positionKeys.back().second;
                    else {
                        auto prev = std::prev(it);
                        float f = (t - prev->first) / (it->first - prev->first);
                        bt.position = {
                            prev->second.x + (it->second.x - prev->second.x) * f,
                            prev->second.y + (it->second.y - prev->second.y) * f,
                            prev->second.z + (it->second.z - prev->second.z) * f
                        };
                    }
                }
                else bt.position = { 0,0,0 };

                // 回転
                if (!ch.rotationKeys.empty()) {
                    auto it = std::lower_bound(ch.rotationKeys.begin(), ch.rotationKeys.end(), std::pair<float, XMFLOAT4>(t, {}),
                        [](auto& a, auto& b) {return a.first < b.first; });
                    if (it == ch.rotationKeys.begin()) bt.rotation = it->second;
                    else if (it == ch.rotationKeys.end()) bt.rotation = ch.rotationKeys.back().second;
                    else {
                        auto prev = std::prev(it);
                        float f = (t - prev->first) / (it->first - prev->first);
                        XMVECTOR qa = XMLoadFloat4(&prev->second);
                        XMVECTOR qb = XMLoadFloat4(&it->second);
                        XMVECTOR q = XMQuaternionNormalize(XMQuaternionSlerp(qa, qb, f));
                        XMStoreFloat4(&bt.rotation, q);
                    }
                }
                else bt.rotation = { 0,0,0,1 };

                // スケール
                if (!ch.scaleKeys.empty()) {
                    auto it = std::lower_bound(ch.scaleKeys.begin(), ch.scaleKeys.end(), std::pair<float, XMFLOAT3>(t, {}),
                        [](auto& a, auto& b) {return a.first < b.first; });
                    if (it == ch.scaleKeys.begin()) bt.scale = it->second;
                    else if (it == ch.scaleKeys.end()) bt.scale = ch.scaleKeys.back().second;
                    else {
                        auto prev = std::prev(it);
                        float f = (t - prev->first) / (it->first - prev->first);
                        bt.scale = {
                            prev->second.x + (it->second.x - prev->second.x) * f,
                            prev->second.y + (it->second.y - prev->second.y) * f,
                            prev->second.z + (it->second.z - prev->second.z) * f
                        };
                    }
                }
                else bt.scale = { 1,1,1 };
                bt.isValid = true;
                rch.timeline[t] = bt;
            }
            rt.channels.push_back(rch);
        }
        m_clips.push_back(rt);
    }

    // Bone 行列配列初期化
    if (!m_resource->bones.empty()) {
        m_boneMatrices.assign(m_resource->bones.size(), XMFLOAT4X4());
        for (auto& m : m_boneMatrices)
            XMStoreFloat4x4(&m, XMMatrixIdentity());
        m_sourceBlend = m_boneMatrices;
        m_targetBlend = m_boneMatrices;
    }

    // nodeName → boneIndex マップ
    m_nodeToBone.clear();
    for (size_t i = 0; i < m_resource->bones.size(); ++i)
        m_nodeToBone[m_resource->bones[i].name] = (int)i;
}

void AnimationComponent::InGameUpdate() {
    if (!m_playing || m_paused) return;
    if (m_currentClip < 0 || m_currentClip >= (int)m_clips.size()) return;

    float dt = 0.016f; // 仮: 実際は TimeSystem から取得
    if (m_blending) UpdateBlend(dt);
    else UpdateAnimation(dt);

    RebuildBoneMatrices();
    ApplyToModel();
}

void AnimationComponent::UpdateAnimation(float dt) {
    auto& clip = m_clips[m_currentClip];
    m_time += dt * m_speed;
    if (m_time >= clip.duration) {
        if (m_loop) m_time = fmod(m_time, (float)clip.duration);
        else { m_time = (float)clip.duration; m_playing = false; }
    }
}

void AnimationComponent::UpdateBlend(float dt) {
    m_blendTimer += dt;
    float prog = m_blendTimer / m_blendDuration;
    if (prog >= 1.0f) {
        m_blending = false;
        m_currentClip = m_blendTarget;
        m_blendTimer = 0.0f;
        m_time = 0.0f;
    }
    else {
        m_time += dt * m_speed;
        auto& tgt = m_clips[m_blendTarget];
        if (m_time >= tgt.duration) {
            if (m_loop) m_time = fmod(m_time, (float)tgt.duration);
            else m_time = (float)tgt.duration;
        }
    }
}

void AnimationComponent::RebuildBoneMatrices() {
    if (m_boneMatrices.empty()) return;

    if (m_blending) {
        BuildClipPose(m_currentClip, m_time, m_sourceBlend);
        BuildClipPose(m_blendTarget, m_time, m_targetBlend);
        float f = m_blendTimer / m_blendDuration;
        // SmoothStep
        f = f * f * (3.0f - 2.0f * f);
        for (size_t i = 0; i < m_boneMatrices.size(); ++i) {
            XMMATRIX A = XMLoadFloat4x4(&m_sourceBlend[i]);
            XMMATRIX B = XMLoadFloat4x4(&m_targetBlend[i]);
            XMMATRIX R = BlendBoneMatrix(A, B, f);
            XMStoreFloat4x4(&m_boneMatrices[i], R);
        }
    }
    else {
        BuildClipPose(m_currentClip, m_time, m_boneMatrices);
    }
}

DirectX::XMMATRIX AnimationComponent::BlendBoneMatrix(const XMMATRIX& A, const XMMATRIX& B, float f) const {
    // Decompose A
    XMVECTOR sA, rA, tA;
    XMMatrixDecompose(&sA, &rA, &tA, A);
    XMVECTOR sB, rB, tB;
    XMMatrixDecompose(&sB, &rB, &tB, B);

    // Lerp / Slerp
    XMVECTOR s = XMVectorLerp(sA, sB, f);
    XMVECTOR t = XMVectorLerp(tA, tB, f);
    XMVECTOR r = XMQuaternionSlerp(rA, rB, f);
    r = XMQuaternionNormalize(r);

    return XMMatrixScalingFromVector(s) *
        XMMatrixRotationQuaternion(r) *
        XMMatrixTranslationFromVector(t);
}

// 修正後: BuildClipPose（最終行列式とグローバル合成順）
void AnimationComponent::BuildClipPose(int clipIndex, float time,
    std::vector<DirectX::XMFLOAT4X4>& outFinal)
{
    if (clipIndex < 0 || clipIndex >= (int)m_clips.size()) return;
    if (!m_resource) return;

    auto& clipRuntime = m_clips[clipIndex];
    auto& sourceClip = m_resource->clips[clipIndex];

    size_t nodeCount = sourceClip.nodeHierarchy.size();
    std::vector<DirectX::XMMATRIX> local(nodeCount, DirectX::XMMatrixIdentity());

    // 初期ローカル（ノード初期姿勢）
    for (size_t i = 0; i < nodeCount; ++i)
        local[i] = sourceClip.nodeHierarchy[i].localTransform;

    // アニメ適用
    for (auto& ch : clipRuntime.channels) {
        if (ch.nodeIndex < 0 || ch.nodeIndex >= (int)nodeCount) continue;
        BoneTransform bt = InterpChannel(ch, time);
        local[ch.nodeIndex] = BuildMatrix(bt);
    }

    // グローバル合成（旧式: local * parent）
    std::vector<DirectX::XMMATRIX> global(nodeCount, DirectX::XMMatrixIdentity());
    for (size_t i = 0; i < nodeCount; ++i) {
        int parent = sourceClip.nodeHierarchy[i].parentIndex;
        if (parent >= 0)
            global[i] = local[i] * global[parent];
        else
            global[i] = local[i];
    }

    // 出力初期化
    for (auto& f : outFinal)
        DirectX::XMStoreFloat4x4(&f, DirectX::XMMatrixIdentity());

    // 最終ボーン行列: InverseBindPose * CurrentGlobal
    for (size_t b = 0; b < m_resource->bones.size(); ++b) {
        const auto& bone = m_resource->bones[b];

        int nodeIdx = bone.nodeIndex;
        if (nodeIdx < 0 || nodeIdx >= (int)global.size()) {
            // 名前でフォールバック検索
            for (size_t n = 0; n < nodeCount; ++n) {
                if (sourceClip.nodeHierarchy[n].name == bone.name) {
                    nodeIdx = (int)n;
                    break;
                }
            }
        }
        if (nodeIdx < 0) continue;

        DirectX::XMMATRIX boneGlobal = global[nodeIdx];
        DirectX::XMMATRIX finalMat = bone.offset * boneGlobal;

        if (IsValidMatrix(finalMat))
            DirectX::XMStoreFloat4x4(&outFinal[b], finalMat);
    }
}

BoneTransform AnimationComponent::InterpChannel(const AnimationChannelRuntime& ch, float t) const {
    if (ch.timeline.empty())
        return { {0,0,0},{0,0,0,1},{1,1,1},true };

    auto it = ch.timeline.lower_bound(t);
    if (it == ch.timeline.begin()) return it->second;
    if (it == ch.timeline.end())   return ch.timeline.rbegin()->second;

    auto next = it;
    auto prev = std::prev(it);
    float t0 = prev->first;
    float t1 = next->first;
    float f = (t - t0) / (t1 - t0);
    BoneTransform r;
    // pos
    r.position.x = prev->second.position.x + (next->second.position.x - prev->second.position.x) * f;
    r.position.y = prev->second.position.y + (next->second.position.y - prev->second.position.y) * f;
    r.position.z = prev->second.position.z + (next->second.position.z - prev->second.position.z) * f;
    // rot
    XMVECTOR qa = XMLoadFloat4(&prev->second.rotation);
    XMVECTOR qb = XMLoadFloat4(&next->second.rotation);
    XMVECTOR q = XMQuaternionNormalize(XMQuaternionSlerp(qa, qb, f));
    XMStoreFloat4(&r.rotation, q);
    // scale
    r.scale.x = prev->second.scale.x + (next->second.scale.x - prev->second.scale.x) * f;
    r.scale.y = prev->second.scale.y + (next->second.scale.y - prev->second.scale.y) * f;
    r.scale.z = prev->second.scale.z + (next->second.scale.z - prev->second.scale.z) * f;
    r.isValid = true;
    return r;
}

DirectX::XMMATRIX AnimationComponent::BuildMatrix(const BoneTransform& bt) const {
    XMVECTOR S = XMLoadFloat3(&bt.scale);
    XMVECTOR R = XMLoadFloat4(&bt.rotation);
    XMVECTOR P = XMLoadFloat3(&bt.position);
    R = XMQuaternionNormalize(R);
    return XMMatrixScalingFromVector(S) * XMMatrixRotationQuaternion(R) * XMMatrixTranslationFromVector(P);
}

bool AnimationComponent::IsValidMatrix(const XMMATRIX& m) const {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float v = XMVectorGetByIndex(m.r[i], j);
            if (!std::isfinite(v)) return false;
        }
    }
    return true;
}

void AnimationComponent::ApplyToModel() {
    if (!m_renderer || m_boneMatrices.empty()) return;
    m_renderer->SetBoneMatrices(m_boneMatrices);
}

void AnimationComponent::DrawInspector() {
    if (!ImGui::CollapsingHeader("AnimationComponent", ImGuiTreeNodeFlags_DefaultOpen))
        return;
    ImGui::Text("Clips: %zu", m_clips.size());
    if (!m_clips.empty()) {
        std::vector<const char*> names;
        for (auto& c : m_clips) names.push_back(c.name.c_str());
        int idx = m_currentClip;
        if (ImGui::Combo("Current Clip", &idx, names.data(), (int)names.size())) {
            SetAnimationClip(idx);
        }
    }
    if (ImGui::Button(m_playing && !m_paused ? "Pause" : "Play")) {
        if (m_playing && !m_paused) Pause();
        else if (m_paused) Resume();
        else Play();
    } ImGui::SameLine();
    if (ImGui::Button("Stop")) Stop();
    ImGui::SameLine();
    if (ImGui::Button("Restart")) Restart();

    ImGui::Checkbox("Loop", &m_loop);
    ImGui::SliderFloat("Speed", &m_speed, 0.05f, 3.0f, "%.2f");

    if (m_currentClip >= 0) {
        float dur = (float)m_clips[m_currentClip].duration;
        ImGui::Text("Time %.3f / %.3f", m_time, dur);
        float scrub = m_time;
        if (ImGui::SliderFloat("Scrub", &scrub, 0.0f, dur)) m_time = scrub;
        ImGui::ProgressBar(GetAnimationProgress(), ImVec2(-1, 0));
    }

    // Blend
    const char* bnames[] = { "Instant","Fast","Normal","Smooth","Slow" };
    int bidx = (int)m_blendMode;
    if (ImGui::Combo("Blend Mode", &bidx, bnames, 5)) SetBlendMode((BlendMode)bidx);
    ImGui::SliderFloat("Blend Duration", &m_blendDuration, 0.0f, 2.0f, "%.2f");
    if (!m_clips.empty() && m_currentClip >= 0) {
        ImGui::Text("Blend To:");
        for (int i = 0; i < (int)m_clips.size(); ++i) {
            if (i == m_currentClip) continue;
            if (ImGui::Button(m_clips[i].name.c_str()))
                PlayBlend(i, m_blendDuration);
        }
    }
}

void AnimationComponent::Play() {
    if (m_currentClip < 0 && !m_clips.empty()) m_currentClip = 0;
    if (m_currentClip < 0) return;
    m_playing = true; m_paused = false;
}
void AnimationComponent::Pause() { m_paused = true; }
void AnimationComponent::Resume() { m_paused = false; }
void AnimationComponent::Stop() { m_playing = false; m_paused = false; m_time = 0.f; m_blending = false; }
void AnimationComponent::Restart() { m_time = 0.f; m_playing = true; m_paused = false; }

bool AnimationComponent::SetAnimationClip(int clipIndex) {
    if (clipIndex < 0 || clipIndex >= (int)m_clips.size()) return false;
    m_currentClip = clipIndex; m_time = 0.f; m_blending = false;
    return true;
}

void AnimationComponent::SetBlendMode(BlendMode m) {
    m_blendMode = m;
    switch (m) {
    case BlendMode::Instant: m_blendDuration = 0.0f; break;
    case BlendMode::Fast:    m_blendDuration = 0.1f; break;
    case BlendMode::Normal:  m_blendDuration = 0.3f; break;
    case BlendMode::Smooth:  m_blendDuration = 0.5f; break;
    case BlendMode::Slow:    m_blendDuration = 1.0f; break;
    }
}

bool AnimationComponent::IsPlaying() const { return m_playing && !m_paused; }
bool AnimationComponent::IsPaused()  const { return m_paused; }
bool AnimationComponent::IsFinished() const {
    if (m_loop || m_currentClip < 0 || m_currentClip >= (int)m_clips.size()) return false;
    return m_time >= (float)m_clips[m_currentClip].duration;
}
float AnimationComponent::GetAnimationProgress() const {
    if (m_currentClip < 0 || m_currentClip >= (int)m_clips.size()) return 0.f;
    float d = (float)m_clips[m_currentClip].duration;
    return d > 0 ? m_time / d : 0.f;
}

void AnimationComponent::PlayBlend(int targetClip, float duration) {
    if (targetClip < 0 || targetClip >= (int)m_clips.size()) return;
    if (targetClip == m_currentClip) return;
    m_blending = true;
    m_blendTarget = targetClip;
    m_blendDuration = duration;
    m_blendTimer = 0.f;
    m_sourceBlend = m_boneMatrices;
}

void AnimationComponent::SaveToFile(std::ostream& out) {
    out << m_currentClip << "\n"
        << (m_loop ? 1 : 0) << "\n"
        << m_speed << "\n"
        << (int)m_blendMode << "\n";
}

void AnimationComponent::LoadFromFile(std::istream& in) {
    in >> m_currentClip;
    int loop; in >> loop; m_loop = loop != 0;
    in >> m_speed;
    int bm; in >> bm; m_blendMode = (BlendMode)bm;
    SetBlendMode(m_blendMode);
}