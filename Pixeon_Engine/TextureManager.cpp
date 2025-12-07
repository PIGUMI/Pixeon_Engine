#include "TextureManager.h"
#include "AssetManager.h"
#include "System.h"
#include "DirectXTex/DirectXTex.h"
#include "IMGUI/imgui.h"
#include <algorithm>
#include <Windows.h>

TextureManager* TextureManager::s_instance = nullptr;

TextureManager* TextureManager::Instance() {
	if (!s_instance) s_instance = new TextureManager();
	return s_instance;
}

void TextureManager::DeleteInstance() {
	if (s_instance) {
		s_instance->UnInit();
		delete s_instance;
		s_instance = nullptr;
	}
}

void TextureManager::UnInit() {
	std::lock_guard<std::mutex> lk(m_mtx);
	m_cache.clear();
	m_pinned.clear();
	m_failReasons.clear();
	m_frame = 0;
}

void TextureManager::SetFail(const std::string& name, const std::string& reason) {
	m_failReasons[name] = reason;
}
std::string TextureManager::GetLastFailReason(const std::string& name) const {
	auto it = m_failReasons.find(name);
	return it == m_failReasons.end() ? "" : it->second;
}

std::shared_ptr<TextureResource> TextureManager::LoadOrGet(const std::string& logicalName) {
	std::lock_guard<std::mutex> lk(m_mtx);
	m_frame++;
	auto it = m_cache.find(logicalName);
	if (it != m_cache.end()) {
		if (auto sp = it->second.weak.lock()) {
			it->second.lastUse = m_frame;
			return sp;
		}
	}
	auto tex = LoadInternal(logicalName);
	if (tex) {
		Entry e;
		e.weak = tex;
		e.lastUse = m_frame;
		e.bytes = tex->gpuBytes;
		m_cache[logicalName] = e;
		m_failReasons.erase(logicalName);
	}
	return tex;
}

bool TextureManager::LoadTexture(const std::string& name) {
	auto tex = LoadOrGet(name);
	if (tex && tex->srv) {
		std::lock_guard<std::mutex> lk(m_mtx);
		m_pinned[name] = tex;
		return true;
	}
	return false;
}

bool TextureManager::Reload(const std::string& name) {
	{
		std::lock_guard<std::mutex> lk(m_mtx);
		m_cache.erase(name);
		m_pinned.erase(name);
		m_failReasons.erase(name);
	}
	return LoadTexture(name);
}

bool TextureManager::RemoveFromCache(const std::string& name) {
	std::lock_guard<std::mutex> lk(m_mtx);
	m_cache.erase(name);
	m_pinned.erase(name);
	m_failReasons.erase(name);
	return true;
}

bool TextureManager::IsLoaded(const std::string& name) {
	std::lock_guard<std::mutex> lk(m_mtx);
	auto it = m_cache.find(name);
	return (it != m_cache.end() && !it->second.weak.expired());
}

bool TextureManager::Pin(const std::string& name) {
	std::lock_guard<std::mutex> lk(m_mtx);
	auto it = m_cache.find(name);
	if (it == m_cache.end()) return false;
	if (auto sp = it->second.weak.lock()) {
		m_pinned[name] = sp;
		return true;
	}
	return false;
}

bool TextureManager::Unpin(const std::string& name) {
	std::lock_guard<std::mutex> lk(m_mtx);
	return m_pinned.erase(name) > 0;
}

bool TextureManager::IsPinned(const std::string& name) {
	std::lock_guard<std::mutex> lk(m_mtx);
	return m_pinned.find(name) != m_pinned.end();
}

std::shared_ptr<TextureResource> TextureManager::LoadInternal(const std::string& logicalName) {
	// (1) Raw �ǂݍ���
	std::vector<uint8_t> data;
	if (!AssetManager::Instance()->LoadAsset(logicalName, data) || data.empty()) {
		SetFail(logicalName, "RawLoadFailed(size=0 or not found)");
		return nullptr;
	}

	// (2) �g���q����
	std::string ext;
	if (auto p = logicalName.find_last_of('.'); p != std::string::npos) {
		ext = logicalName.substr(p + 1);
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
	}

	DirectX::ScratchImage img;
	HRESULT hr = E_FAIL;

	if (ext == "dds") {
		hr = DirectX::LoadFromDDSMemory(data.data(), data.size(), DirectX::DDS_FLAGS_NONE, nullptr, img);
	}
	else if (ext == "tga") {
		hr = DirectX::LoadFromTGAMemory(data.data(), data.size(), nullptr, img);
	}
	else if (ext == "hdr") {
		hr = DirectX::LoadFromHDRMemory(data.data(), data.size(), nullptr, img);
	}
	else {
		hr = DirectX::LoadFromWICMemory(data.data(), data.size(), DirectX::WIC_FLAGS_NONE, nullptr, img);
	}
	if (FAILED(hr)) {
		// ヘッダバイト情報を含めた詳細なエラーレポート（デバッグ用）
		char buf[256];
		char headerHex[32] = "";
		if (data.size() >= 8) {
			sprintf_s(headerHex, "%02X%02X%02X%02X%02X%02X%02X%02X",
				data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
		}
		sprintf_s(buf, "DecodeFailed hr=0x%08X ext=%s header=%s size=%zu",
			(unsigned)hr, ext.c_str(), headerHex, data.size());
		SetFail(logicalName, buf);
		return nullptr;
	}

	// (3) Mip ���� (���s�͌x���̂�)
	if (img.GetMetadata().mipLevels <= 1) {
		DirectX::ScratchImage mip;
		HRESULT hrMip = DirectX::GenerateMipMaps(img.GetImages(), img.GetImageCount(), img.GetMetadata(),
			DirectX::TEX_FILTER_DEFAULT, 0, mip);
		if (SUCCEEDED(hrMip)) {
			img = std::move(mip);
		}
	}

	// (4) �f�o�C�X�m�F
	auto dev = DirectX11::GetInstance()->GetDevice();
	if (!dev) {
		SetFail(logicalName, "DeviceNull");
		return nullptr;
	}

	// (5) SRV �쐬
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	hr = DirectX::CreateShaderResourceView(dev, img.GetImages(), img.GetImageCount(),
		img.GetMetadata(), srv.GetAddressOf());
	if (FAILED(hr)) {
		char buf[128];
		sprintf_s(buf, "CreateSRVFailed hr=0x%08X", (unsigned)hr);
		SetFail(logicalName, buf);
		return nullptr;
	}

	auto tex = std::make_shared<TextureResource>();
	tex->name = logicalName;
	tex->srv = srv;
	tex->width = (uint32_t)img.GetMetadata().width;
	tex->height = (uint32_t)img.GetMetadata().height;
	tex->gpuBytes = tex->width * tex->height * 4ull;

	OutputDebugStringA(("[TextureManager] Load OK: " + logicalName + "\n").c_str());
	return tex;
}

void TextureManager::GarbageCollect() {
	std::lock_guard<std::mutex> lk(m_mtx);
	for (auto it = m_cache.begin(); it != m_cache.end();) {
		if (it->second.weak.expired() && m_pinned.find(it->first) == m_pinned.end()) {
			it = m_cache.erase(it);
		}
		else {
			++it;
		}
	}
}

// GUI
void TextureManager::DrawDebugGUI() {
	std::lock_guard<std::mutex> lk(m_mtx);
	ImGui::TextUnformatted("TextureManager");
	ImGui::Separator();

	size_t alive = 0;
	size_t totalBytes = 0;
	for (auto& kv : m_cache) {
		if (auto sp = kv.second.weak.lock()) {
			alive++;
			totalBytes += kv.second.bytes;
		}
	}
	ImGui::Text("Cached Entries: %zu (alive=%zu)", m_cache.size(), alive);
	ImGui::Text("Pinned: %zu", m_pinned.size());
	ImGui::Text("GPU Approx Total: %.2f MB", totalBytes / (1024.0 * 1024.0));
	ImGui::Text("Memory Budget: %.2f MB", m_budget / (1024.0 * 1024.0));

	if (ImGui::Button("GC (Dead Only)")) {
		for (auto it = m_cache.begin(); it != m_cache.end();) {
			if (it->second.weak.expired() && m_pinned.find(it->first) == m_pinned.end())
				it = m_cache.erase(it);
			else
				++it;
		}
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Cached List:");
	static char filterCache[128] = "";
	ImGui::InputText("Filter Cached", filterCache, sizeof(filterCache));
	ImGui::BeginChild("TM_CachedList", ImVec2(0, 110), true);
	for (auto& kv : m_cache) {
		if (filterCache[0] && kv.first.find(filterCache) == std::string::npos) continue;
		bool aliveOne = !kv.second.weak.expired();
		bool pinned = (m_pinned.find(kv.first) != m_pinned.end());
		ImGui::Text("%s | %s | %s",
			kv.first.c_str(),
			aliveOne ? "alive" : "dead",
			pinned ? "pinned" : "");
	}
	ImGui::EndChild();

	ImGui::Separator();
	ImGui::TextUnformatted("Asset Textures (Load / Pin)");
	static char filterAsset[128] = "";
	ImGui::InputText("Filter Assets", filterAsset, sizeof(filterAsset));
	auto assetTexList = AssetManager::Instance()->GetCachedTextureNames();

	int unloadedCount = 0;
	for (auto& n : assetTexList) {
		auto it = m_cache.find(n);
		if (it == m_cache.end() || it->second.weak.expired()) unloadedCount++;
	}
	ImGui::Text("Total Raw=%d  Loaded=%zu  Unloaded=%d  Failed=%zu",
		(int)assetTexList.size(), m_cache.size(), unloadedCount, m_failReasons.size());

	static int sel = -1;

	if (ImGui::Button("Load All Unloaded")) {
		for (auto& n : assetTexList) {
			auto it = m_cache.find(n);
			bool need = (it == m_cache.end()) || it->second.weak.expired();
			if (need) {
				auto tex = LoadInternal(n);
				if (tex) {
					Entry e;
					e.weak = tex;
					e.lastUse = ++m_frame;
					e.bytes = tex->gpuBytes;
					m_cache[n] = e;
					m_pinned[n] = tex;
					m_failReasons.erase(n);
				}
			}
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload Selected") && sel >= 0 && sel < (int)assetTexList.size()) {
		std::string name = assetTexList[sel];
		m_cache.erase(name);
		m_pinned.erase(name);
		m_failReasons.erase(name);
		auto tex = LoadInternal(name);
		if (tex) {
			Entry e;
			e.weak = tex;
			e.lastUse = ++m_frame;
			e.bytes = tex->gpuBytes;
			m_cache[name] = e;
			m_pinned[name] = tex;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Remove Selected") && sel >= 0 && sel < (int)assetTexList.size()) {
		std::string name = assetTexList[sel];
		m_cache.erase(name);
		m_pinned.erase(name);
		m_failReasons.erase(name);
	}

	ImGui::BeginChild("TM_AssetList", ImVec2(0, 210), true);
	for (int i = 0; i < (int)assetTexList.size(); ++i) {
		const std::string& n = assetTexList[i];
		if (filterAsset[0] && n.find(filterAsset) == std::string::npos) continue;
		bool loaded = (m_cache.find(n) != m_cache.end()) && !m_cache[n].weak.expired();
		bool pinned = (m_pinned.find(n) != m_pinned.end());
		bool failed = (m_failReasons.find(n) != m_failReasons.end());

		ImVec4 col;
		if (failed) col = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
		else if (pinned) col = ImVec4(0.5f, 0.9f, 1.0f, 1.0f);
		else if (loaded) col = ImVec4(0.6f, 0.9f, 0.6f, 1.0f);
		else col = ImVec4(0.95f, 0.85f, 0.4f, 1.0f);

		ImGui::PushStyleColor(ImGuiCol_Text, col);
		if (ImGui::Selectable(n.c_str(), sel == i))
			sel = i;
		ImGui::PopStyleColor();

		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			auto tex = LoadInternal(n);
			if (tex) {
				Entry e;
				e.weak = tex;
				e.lastUse = ++m_frame;
				e.bytes = tex->gpuBytes;
				m_cache[n] = e;
				m_pinned[n] = tex;
				m_failReasons.erase(n);
			}
		}
	}
	ImGui::EndChild();

	if (sel >= 0 && sel < (int)assetTexList.size()) {
		ImGui::Separator();
		std::string selName = assetTexList[sel];
		ImGui::Text("Selected: %s", selName.c_str());
		bool loaded = (m_cache.find(selName) != m_cache.end()) && !m_cache[selName].weak.expired();
		ImGui::Text("Loaded: %s  Pinned: %s", loaded ? "Yes" : "No",
			(m_pinned.find(selName) != m_pinned.end()) ? "Yes" : "No");
		auto fr = GetLastFailReason(selName);
		if (!fr.empty()) {
			ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "FailReason: %s", fr.c_str());
		}
		if (!loaded) {
			if (ImGui::Button("Load & Pin")) {
				auto tex = LoadInternal(selName);
				if (tex) {
					Entry e;
					e.weak = tex;
					e.lastUse = ++m_frame;
					e.bytes = tex->gpuBytes;
					m_cache[selName] = e;
					m_pinned[selName] = tex;
					m_failReasons.erase(selName);
				}
			}
		}
		else {
			if (ImGui::Button("Pin") && m_pinned.find(selName) == m_pinned.end()) {
				if (auto sp = m_cache[selName].weak.lock())
					m_pinned[selName] = sp;
			}
			ImGui::SameLine();
			if (ImGui::Button("Unpin")) {
				m_pinned.erase(selName);
			}
			ImGui::SameLine();
			if (ImGui::Button("Reload")) {
				m_cache.erase(selName);
				m_pinned.erase(selName);
				m_failReasons.erase(selName);
				auto tex = LoadInternal(selName);
				if (tex) {
					Entry e;
					e.weak = tex;
					e.lastUse = ++m_frame;
					e.bytes = tex->gpuBytes;
					m_cache[selName] = e;
					m_pinned[selName] = tex;
				}
			}
		}
	}
}