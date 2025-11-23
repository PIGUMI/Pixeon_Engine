#define NOMINMAX
#include "AnimationComponentV2.h"
#include "EditrGUI.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

static void Log(const std::string& s) {
    EditrGUI::GetInstance()->WriteLog(s);
}

void AnimationComponentV2::Init(Object* owner) {
    _Parent = owner;
    _ComponentName = "AnimationV2";
    _Type = ComponentManager::COMPONENT_TYPE::ANIMATION;
}

void AnimationComponentV2::BeginPlay() {
    m_renderer = _Parent ? _Parent->GetComponent<ModelRenderComponent>() : nullptr;
    if (!m_renderer) { Log("[AnimationV2] renderer missing"); return; }
    m_resource = m_renderer->GetModelPath().empty()
        ? nullptr
        : ModelManager::Instance()->LoadOrGet(m_renderer->GetModelPath());
    if (!m_resource) { Log("[AnimationV2] resource null"); return; }

    BuildRuntimeFromResource();

    if (!m_bones.empty()) {
        m_bonePalette.resize(m_bones.size());
        for (auto& m : m_bonePalette) XMStoreFloat4x4(&m, XMMatrixIdentity());
    }

    // 自動最初のクリップ再生
    if (!m_clips.empty()) Play(0);

    DumpBoneMap();
    ValidateBindPose();
}

void AnimationComponentV2::BuildRuntimeFromResource() {
    m_bones.clear();
    for (auto& B : m_resource->bones) {
        ACV2_Bone rb;
        rb.name = B.name;
        rb.parentIndex = B.parentIndex;
        rb.nodeIndex = B.nodeIndex;
        rb.inverseBind = B.offset; // InverseBindPose
        m_bones.push_back(rb);
    }

    m_clips.clear();
    for (auto& C : m_resource->clips) {
        ACV2_Clip rc;
        rc.name = C.name;
        rc.duration = float(C.duration / C.tps);
        rc.ticksPerSecond = float(C.tps);

        // ノード階層
        rc.nodes.reserve(C.nodeHierarchy.size());
        for (auto& N : C.nodeHierarchy) {
            ACV2_Node rn;
            rn.name = N.name;
            rn.parentIndex = N.parentIndex;
            rn.bindLocal = N.localTransform; // AssimpToXM_RowMajor 済み
            rc.nodes.push_back(rn);
        }

        // チャンネル
        rc.channels.reserve(C.channels.size());
        for (auto& CH : C.channels) {
            ACV2_Channel rch;
            rch.nodeIndex = CH.nodeIndex;
            rch.posKeys = CH.positionKeys;
            rch.rotKeys = CH.rotationKeys;
            rch.sclKeys = CH.scaleKeys;
            rc.channels.push_back(rch);
        }

        m_clips.push_back(rc);
    }
    Log("[AnimationV2] runtime build clips=" + std::to_string(m_clips.size()) +
        " bones=" + std::to_string(m_bones.size()));
}

void AnimationComponentV2::Play(int clipIndex) {
    if (clipIndex < 0 || clipIndex >= (int)m_clips.size()) return;
    m_currentClip = clipIndex;
    m_time = 0.f;
    m_paused = false;
    m_blendState = BlendState::None;
    Log("[AnimationV2] Play clip=" + m_clips[clipIndex].name);
}

void AnimationComponentV2::Stop() {
    m_paused = false;
    m_currentClip = -1;
    m_time = 0.f;
    m_blendState = BlendState::None;
    Log("[AnimationV2] Stop");
}

void AnimationComponentV2::Pause() {
    if (m_currentClip >= 0) m_paused = true;
}

void AnimationComponentV2::Resume() {
    if (m_currentClip >= 0) m_paused = false;
}

void AnimationComponentV2::Restart() {
    if (m_currentClip >= 0) {
        m_time = 0.f;
        m_paused = false;
        Log("[AnimationV2] Restart clip=" + m_clips[m_currentClip].name);
    }
}

void AnimationComponentV2::StartBlend(int targetClip, float duration) {
    if (m_currentClip < 0) return;
    if (targetClip < 0 || targetClip >= (int)m_clips.size()) return;
    if (targetClip == m_currentClip) return;
    m_blendTarget = targetClip;
    m_blendDuration = std::max(0.0f, duration);
    m_blendTimer = 0.f;
    m_blendState = BlendState::Blending;

    // 現在ポーズを source / target 用に確保
    m_paletteSource = m_bonePalette;
    m_paletteTarget = m_bonePalette; // 後で target 計算
    Log("[AnimationV2] Blend start " + m_clips[m_currentClip].name + " -> " + m_clips[targetClip].name);
}

void AnimationComponentV2::InGameUpdate() {
    if (m_currentClip < 0 || m_paused) return;
    UpdatePlayback(0.016f); // 固定フレーム仮
    ApplyToRenderer();

    static int logCounter = 0;
    if (++logCounter % 180 == 0) {
        Log("[AnimationV2] t=" + std::to_string(m_time) + " clip=" + m_clips[m_currentClip].name);
    }
    // Inspector フラグによる一回処理
    if (m_debugShowBoneMapOnce) {
        DumpBoneMap();
        m_debugShowBoneMapOnce = false;
    }
    if (m_debugValidateBindPoseOnce) {
        ValidateBindPose();
        m_debugValidateBindPoseOnce = false;
    }
    if (m_debugCheckPaletteValidityOnce) {
        CheckPaletteValidity();
        m_debugCheckPaletteValidityOnce = false;
    }
}

void AnimationComponentV2::UpdatePlayback(float dt) {
    auto& clip = m_clips[m_currentClip];
    if (m_blendState == BlendState::Blending) {
        // まず targetClip の現在時刻のポーズを計算
        m_time += dt * m_speed;
        if (clip.duration > 0.f) {
            if (m_time >= clip.duration) {
                if (m_loop) m_time = fmod(m_time, clip.duration);
                else m_time = clip.duration;
            }
        }
        // targetClip は常に 0→進行 （簡易仕様）
        float targetT = std::min(m_time, m_clips[m_blendTarget].duration);
        BuildPose(m_time, m_currentClip, m_paletteSource);
        BuildPose(targetT, m_blendTarget, m_paletteTarget);

        m_blendTimer += dt;
        float f = (m_blendDuration <= 0.f) ? 1.f : (m_blendTimer / m_blendDuration);
        if (f >= 1.f) {
            m_currentClip = m_blendTarget;
            m_blendState = BlendState::None;
            m_time = targetT;
            // 最終ポーズ
            m_bonePalette = m_paletteTarget;
            Log("[AnimationV2] Blend finished. Now clip=" + m_clips[m_currentClip].name);
        }
        else {
            f = f * f * (3.f - 2.f * f); // SmoothStep
            if (m_bonePalette.size() != m_paletteSource.size())
                m_bonePalette.resize(m_paletteSource.size());
            for (size_t i = 0; i < m_bonePalette.size(); ++i) {
                XMMATRIX A = XMLoadFloat4x4(&m_paletteSource[i]);
                XMMATRIX B = XMLoadFloat4x4(&m_paletteTarget[i]);
                // 分解補間
                XMVECTOR sA, rA, tA; XMMatrixDecompose(&sA, &rA, &tA, A);
                XMVECTOR sB, rB, tB; XMMatrixDecompose(&sB, &rB, &tB, B);
                XMVECTOR s = XMVectorLerp(sA, sB, f);
                XMVECTOR t = XMVectorLerp(tA, tB, f);
                XMVECTOR r = XMQuaternionNormalize(XMQuaternionSlerp(rA, rB, f));
                XMMATRIX R = XMMatrixScalingFromVector(s) *
                    XMMatrixRotationQuaternion(r) *
                    XMMatrixTranslationFromVector(t);
                XMStoreFloat4x4(&m_bonePalette[i], R);
            }
        }
    }
    else {
        m_time += dt * m_speed;
        if (clip.duration > 0.f) {
            if (m_time >= clip.duration) {
                if (m_loop) m_time = fmod(m_time, clip.duration);
                else m_time = clip.duration;
            }
        }
        BuildPose(m_time, m_currentClip, m_bonePalette);
    }
}

DirectX::XMMATRIX AnimationComponentV2::InterpChannel(const ACV2_Channel& ch, float t, float clipDuration) const {
    // 位置
    auto SampleVec3 = [&](const std::vector<std::pair<float, XMFLOAT3>>& keys)->XMFLOAT3 {
        if (keys.empty()) return { 0,0,0 };
        if (keys.size() == 1) return keys[0].second;
        if (t <= keys.front().first) return keys.front().second;
        if (t >= keys.back().first) return keys.back().second;
        auto it = std::upper_bound(keys.begin(), keys.end(), t,
            [](float v, const std::pair<float, XMFLOAT3>& k) { return v < k.first; });
        size_t i1 = std::distance(keys.begin(), it);
        size_t i0 = i1 - 1;
        float t0 = keys[i0].first, t1 = keys[i1].first;
        float f = (t - t0) / (t1 - t0);
        const auto& a = keys[i0].second;
        const auto& b = keys[i1].second;
        return { a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f };
        };
    auto SampleQuat = [&](const std::vector<std::pair<float, XMFLOAT4>>& keys)->XMFLOAT4 {
        if (keys.empty()) return { 0,0,0,1 };
        if (keys.size() == 1) return keys[0].second;
        if (t <= keys.front().first) return keys.front().second;
        if (t >= keys.back().first) return keys.back().second;
        auto it = std::upper_bound(keys.begin(), keys.end(), t,
            [](float v, const std::pair<float, XMFLOAT4>& k) { return v < k.first; });
        size_t i1 = std::distance(keys.begin(), it);
        size_t i0 = i1 - 1;
        float t0 = keys[i0].first, t1 = keys[i1].first;
        float f = (t - t0) / (t1 - t0);
        XMVECTOR qa = XMLoadFloat4(&keys[i0].second);
        XMVECTOR qb = XMLoadFloat4(&keys[i1].second);
        XMVECTOR q = XMQuaternionNormalize(XMQuaternionSlerp(qa, qb, f));
        XMFLOAT4 r; XMStoreFloat4(&r, q);
        return r;
        };
    auto SampleScale = SampleVec3;

    XMFLOAT3 P = SampleVec3(ch.posKeys);
    XMFLOAT4 R = SampleQuat(ch.rotKeys);
    XMFLOAT3 S = ch.sclKeys.empty() ? XMFLOAT3{ 1,1,1 } : SampleScale(ch.sclKeys);
    XMVECTOR Pv = XMLoadFloat3(&P);
    XMVECTOR Rv = XMLoadFloat4(&R);
    XMVECTOR Sv = XMLoadFloat3(&S);
    Rv = XMQuaternionNormalize(Rv);
    return XMMatrixScalingFromVector(Sv) *
        XMMatrixRotationQuaternion(Rv) *
        XMMatrixTranslationFromVector(Pv);
}

void AnimationComponentV2::ComputeGlobals(const ACV2_Clip& clip,
    const std::vector<XMMATRIX>& locals,
    std::vector<XMMATRIX>& globals) const {
    size_t n = clip.nodes.size();
    globals.resize(n);
    for (size_t i = 0; i < n; ++i) {
        int parent = clip.nodes[i].parentIndex;
        if (parent >= 0)
            globals[i] = globals[parent] * locals[i];
        else
            globals[i] = locals[i];
    }
}

void AnimationComponentV2::BuildPose(float t, int clipIdx, std::vector<XMFLOAT4X4>& outPalette) {
    if (clipIdx < 0 || clipIdx >= (int)m_clips.size()) return;
    auto& clip = m_clips[clipIdx];
    size_t nodeCount = clip.nodes.size();

    std::vector<XMMATRIX> locals(nodeCount);
    // 初期は bindLocal
    for (size_t i = 0; i < nodeCount; ++i)
        locals[i] = clip.nodes[i].bindLocal;

    // アニメキー適用
    for (auto& ch : clip.channels) {
        if (ch.nodeIndex < 0 || ch.nodeIndex >= (int)nodeCount) continue;
        // 任意キーが無ければ bindLocal のまま
        bool hasAny = !ch.posKeys.empty() || !ch.rotKeys.empty() || !ch.sclKeys.empty();
        if (hasAny)
            locals[ch.nodeIndex] = InterpChannel(ch, t, clip.duration);
    }

    std::vector<XMMATRIX> globals;
    ComputeGlobals(clip, locals, globals);

    if (outPalette.size() != m_bones.size())
        outPalette.resize(m_bones.size());

    int applied = 0;
    for (size_t b = 0; b < m_bones.size(); ++b) {
        const auto& bone = m_bones[b];
        XMMATRIX skin = XMMatrixIdentity();
        if (bone.nodeIndex >= 0 && bone.nodeIndex < (int)globals.size()) {
            // palette = currentGlobal * inverseBind
            skin = globals[bone.nodeIndex] * bone.inverseBind;
            ++applied;
        }
        XMStoreFloat4x4(&outPalette[b], skin);
    }

    static int counter = 0;
    if (++counter % 240 == 0) {
        Log("[AnimationV2] BuildPose clip=" + clip.name + " appliedBones=" + std::to_string(applied));
    }
}

void AnimationComponentV2::ApplyToRenderer() {
    if (!m_renderer) return;
    // 転置モード (行ベクトル VS の救済)
    if (m_debugTransposeForShader) {
        std::vector<XMFLOAT4X4> trans(m_bonePalette.size());
        for (size_t i = 0; i < m_bonePalette.size(); ++i) {
            XMMATRIX M = XMLoadFloat4x4(&m_bonePalette[i]);
            XMStoreFloat4x4(&trans[i], XMMatrixTranspose(M));
        }
        m_renderer->SetBoneMatrices(trans);
    }
    else {
        m_renderer->SetBoneMatrices(m_bonePalette);
    }
}

void AnimationComponentV2::ValidateBindPose() {
    if (m_currentClip < 0) return;
    // t=0 で再構築
    std::vector<XMFLOAT4X4> test;
    BuildPose(0.f, m_currentClip, test);
    int nonIdentity = 0;
    for (auto& m : test) {
        float d = fabs(m._11 - 1) + fabs(m._22 - 1) + fabs(m._33 - 1) +
            fabs(m._12) + fabs(m._13) + fabs(m._14) +
            fabs(m._21) + fabs(m._23) + fabs(m._24) +
            fabs(m._31) + fabs(m._32) + fabs(m._34) +
            fabs(m._41) + fabs(m._42) + fabs(m._43);
        if (d > 1e-3f) ++nonIdentity;
    }
    Log("[AnimationV2] BindPoseCheck nonIdentity=" + std::to_string(nonIdentity) +
        " / " + std::to_string(test.size()));
}

void AnimationComponentV2::DumpBoneMap() {
    if (m_currentClip < 0) return;
    auto& clip = m_clips[m_currentClip];
    for (size_t i = 0; i < m_bones.size(); ++i) {
        const auto& b = m_bones[i];
        std::string nodeName = (b.nodeIndex >= 0 && b.nodeIndex < (int)clip.nodes.size())
            ? clip.nodes[b.nodeIndex].name : "<INVALID>";
        std::string status = (nodeName == b.name) ? "OK" : "Mismatch";
        Log("[BoneMapV2] idx=" + std::to_string(i) + " bone=" + b.name +
            " nodeIdx=" + std::to_string(b.nodeIndex) + " nodeName=" + nodeName +
            " status=" + status);
    }
}

void AnimationComponentV2::CheckPaletteValidity() {
    int bad = 0;
    for (auto& m : m_bonePalette) {
        // NaN/INF 簡易チェック
        bool invalid = false;
        for (int r = 0; r < 4 && !invalid; ++r) {
            for (int c = 0; c < 4; ++c) {
                float* base = (float*)&m;
                float v = base[r * 4 + c];
                if (!std::isfinite(v)) { invalid = true; break; }
            }
        }
        if (invalid) ++bad;
    }
    Log("[AnimationV2] PaletteValidity badMatrices=" + std::to_string(bad));
}

void AnimationComponentV2::DrawInspector() {
    if (!ImGui::CollapsingHeader("AnimationComponentV2", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Text("Clips: %zu", m_clips.size());
    if (!m_clips.empty()) {
        int sel = m_currentClip;
        std::vector<const char*> names;
        names.reserve(m_clips.size());
        for (auto& c : m_clips) names.push_back(c.name.c_str());
        if (ImGui::Combo("Current Clip", &sel, names.data(), (int)names.size())) {
            Play(sel);
        }
    }
    ImGui::Checkbox("Loop", &m_loop);
    ImGui::SliderFloat("Speed", &m_speed, 0.05f, 3.0f, "%.2f");
    if (m_currentClip >= 0) {
        float dur = m_clips[m_currentClip].duration;
        ImGui::Text("Time %.3f / %.3f", m_time, dur);
        float scrub = m_time;
        if (ImGui::SliderFloat("Scrub", &scrub, 0.f, dur)) {
            m_time = scrub;
            BuildPose(m_time, m_currentClip, m_bonePalette);
            ApplyToRenderer();
        }
        float prog = (dur > 0) ? (m_time / dur) : 0.f;
        ImGui::ProgressBar(prog, ImVec2(-1, 0));
    }

    ImGui::Separator();
    if (ImGui::Button(m_paused ? "Resume" : "Pause")) {
        if (m_paused) Resume();
        else Pause();
    } ImGui::SameLine();
    if (ImGui::Button("Play")) {
        if (m_currentClip < 0 && !m_clips.empty()) Play(0);
        else Restart();
    } ImGui::SameLine();
    if (ImGui::Button("Stop")) Stop();

    if (m_currentClip >= 0) {
        ImGui::Text("Blend To:");
        for (int i = 0; i < (int)m_clips.size(); ++i) {
            if (i == m_currentClip) continue;
            ImGui::PushID(i);
            if (ImGui::SmallButton(m_clips[i].name.c_str())) {
                StartBlend(i, m_blendDuration);
            }
            ImGui::PopID();
        }
        ImGui::SliderFloat("Blend Duration", &m_blendDuration, 0.f, 2.f, "%.2f");
    }

    ImGui::Separator();
    ImGui::Checkbox("TransposeForShader(mul(pos,M)互換)", &m_debugTransposeForShader);
    ImGui::Checkbox("Auto BindPose Check Each Frame", &m_debugAutoBindCheck);
    if (ImGui::Button("Dump BoneMap Once")) m_debugShowBoneMapOnce = true;
    ImGui::SameLine();
    if (ImGui::Button("Validate BindPose Once")) m_debugValidateBindPoseOnce = true;
    ImGui::SameLine();
    if (ImGui::Button("Check Palette Validity Once")) m_debugCheckPaletteValidityOnce = true;

    if (m_debugAutoBindCheck && m_currentClip >= 0) {
        // 毎フレーム軽いチェック
        std::vector<XMFLOAT4X4> test;
        BuildPose(0.f, m_currentClip, test);
        int nonIdentity = 0;
        for (auto& m : test) {
            float d = fabs(m._11 - 1) + fabs(m._22 - 1) + fabs(m._33 - 1) + fabs(m._12) + fabs(m._13) + fabs(m._14) +
                fabs(m._21) + fabs(m._23) + fabs(m._24) + fabs(m._31) + fabs(m._32) + fabs(m._34) +
                fabs(m._41) + fabs(m._42) + fabs(m._43);
            if (d > 1e-3f) ++nonIdentity;
        }
        ImGui::Text("BindPose NonIdentity: %d / %zu", nonIdentity, test.size());
    }
}