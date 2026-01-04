#define NOMINMAX
#include "AnimationComponent.h"
#include "MainFrame.h"
#include "GUI.h"
#include "IMGUI/imgui.h"
#include "AnimationDebug.h"
#include "AssetManager.h"
#include <algorithm>
#include <set>
#include <cmath>
#include <limits>

using namespace DirectX;

static std::string GetFileStem(const std::string& path) {
	size_t pos = path.find_last_of("/\\");
	std::string name = (pos == std::string::npos) ? path : path.substr(pos + 1);
	size_t dot = name.find_last_of('.');
	if (dot != std::string::npos) name = name.substr(0, dot);
	return name;
}

static std::vector<std::string> GetFBXListFromModelManager() {
	return AssetManager::Instance()->GetCachedAssetNames(true);
}

void AnimationComponent::Init(AbstractObject* owner) {
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

void AnimationComponent::EnsureLinked() {
	// ModelRender の取得/変更検知
	ModelRenderComponent* r = GetRenderer();
	bool modelChanged = (r != m_renderer);
	m_renderer = r;

	// モデルパスの変更検知
	std::string newPath;
	if (m_renderer) newPath = m_renderer->GetModelPath();
	if (newPath != m_linkedModelPath) {
		m_linkedModelPath = newPath;
		modelChanged = true;
	}

	if (!m_renderer || m_linkedModelPath.empty()) return;

	// 変更があれば ModelSharedResource を更新
	if (modelChanged || !m_resource) {
		m_resource = GetResource();
		if (!m_resource) return;

		// ボーン配列をモデルに合わせて確保
		m_boneMatrices.assign(m_resource->bones.size(), DirectX::XMFLOAT4X4());
		for (auto& m : m_boneMatrices) XMStoreFloat4x4(&m, XMMatrixIdentity());

		// デバッグ＆整合（必要ならログ）
		QuickIntegrityReport(m_resource.get());
		DumpBoneChannelMapping(m_resource.get());
		RebindBoneNodeIndices(m_resource.get());
		QuickIntegrityReport(m_resource.get());

		// スケルトンは 0 を基準（存在チェック）
		m_skeletonClipIndex = 0;
		if (m_skeletonClipIndex < 0 || m_skeletonClipIndex >= (int)m_resource->clips.size())
			return;

		// ノード名→ボーンIndex
		m_nodeToBone.clear();
		for (size_t i = 0; i < m_resource->bones.size(); ++i)
			m_nodeToBone[m_resource->bones[i].name] = (int)i;

		// 統合クリップ再構築
		RebuildAnimationClips();

		// 範囲補正
		if (m_currentClip >= (int)m_clips.size()) m_currentClip = (int)m_clips.size() - 1;
		if (m_currentClip < 0 && !m_clips.empty()) m_currentClip = 0;
	}

	// クリップ未構築なら再構築
	if (m_clips.empty() && m_resource) {
		RebuildAnimationClips();
		if (m_currentClip < 0 && !m_clips.empty()) m_currentClip = 0;
	}
}

void AnimationComponent::BeginPlay() {
	EnsureLinked();
	if (!m_resource) return;
	Play();
}

void AnimationComponent::EditUpdate()
{
	// エディタモードでもリンクを維持・追従
	EnsureLinked();
	if (!m_resource) return;

	// エディタでも時間更新（再生中のみ）
	if (m_playing && !m_paused) {
		float dt = MainFrame::GetInstance()->GetDeltaTime();
		dt = std::min(dt, 0.1f);
		UpdateAnimation(dt);
	}

	// 再生停止中でもスクラブのポーズを反映
	if (m_currentClip >= 0 && m_currentClip < (int)m_clips.size()) {
		RebuildBoneMatrices();
		ApplyToModel();
	}
}

void AnimationComponent::InGameUpdate() {
	if (!m_playing || m_paused) return;
	if (m_currentClip < 0 || m_currentClip >= (int)m_clips.size()) return;

	float dt = MainFrame::GetInstance()->GetDeltaTime();
	dt = std::min(dt, 0.1f);
	UpdateAnimation(dt);
	RebuildBoneMatrices();
	ApplyToModel();
}

void AnimationComponent::UpdateAnimation(float dt) {
	if (m_currentClip < 0 || m_currentClip >= (int)m_clips.size()) return;
	auto& clip = m_clips[m_currentClip];
	m_time += dt * m_speed;
	if (m_time >= clip.duration) {
		if (m_loop) m_time = fmod(m_time, (float)clip.duration);
		else { m_time = (float)clip.duration; m_playing = false; }
	}
}

void AnimationComponent::RebuildBoneMatrices() {
	if (m_boneMatrices.empty()) return;
	if (m_currentClip < 0 || m_currentClip >= (int)m_clips.size()) return;
	BuildClipPose(m_clips[m_currentClip], m_time, m_boneMatrices);
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

void AnimationComponent::BuildClipPose(const AnimationClipRuntime& rtClip, float time,
	std::vector<DirectX::XMFLOAT4X4>& outFinal)
{
	if (!m_resource) return;

	if (!rtClip.isLoaded && m_currentClip >= 0) {
		const_cast<AnimationComponent*>(this)->LazyLoadClip(m_currentClip);
	}

	if (m_skeletonClipIndex < 0 || m_skeletonClipIndex >= (int)m_resource->clips.size()) return;

	// スケルトンはモデル側（固定）
	const auto& skeleton = m_resource->clips[m_skeletonClipIndex].nodeHierarchy;
	size_t nodeCount = skeleton.size();

	std::vector<DirectX::XMMATRIX> local(nodeCount, DirectX::XMMatrixIdentity());
	for (size_t i = 0; i < nodeCount; ++i)
		local[i] = skeleton[i].localTransform;

	// クリップのキーを適用（nodeIndex はモデル側スケルトンに既にリバインド済み）
	for (auto& ch : rtClip.channels) {
		if (ch.nodeIndex < 0 || ch.nodeIndex >= (int)nodeCount) continue;
		BoneTransform bt = InterpChannel(ch, time);
		local[ch.nodeIndex] = BuildMatrix(bt);
	}

	// global = local * global(parent)
	std::vector<DirectX::XMMATRIX> global(nodeCount, DirectX::XMMatrixIdentity());
	for (size_t i = 0; i < nodeCount; ++i) {
		int parent = skeleton[i].parentIndex;
		if (parent >= 0)
			global[i] = local[i] * global[parent];
		else
			global[i] = local[i];
	}

	if (outFinal.size() != m_resource->bones.size())
		outFinal.resize(m_resource->bones.size());
	for (auto& f : outFinal)
		DirectX::XMStoreFloat4x4(&f, DirectX::XMMatrixIdentity());

	for (size_t b = 0; b < m_resource->bones.size(); ++b) {
		const auto& bone = m_resource->bones[b];
		int nodeIdx = bone.nodeIndex;
		if (nodeIdx < 0 || nodeIdx >= (int)global.size()) continue;

		DirectX::XMMATRIX finalMat = bone.offset * global[nodeIdx];
		if (IsValidMatrix(finalMat)) {
			XMStoreFloat4x4(&outFinal[b], finalMat);
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

	// クリップ名キャッシュの更新
	if (m_clipNamesNeedUpdate) {
		m_clipNamesCache.clear();
		m_clipNamePtrs.clear();
		m_clipNamesCache.reserve(m_clips.size());
		m_clipNamePtrs.reserve(m_clips.size());

		for (auto& c : m_clips) {
			m_clipNamesCache.push_back(c.name);
		}
		for (auto& name : m_clipNamesCache) {
			m_clipNamePtrs.push_back(name.c_str());
		}
		m_clipNamesNeedUpdate = false;
	}

	// クリップ数と選択
	ImGui::Text("Clips: %zu", m_clips.size());
	if (!m_clips.empty()) {
		int idx = m_currentClip;
		if (ImGui::Combo("Current Clip", &idx, m_clipNamePtrs.data(), (int)m_clipNamePtrs.size())) {
			SetAnimationClip(idx);
		}
	}

	// 再生操作
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

	// 外部アニメFBX管理UI
	ImGui::Separator();
	ImGui::Text("External FBX Animation Files:");

	// 折りたたみ可能にして描画負荷を軽減
	if (ImGui::TreeNode("Loaded Files")) {
		for (int i = 0; i < (int)m_externalAnimationFiles.size(); ++i) {
			ImGui::BulletText("[%d] %s", i, m_externalAnimationFiles[i].c_str());
			ImGui::SameLine();
			std::string btnId = "Remove##ext" + std::to_string(i);
			if (ImGui::SmallButton(btnId.c_str())) {
				m_externalAnimationFiles.erase(m_externalAnimationFiles.begin() + i);
				RebuildAnimationClips();
				if (m_currentClip >= (int)m_clips.size()) m_currentClip = (int)m_clips.size() - 1;
				break;
			}
		}
		ImGui::TreePop();
	}

	if (ImGui::Button("Add From Model Cache")) {
		m_cachedFBXList = GetFBXListFromModelManager();
		m_filterBuffer[0] = '\0';
		m_highlightIndex = -1;
		m_filteredIndices.clear();
		ImGui::OpenPopup("AnimFBXSelectPopup");
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload Animations")) {
		RebuildAnimationClips();
	}

	// FBX選択ポップアップ
	if (ImGui::BeginPopupModal("AnimFBXSelectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		bool filterChanged = ImGui::InputText("Filter", m_filterBuffer, sizeof(m_filterBuffer));

		if (filterChanged || m_filteredIndices.empty()) {
			m_filteredIndices.clear();
			std::string filterStr(m_filterBuffer);
			for (int i = 0; i < (int)m_cachedFBXList.size(); ++i) {
				if (filterStr.empty() || m_cachedFBXList[i].find(filterStr) != std::string::npos) {
					m_filteredIndices.push_back(i);
				}
			}
			if (filterChanged) m_highlightIndex = -1;
		}

		ImGui::Text("Count: %zu / %zu", m_filteredIndices.size(), m_cachedFBXList.size());
		ImGui::Separator();

		ImGui::BeginChild("AnimFBXSelectList", ImVec2(420, 320), true);
		for (int i = 0; i < (int)m_filteredIndices.size(); ++i) {
			int actualIndex = m_filteredIndices[i];
			const std::string& rawName = m_cachedFBXList[actualIndex];
			bool selected = (m_highlightIndex == actualIndex);

			if (ImGui::Selectable(rawName.c_str(), selected)) {
				m_highlightIndex = actualIndex;
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					AddAnimationFile(rawName);
					m_cachedFBXList.clear();
					m_filteredIndices.clear();
					ImGui::CloseCurrentPopup();
				}
			}
		}
		ImGui::EndChild();

		ImGui::Separator();
		if (ImGui::Button("Add")) {
			if (m_highlightIndex >= 0 && m_highlightIndex < (int)m_cachedFBXList.size()) {
				AddAnimationFile(m_cachedFBXList[m_highlightIndex]);
			}
			m_cachedFBXList.clear();
			m_filteredIndices.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			m_cachedFBXList.clear();
			m_filteredIndices.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// インデックス指定でのアニメ再生（折りたたみ可能）
	ImGui::Separator();
	if (ImGui::TreeNode("Advanced:  Play by Index")) {
		int extCount = GetExternalFileCount();
		if (extCount <= 0) {
			ImGui::TextUnformatted("No external FBX registered.");
		}
		else {
			if (m_extIdxUI < 0) m_extIdxUI = 0;
			if (m_extIdxUI >= extCount) m_extIdxUI = extCount - 1;

			int clipCount = GetClipCountInExternalFile(m_extIdxUI);
			if (clipCount < 0) clipCount = 0;
			if (m_clipIdxUI < 0) m_clipIdxUI = 0;
			if (m_clipIdxUI >= clipCount && clipCount > 0) m_clipIdxUI = clipCount - 1;

			ImGui::InputInt("External File Index", &m_extIdxUI);
			ImGui::InputInt("Clip Index In File", &m_clipIdxUI);
			ImGui::Text("ExternalCount=%d, ClipCount=%d", extCount, clipCount);

			if (ImGui::Button("Set Animation by Indices")) {
				if (SetAnimationByExternalIndex(m_extIdxUI, m_clipIdxUI)) {
					Play();
				}
			}
		}
		ImGui::TreePop();
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

	LazyLoadClip(clipIndex);

	m_currentClip = clipIndex;
	m_time = 0.f;
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

	// 外部FBXリスト
	out << m_externalAnimationFiles.size() << "\n";
	for (auto& p : m_externalAnimationFiles) {
		out << p << "\n";
	}
}

void AnimationComponent::LoadFromFile(std::istream& in) {
	in >> m_currentClip;
	int loop; in >> loop; m_loop = loop != 0;
	in >> m_speed;

	size_t count = 0;
	if (in >> count) {
		in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		m_externalAnimationFiles.clear();
		for (size_t i = 0; i < count; ++i) {
			std::string line;
			std::getline(in, line);
			if (!line.empty()) m_externalAnimationFiles.push_back(line);
		}
	}
	else {
		in.clear();
	}
}

void AnimationComponent::AddAnimationFile(const std::string& path) {
	if (std::find(m_externalAnimationFiles.begin(), m_externalAnimationFiles.end(), path) == m_externalAnimationFiles.end()) {
		m_externalAnimationFiles.push_back(path);
		RebuildAnimationClips();
	}
}

void AnimationComponent::ClearAnimationFiles() {
	m_externalAnimationFiles.clear();
	RebuildAnimationClips();
}

int AnimationComponent::GetClipCountInExternalFile(int externalFileIndex) const {
	if (externalFileIndex < 0 || externalFileIndex >= (int)m_externalAnimationFiles.size()) return 0;
	auto extRes = ModelManager::Instance()->LoadOrGet(m_externalAnimationFiles[externalFileIndex]);
	return extRes ? (int)extRes->clips.size() : 0;
}

bool AnimationComponent::SetAnimationByExternalIndex(int externalFileIndex, int clipIndexInFile) {
	if (externalFileIndex < 0 || externalFileIndex >= (int)m_externalAnimationFiles.size()) return false;
	if (clipIndexInFile < 0) return false;

	for (int i = 0; i < (int)m_clipOrigins.size(); ++i) {
		const auto& o = m_clipOrigins[i];
		if (o.source == ClipOrigin::Source::External &&
			o.externalFileIndex == externalFileIndex &&
			o.clipIndexInSource == clipIndexInFile) {
			return SetAnimationClip(i);
		}
	}
	return false;
}

void AnimationComponent::RebuildAnimationClips() {
	if (!m_resource) return;

	auto buildClipMetadata = [&](const std::shared_ptr<ModelSharedResource>& res,
		const std::string& prefix,
		ClipOrigin::Source src,
		int externalFileIndex)
		{
			if (!res) return;
			for (int ci = 0; ci < (int)res->clips.size(); ++ci) {
				const auto& c = res->clips[ci];

				// メタデータのみ構築（実データは遅延ロード）
				AnimationClipRuntime rt;
				rt.name = prefix.empty() ? c.name : (prefix + ":" + c.name);
				rt.duration = c.duration / c.tps;
				rt.tps = c.tps;
				rt.isLoaded = false;  // ← まだロードしていない

				ClipOrigin origin;
				origin.source = src;
				origin.externalFileIndex = (src == ClipOrigin::Source::External) ? externalFileIndex : -1;
				origin.clipIndexInSource = ci;

				m_clips.push_back(rt);
				m_clipOrigins.push_back(origin);
			}
		};

	int prevIndex = m_currentClip;
	m_clips.clear();
	m_clipOrigins.clear();

	// モデル内クリップ（メタデータのみ）
	buildClipMetadata(m_resource, "", ClipOrigin::Source::Model, -1);

	// 外部アニメFBX（メタデータのみ）
	for (int fi = 0; fi < (int)m_externalAnimationFiles.size(); ++fi) {
		const auto& path = m_externalAnimationFiles[fi];
		auto extRes = ModelManager::Instance()->LoadOrGet(path);
		std::string prefix = GetFileStem(path);
		buildClipMetadata(extRes, prefix, ClipOrigin::Source::External, fi);
	}

	// インデックスの整合
	if (m_clips.empty()) {
		m_currentClip = -1;
	}
	else {
		if (prevIndex < 0) prevIndex = 0;
		if (prevIndex >= (int)m_clips.size()) prevIndex = (int)m_clips.size() - 1;
		m_currentClip = prevIndex;
	}

	// キャッシュ無効化
	m_clipNamesNeedUpdate = true;
}

void AnimationComponent::LazyLoadClip(int clipIndex) {
	if (clipIndex < 0 || clipIndex >= (int)m_clips.size()) return;
	auto& rt = m_clips[clipIndex];

	// 既にロード済み
	if (rt.isLoaded) return;

	// ロード元リソースを取得
	const auto& origin = m_clipOrigins[clipIndex];
	std::shared_ptr<ModelSharedResource> sourceRes;

	if (origin.source == ClipOrigin::Source::Model) {
		sourceRes = m_resource;
	}
	else {
		if (origin.externalFileIndex >= 0 && origin.externalFileIndex < (int)m_externalAnimationFiles.size()) {
			sourceRes = ModelManager::Instance()->LoadOrGet(m_externalAnimationFiles[origin.externalFileIndex]);
		}
	}

	if (!sourceRes || origin.clipIndexInSource < 0 || origin.clipIndexInSource >= (int)sourceRes->clips.size()) {
		return;
	}

	const auto& c = sourceRes->clips[origin.clipIndexInSource];

	// チャンネルデータを構築
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

	// ノードインデックスを再バインド
	RebindChannelNodeIndices(m_resource.get(), rt);

	rt.isLoaded = true;
}