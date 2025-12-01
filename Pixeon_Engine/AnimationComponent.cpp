#define NOMINMAX
#include "AnimationComponent.h"
#include "AnimationDebug.h"
#include "EngineManager.h"
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
	return _Parent ? _Parent->GetComponent<ModelRenderComponent>() : nullptr;
}

std::shared_ptr<ModelSharedResource> AnimationComponent::GetResource() {
	return m_renderer ? ModelManager::Instance()->LoadOrGet(m_renderer->GetModelPath()) : nullptr;
}

void AnimationComponent::BeginPlay() {
	m_renderer = GetRenderer();
	if (!m_renderer) { ErrorLogger::Instance().LogError("Animation", "Renderer not found"); return; }
	m_resource = GetResource();
	if (!m_resource) { ErrorLogger::Instance().LogError("Animation", "Resource null"); return; }

	// クリップ構築（キー統合）
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
			std::set<float> times;
			for (auto& k : ch.positionKeys) times.insert(k.first);
			for (auto& k : ch.rotationKeys) times.insert(k.first);
			for (auto& k : ch.scaleKeys)    times.insert(k.first);

			for (float t : times) {
				BoneTransform bt;
				// 位置補間
				if (!ch.positionKeys.empty()) {
					auto it = std::lower_bound(
						ch.positionKeys.begin(), ch.positionKeys.end(),
						std::pair<float, DirectX::XMFLOAT3>(t, {}),
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

				// 回転補間
				if (!ch.rotationKeys.empty()) {
					auto it = std::lower_bound(
						ch.rotationKeys.begin(), ch.rotationKeys.end(),
						std::pair<float, DirectX::XMFLOAT4>(t, {}),
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

				// スケール補間
				if (!ch.scaleKeys.empty()) {
					auto it = std::lower_bound(
						ch.scaleKeys.begin(), ch.scaleKeys.end(),
						std::pair<float, DirectX::XMFLOAT3>(t, {}),
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

	// ボーン配列確保
	if (!m_resource->bones.empty()) {
		m_boneMatrices.assign(m_resource->bones.size(), DirectX::XMFLOAT4X4());
		for (auto& m : m_boneMatrices)
			XMStoreFloat4x4(&m, XMMatrixIdentity());
	}

	// デバッグ処理 (任意残存)
	QuickIntegrityReport(m_resource.get());
	DumpBoneChannelMapping(m_resource.get());
	int fixCount = RebindBoneNodeIndices(m_resource.get());
	if (fixCount > 0) {
		EditrGUI::GetInstance()->WriteLog("[BeginPlay] Fixed " + std::to_string(fixCount) + " bone mappings");
	}
	for (auto& rtClip : m_clips) {
		RebindChannelNodeIndices(m_resource.get(), rtClip);
	}
	QuickIntegrityReport(m_resource.get());

	int missing = 0;
	for (auto& b : m_resource->bones) if (b.nodeIndex < 0) ++missing;
	EditrGUI::GetInstance()->WriteLog("[Animation BeginPlay] clips=" + std::to_string(m_clips.size()) +
		" bones=" + std::to_string(m_resource->bones.size()) +
		" boneMissingNodeIndex=" + std::to_string(missing));

	m_nodeToBone.clear();
	for (size_t i = 0; i < m_resource->bones.size(); ++i)
		m_nodeToBone[m_resource->bones[i].name] = (int)i;
	Play();
}

void AnimationComponent::EditUpdate()
{
}

void AnimationComponent::InGameUpdate() {
	if (!m_playing || m_paused) return;
	if (m_currentClip < 0 || m_currentClip >= (int)m_clips.size()) return;

	float dt = EngineManager::GetInstance()->GetDeltaTime();
	dt = std::min(dt, 0.1f);
	UpdateAnimation(dt);
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

void AnimationComponent::RebuildBoneMatrices() {
	if (m_boneMatrices.empty()) return;
	BuildClipPose(m_currentClip, m_time, m_boneMatrices);
}

DirectX::XMMATRIX AnimationComponent::BuildMatrix(const BoneTransform& bt) const {
	XMVECTOR S = XMLoadFloat3(&bt.scale);
	XMVECTOR R = XMLoadFloat4(&bt.rotation);
	XMVECTOR P = XMLoadFloat3(&bt.position);
	R = XMQuaternionNormalize(R);
	return XMMatrixScalingFromVector(S) * XMMatrixRotationQuaternion(R) * XMMatrixTranslationFromVector(P);
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
	// 位置
	r.position.x = prev->second.position.x + (next->second.position.x - prev->second.position.x) * f;
	r.position.y = prev->second.position.y + (next->second.position.y - prev->second.position.y) * f;
	r.position.z = prev->second.position.z + (next->second.position.z - prev->second.position.z) * f;
	// 回転
	XMVECTOR qa = XMLoadFloat4(&prev->second.rotation);
	XMVECTOR qb = XMLoadFloat4(&next->second.rotation);
	XMVECTOR q = XMQuaternionNormalize(XMQuaternionSlerp(qa, qb, f));
	XMStoreFloat4(&r.rotation, q);
	// スケール
	r.scale.x = prev->second.scale.x + (next->second.scale.x - prev->second.scale.x) * f;
	r.scale.y = prev->second.scale.y + (next->second.scale.y - prev->second.scale.y) * f;
	r.scale.z = prev->second.scale.z + (next->second.scale.z - prev->second.scale.z) * f;
	r.isValid = true;
	return r;
}

void AnimationComponent::BuildClipPose(int clipIndex, float time,
	std::vector<DirectX::XMFLOAT4X4>& outFinal)
{
	if (clipIndex < 0 || clipIndex >= (int)m_clips.size()) return;
	if (!m_resource) return;

	static bool dumpedNodes = false;
	if (!dumpedNodes)
	{
		dumpedNodes = true;

		const AnimationClip& clip = m_resource->clips[clipIndex];
		std::string text = "Node Hierarchy:\n";
		for (size_t i = 0; i < clip.nodeHierarchy.size(); ++i)
		{
			static int cnt = 0;
			auto& n = clip.nodeHierarchy[i];
			text += std::to_string(i) + ": " + n.name +
				" (parent=" + std::to_string(n.parentIndex) + ")";
			if (cnt > 3)
			{
				text += "\n";
			}
			cnt++;
		}
	}

	auto& clipRuntime = m_clips[clipIndex];
	auto& sourceClip = m_resource->clips[clipIndex];
	size_t nodeCount = sourceClip.nodeHierarchy.size();

	std::vector<DirectX::XMMATRIX> local(nodeCount, DirectX::XMMatrixIdentity());
	for (size_t i = 0; i < nodeCount; ++i)
		local[i] = sourceClip.nodeHierarchy[i].localTransform;

	for (auto& ch : clipRuntime.channels) {
		if (ch.nodeIndex < 0 || ch.nodeIndex >= (int)nodeCount) continue;
		BoneTransform bt = InterpChannel(ch, time);
		local[ch.nodeIndex] = BuildMatrix(bt);
	}

	// 正常プロジェクト互換: globalChild = localChild * globalParent
	std::vector<DirectX::XMMATRIX> global(nodeCount, DirectX::XMMatrixIdentity());
	for (size_t i = 0; i < nodeCount; ++i) {
		int parent = sourceClip.nodeHierarchy[i].parentIndex;
		if (parent >= 0)
			global[i] = local[i] * global[parent];
		else
			global[i] = local[i];
	}

	if (outFinal.size() != m_resource->bones.size())
		outFinal.resize(m_resource->bones.size());
	for (auto& f : outFinal)
		DirectX::XMStoreFloat4x4(&f, DirectX::XMMatrixIdentity());

	int applied = 0;
	for (size_t b = 0; b < m_resource->bones.size(); ++b) {
		const auto& bone = m_resource->bones[b];
		int nodeIdx = bone.nodeIndex;
		if (nodeIdx < 0 || nodeIdx >= (int)global.size()) continue;

		DirectX::XMMATRIX finalMat = bone.offset * global[nodeIdx];
		if (IsValidMatrix(finalMat)) {
			XMStoreFloat4x4(&outFinal[b], finalMat);
			++applied;
		}
		static bool shownOnce = false;
		if (!shownOnce && (b == 5 || b == 6 || b == 7))
		{
			DirectX::XMFLOAT4X4 gm, om, fm;
			DirectX::XMStoreFloat4x4(&gm, global[nodeIdx]);
			DirectX::XMStoreFloat4x4(&om, bone.offset);
			DirectX::XMStoreFloat4x4(&fm, finalMat);
		}
	}

	static int poseCounter = 0;
	if (++poseCounter % 240 == 0) {
		if (!outFinal.empty()) {
			auto& m = outFinal[0];
		}
	}
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
	std::string title = _ComponentName + "##" + std::to_string(reinterpret_cast<uintptr_t>(this));
	if (!ImGui::CollapsingHeader(title.c_str()))
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
}

void AnimationComponent::Play() {
	if (m_currentClip < 0 && !m_clips.empty()) m_currentClip = 0;
	if (m_currentClip < 0) return;
	m_playing = true; m_paused = false;
}

void AnimationComponent::Pause() { m_paused = true; }

void AnimationComponent::Resume() { m_paused = false; }

void AnimationComponent::Stop() { m_playing = false; m_paused = false; m_time = 0.f; }

void AnimationComponent::Restart() { m_time = 0.f; m_playing = true; m_paused = false; }

bool AnimationComponent::SetAnimationClip(int clipIndex) {
	if (clipIndex < 0 || clipIndex >= (int)m_clips.size()) return false;
	m_currentClip = clipIndex; m_time = 0.f;
	return true;
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

void AnimationComponent::SaveToFile(std::ostream& out) {
	out << m_currentClip << "\n"
		<< (m_loop ? 1 : 0) << "\n"
		<< m_speed << "\n";
}

void AnimationComponent::LoadFromFile(std::istream& in) {
	in >> m_currentClip;
	int loop; in >> loop; m_loop = loop != 0;
	in >> m_speed;
}