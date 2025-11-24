#include "SoundManager.h"
#include "AssetManager.h"
#include <Windows.h>
#include "IMGUI/imgui.h"

SoundManager* SoundManager::s_instance = nullptr;

SoundManager* SoundManager::Instance()
{
	if (!s_instance) {
		s_instance = new SoundManager();
	}
	return s_instance;
}

void SoundManager::DeleteInstance() {
	if (s_instance) {
		s_instance->UnInit();
		delete s_instance;
		s_instance = nullptr;
	}
}

void SoundManager::UnInit() {
	std::lock_guard<std::mutex> lk(m_mtx);
	m_cache.clear();
	m_frame = 0;
}

std::shared_ptr<SoundResource> SoundManager::LoadOrGet(const std::string& logicalName, bool streaming) {
	std::lock_guard<std::mutex> lk(m_mtx);
	m_frame++;
	auto it = m_cache.find(logicalName);
	if (it != m_cache.end()) {
		if (auto sp = it->second.weak.lock()) {
			it->second.lastUse = m_frame;
			return sp;
		}
	}
	auto snd = LoadInternal(logicalName, streaming);
	if (snd) {
		Entry e;
		e.weak = snd;
		e.lastUse = m_frame;
		e.bytes = snd->pcmData.size();
		e.streaming = streaming;
		m_cache[logicalName] = e;
	}
	return snd;
}

std::shared_ptr<SoundResource> SoundManager::LoadInternal(const std::string& logicalName, bool streaming) {
	std::vector<uint8_t> data;
	if (!AssetManager::Instance()->LoadAsset(logicalName, data) || data.empty()) {
		OutputDebugStringA(("[SoundManager] Raw load failed: " + logicalName + "\n").c_str());
		return nullptr;
	}
	auto snd = std::make_shared<SoundResource>();
	snd->name = logicalName;
	if (!streaming) {
		// 簡易: そのまま格納（実際は WAV パースしてヘッダ除去）
		snd->pcmData = data;
	}
	else {
		// ストリーミング: ヘッダ解析だけ行い PCM は都度読む設計へ拡張
	}
	return snd;
}

void SoundManager::GarbageCollect() {
	std::lock_guard<std::mutex> lk(m_mtx);
	for (auto it = m_cache.begin(); it != m_cache.end(); ) {
		if (it->second.weak.expired()) {
			it = m_cache.erase(it);
		}
		else {
			++it;
		}
	}
}

void SoundManager::DrawDebugGUI() {
	std::lock_guard<std::mutex> lk(m_mtx);
	ImGui::TextUnformatted("SoundManager");
	ImGui::Separator();
	size_t alive = 0;
	size_t total = 0;
	for (auto& kv : m_cache) {
		if (!kv.second.weak.expired()) {
			alive++;
			total += kv.second.bytes;
		}
	}
	ImGui::Text("Cached: %zu (alive=%zu)", m_cache.size(), alive);
	ImGui::Text("PCM Approx: %.2f MB", total / (1024.0 * 1024.0));
	static char filter[128] = "";
	ImGui::InputText("Filter##Sound", filter, sizeof(filter));
	if (ImGui::Button("GC Dead##Sound")) {
		for (auto it = m_cache.begin(); it != m_cache.end();) {
			if (it->second.weak.expired()) it = m_cache.erase(it);
			else ++it;
		}
	}
	ImGui::Separator();
	ImGui::BeginChild("SoundList", ImVec2(0, 160), true);
	for (auto& kv : m_cache) {
		if (filter[0] && kv.first.find(filter) == std::string::npos) continue;
		bool aliveRes = !kv.second.weak.expired();
		ImGui::Text("%s | %s | %zu bytes | %s",
			kv.first.c_str(),
			aliveRes ? "alive" : "dead",
			kv.second.bytes,
			kv.second.streaming ? "stream" : "full");
	}
	ImGui::EndChild();
}