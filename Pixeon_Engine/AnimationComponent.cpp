#include "AnimationComponent.h"
#include "ModelRender.h"
#include "EditrGUI.h"
#include "ErrorLog.h"


using namespace DirectX;

void AnimationComponentRebuild::Init(Object* owner) {
    _Parent = owner;
    _ComponentName = "AnimationRebuild";
    _Type = ComponentManager::COMPONENT_TYPE::ANIMATION;
}

void AnimationComponentRebuild::BeginPlay() {
    m_renderer = _Parent->GetComponent<ModelRenderComponent>();
    if (!m_renderer) {
        //ErrorLog::Instance().LogError("Animation", "No renderer");
		EditrGUI::GetInstance()->WriteLog("[AnimationComponentRebuild::BeginPlay] No renderer");
        return;
    }
    // Renderer から新しいリソース取得（あなたの実装に合わせて差し替え）
    // ここでは仮: dynamic_cast などで取得
    m_resource = std::dynamic_pointer_cast<ModelSharedResourceNew>(
        /* あなたの ModelManagerRebuild::Instance()->Load(...) 呼び出し */ nullptr
    );
    if (!m_resource) {
        ErrorLog::Instance().LogError("Animation", "No resource");
        return;
    }
    m_clips.clear();
    for (auto& c : m_resource->clips) {
        RuntimeClip rc;
        rc.name = c.name;
        rc.duration = c.durationSeconds;
        rc.tps = c.ticksPerSecond;
        for (auto& ch : c.channels) {
            RuntimeChannel rch;
            rch.nodeIndex = ch.nodeIndex;
            rch.timeline = const_cast<std::map<float, AnimeTransform>*>(&ch.timeline);
            rc.channels.push_back(rch);
        }
        m_clips.push_back(rc);
    }

    m_boneFinal.assign(m_resource->bones.size(), XMFLOAT4X4());
    for (auto& m : m_boneFinal) XMStoreFloat4x4(&m, XMMatrixIdentity());
    // 自動再生
    if (!m_clips.empty()) {
        m_currentClip = 0;
        m_time = 0;
    }
    int missing = 0;
    for (auto& b : m_resource->bones) if (b.nodeIndex < 0) ++missing;
    EditrGUI::GetInstance()->WriteLog("[AnimationRebuild BeginPlay] clips=" + std::to_string(m_clips.size()) +
        " bones=" + std::to_string(m_resource->bones.size()) +
        " missingNodeIndex=" + std::to_string(missing));
}

AnimeTransform AnimationComponentRebuild::SampleChannel(const RuntimeChannel& ch, float t) const {
    AnimeTransform def{};
    def.translation = { 0,0,0 };
    def.rotation = { 0,0,0,1 };
    def.scale = { 1,1,1 };
    if (!ch.timeline || ch.timeline->empty()) return def;
    auto it = ch.timeline->lower_bound(t);
    if (it == ch.timeline->begin()) return it->second;
    if (it == ch.timeline->end()) return std::prev(it)->second;
    auto prev = std::prev(it);
    float t0 = prev->first; float t1 = it->first;
    float f = (t - t0) / (t1 - t0);

    // 補間
    AnimeTransform a = prev->second;
    AnimeTransform b = it->second;
    // 回転 slerp (符号あわせ)
    XMVECTOR qa = XMLoadFloat4(&a.rotation);
    XMVECTOR qb = XMLoadFloat4(&b.rotation);
    if (XMVectorGetX(XMQuaternionDot(qa, qb)) < 0.0f) {
        qb = XMVectorNegate(qb);
    }
    XMVECTOR q = XMQuaternionSlerp(qa, qb, f);
    q = XMQuaternionNormalize(q);

    AnimeTransform r;
    r.translation = {
        a.translation.x + (b.translation.x - a.translation.x) * f,
        a.translation.y + (b.translation.y - a.translation.y) * f,
        a.translation.z + (b.translation.z - a.translation.z) * f
    };
    XMStoreFloat4(&r.rotation, q);
    r.scale = {
        a.scale.x + (b.scale.x - a.scale.x) * f,
        a.scale.y + (b.scale.y - a.scale.y) * f,
        a.scale.z + (b.scale.z - a.scale.z) * f
    };
    return r;
}

XMMATRIX AnimationComponentRebuild::BuildLocalMatrix(const AnimeTransform& tr) const {
    XMVECTOR S = XMLoadFloat3(&tr.scale);
    XMVECTOR R = XMLoadFloat4(&tr.rotation);
    R = XMQuaternionNormalize(R);
    XMVECTOR T = XMLoadFloat3(&tr.translation);
    return XMMatrixScalingFromVector(S) *
        XMMatrixRotationQuaternion(R) *
        XMMatrixTranslationFromVector(T);
}

void AnimationComponentRebuild::BuildPoseSingle(int clipIndex, float time,
    std::vector<XMFLOAT4X4>& outFinal) {
    if (clipIndex < 0 || clipIndex >= (int)m_clips.size()) return;
    auto& clip = m_clips[clipIndex];
    auto& nodes = m_resource->nodes;

    std::vector<XMMATRIX> local(nodes.size());
    // 初期は bindLocal
    for (size_t i = 0; i < nodes.size(); ++i) local[i] = nodes[i].bindLocal;

    // チャンネル適用
    float tLoop = clip.duration > 0 && m_loop ? fmod(time, clip.duration) : std::min(time, clip.duration);
    for (auto& ch : clip.channels) {
        if (ch.nodeIndex < 0 || ch.nodeIndex >= (int)nodes.size()) continue;
        AnimeTransform at = SampleChannel(ch, tLoop);
        local[ch.nodeIndex] = BuildLocalMatrix(at);
    }

    // global
    std::vector<XMMATRIX> global(nodes.size(), XMMatrixIdentity());
    for (size_t i = 0; i < nodes.size(); ++i) {
        int p = nodes[i].parentIndex;
        global[i] = (p >= 0) ? (local[i] * global[p]) : local[i];
    }

    // bone final
    if (outFinal.size() != m_resource->bones.size())
        outFinal.resize(m_resource->bones.size());

    for (size_t b = 0; b < m_resource->bones.size(); ++b) {
        auto& bone = m_resource->bones[b];
        if (bone.nodeIndex < 0 || bone.nodeIndex >= (int)global.size()) {
            XMStoreFloat4x4(&outFinal[b], XMMatrixIdentity());
            continue;
        }
        XMMATRIX final = bone.invBind * global[bone.nodeIndex];
        // 転置してシェーダへ
        XMStoreFloat4x4(&outFinal[b], XMMatrixTranspose(final));
    }
}

void AnimationComponentRebuild::BuildPoseBlended(int A, float tA, int B, float tB, float blend,
    std::vector<XMFLOAT4X4>& outFinal) {
    // 単純: A/B の global 計算 → 両方の最終骨行列を一旦 raw(非転置)で作り、行列分解→補間→再構成(推奨)だが
    // ここでは簡易で "行列Lerp" を避け、チャンネルサンプル段階で補間: 
    // 実装簡略化のため blend は A/B の localTransform 直接補間 (回転はSlerp)
    if (A < 0 || B < 0 || A >= (int)m_clips.size() || B >= (int)m_clips.size()) {
        BuildPoseSingle(A >= 0 ? A : B, A >= 0 ? tA : tB, outFinal);
        return;
    }
    auto& clipA = m_clips[A];
    auto& clipB = m_clips[B];
    auto& nodes = m_resource->nodes;
    std::vector<XMMATRIX> local(nodes.size());
    // bind をベース
    for (size_t i = 0; i < nodes.size(); ++i) local[i] = nodes[i].bindLocal;

    float tLoopA = clipA.duration > 0 && m_loop ? fmod(tA, clipA.duration) : std::min(tA, clipA.duration);
    float tLoopB = clipB.duration > 0 && m_loop ? fmod(tB, clipB.duration) : std::min(tB, clipB.duration);

    auto sampleClip = [&](const RuntimeClip& rc, float sampTime, std::vector<AnimeTransform>& out) {
        out.resize(nodes.size());
        // 初期 transform
        for (size_t i = 0; i < nodes.size(); ++i) {
            out[i].translation = { 0,0,0 };
            out[i].rotation = { 0,0,0,1 };
            out[i].scale = { 1,1,1 };
        }
        for (auto& ch : rc.channels) {
            if (ch.nodeIndex < 0 || ch.nodeIndex >= (int)nodes.size()) continue;
            out[ch.nodeIndex] = SampleChannel(ch, sampTime);
        }
        };

    std::vector<AnimeTransform> ta, tb;
    sampleClip(clipA, tLoopA, ta);
    sampleClip(clipB, tLoopB, tb);

    for (size_t i = 0; i < nodes.size(); ++i) {
        AnimeTransform a = ta[i];
        AnimeTransform b = tb[i];
        // blend補間 (回転slerp)
        XMVECTOR qa = XMLoadFloat4(&a.rotation);
        XMVECTOR qb = XMLoadFloat4(&b.rotation);
        if (XMVectorGetX(XMQuaternionDot(qa, qb)) < 0.0f) qb = XMVectorNegate(qb);
        XMVECTOR q = XMQuaternionSlerp(qa, qb, blend);
        q = XMQuaternionNormalize(q);
        AnimeTransform r;
        r.translation = {
            a.translation.x + (b.translation.x - a.translation.x) * blend,
            a.translation.y + (b.translation.y - a.translation.y) * blend,
            a.translation.z + (b.translation.z - a.translation.z) * blend
        };
        XMStoreFloat4(&r.rotation, q);
        r.scale = {
            a.scale.x + (b.scale.x - a.scale.x) * blend,
            a.scale.y + (b.scale.y - a.scale.y) * blend,
            a.scale.z + (b.scale.z - a.scale.z) * blend
        };
        local[i] = BuildLocalMatrix(r);
    }

    // global
    std::vector<XMMATRIX> global(nodes.size(), XMMatrixIdentity());
    for (size_t i = 0; i < nodes.size(); ++i) {
        int p = nodes[i].parentIndex;
        global[i] = (p >= 0) ? (local[i] * global[p]) : local[i];
    }

    if (outFinal.size() != m_resource->bones.size())
        outFinal.resize(m_resource->bones.size());
    for (size_t b = 0; b < m_resource->bones.size(); ++b) {
        auto& bone = m_resource->bones[b];
        if (bone.nodeIndex < 0 || bone.nodeIndex >= (int)global.size()) {
            XMStoreFloat4x4(&outFinal[b], XMMatrixIdentity());
            continue;
        }
        XMMATRIX final = bone.invBind * global[bone.nodeIndex];
        XMStoreFloat4x4(&outFinal[b], XMMatrixTranspose(final));
    }
}

void AnimationComponentRebuild::InGameUpdate() {
    if (m_currentClip < 0 || m_currentClip >= (int)m_clips.size()) return;
    float dt = 0.016f; // 実際は時間取得
    if (m_blending) {
        m_blendTimer += dt;
        float prog = m_blendDuration > 0 ? (m_blendTimer / m_blendDuration) : 1.0f;
        if (prog >= 1.0f) {
            m_blending = false;
            m_currentClip = m_blendTarget;
            m_time = 0; m_blendTarget = -1;
            EditrGUI::GetInstance()->WriteLog("[Blend] Completed");
        }
        else {
            m_time += dt * m_speed;
            BuildPoseBlended(m_currentClip, m_time, m_blendTarget, m_time, prog, m_boneFinal);
            ApplyToRenderer();
            return;
        }
    }
    // 通常
    m_time += dt * m_speed;
    BuildPoseSingle(m_currentClip, m_time, m_boneFinal);
    ApplyToRenderer();

    static int fc = 0;
    if (++fc % 120 == 0 && !m_boneFinal.empty()) {
        auto& r = m_boneFinal[0];
        EditrGUI::GetInstance()->WriteLog("[AnimTick] clip=" + m_clips[m_currentClip].name +
            " t=" + std::to_string(m_time) +
            " rootT=(" + std::to_string(r._41) + "," + std::to_string(r._42) + "," + std::to_string(r._43) + ")");
    }
}

void AnimationComponentRebuild::ApplyToRenderer() {
    if (!m_renderer) return;
    m_renderer->SetBoneMatrices(m_boneFinal);
}

void AnimationComponentRebuild::Play(int clipIndex, bool loop) {
    if (clipIndex < 0 || clipIndex >= (int)m_clips.size()) return;
    m_currentClip = clipIndex;
    m_loop = loop;
    m_time = 0;
    m_blending = false;
    EditrGUI::GetInstance()->WriteLog("[Play] clip=" + m_clips[clipIndex].name);
}
void AnimationComponentRebuild::PlayBlend(int newClip, float duration) {
    if (newClip < 0 || newClip >= (int)m_clips.size()) return;
    if (m_currentClip < 0) {
        Play(newClip);
        return;
    }
    m_blending = true;
    m_blendTarget = newClip;
    m_blendTimer = 0;
    m_blendDuration = duration;
    EditrGUI::GetInstance()->WriteLog("[Blend] from=" + m_clips[m_currentClip].name +
        " to=" + m_clips[newClip].name +
        " dur=" + std::to_string(duration));
}