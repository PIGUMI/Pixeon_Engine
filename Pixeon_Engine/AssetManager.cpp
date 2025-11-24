#include "AssetManager.h"
#include <fstream>
#include <Windows.h>
#include "IMGUI/imgui.h"
#include "ErrorLog.h"

AssetManager* AssetManager::s_instance_ = nullptr;

AssetManager* AssetManager::Instance()
{
	if (!s_instance_) {
		s_instance_ = new AssetManager();
	}
	return s_instance_;
}

void AssetManager::DeleteInstance()
{
	if (s_instance_) {
		s_instance_->UnInit();
		delete s_instance_;
		s_instance_ = nullptr;
	}
}

void AssetManager::UnInit()
{
	StopAutoSync();
	ClearRawCache();
	{
		std::lock_guard<std::mutex> lk(m_mtx_);
		m_recentChanges_.clear();
	}
}

AssetManager::~AssetManager() {
	StopAutoSync();
}

void AssetManager::SetRoot(const std::string& root) { m_root_ = root; }
void AssetManager::SetLoadMode(LoadMode m) { m_mode_ = m; }

std::string AssetManager::Normalize(const std::string& name) const {
	std::string s = name;
	for (auto& c : s) if (c == '\\') c = '/';
	return s;
}

bool AssetManager::Exists(const std::string& logicalName) {
	std::string norm = Normalize(logicalName);
	{
		std::lock_guard<std::mutex> lk(m_mtx_);
		if (m_cache_.find(norm) != m_cache_.end()) return true;
	}
	std::filesystem::path p = std::filesystem::path(m_root_) / norm;
	return std::filesystem::exists(p);
}

bool AssetManager::LoadAsset(const std::string& logicalName, std::vector<uint8_t>& outData) {
	std::string norm = Normalize(logicalName);
	{
		std::lock_guard<std::mutex> lk(m_mtx_);
		auto it = m_cache_.find(norm);
		if (it != m_cache_.end()) {
			outData = it->second;
			return true;
		}
	}

	std::filesystem::path p = std::filesystem::path(m_root_) / norm;
	std::ifstream ifs(p, std::ios::binary);
	if (!ifs) {
		ErrorLogger::Instance().LogError("AssetManager", "Failed to open asset: " + norm);
		return false;
	}
	ifs.seekg(0, std::ios::end);
	size_t sz = (size_t)ifs.tellg();
	ifs.seekg(0, std::ios::beg);
	outData.resize(sz);
	ifs.read((char*)outData.data(), sz);
	if (!ifs) {
		ErrorLogger::Instance().LogError("AssetManager", "Failed to read asset: " + norm);
		return false;
	}
	{
		std::lock_guard<std::mutex> lk(m_mtx_);
		m_cache_[norm] = outData;
	}
	return true;
}

void AssetManager::ClearRawCache() {
	std::lock_guard<std::mutex> lk(m_mtx_);
	m_cache_.clear();
	m_fileMeta_.clear();
}

void AssetManager::PushChange(ChangeType type, const std::string& path) {
	std::lock_guard<std::mutex> lk(m_mtx_);
	if (m_recentChanges_.size() >= kMaxRecentChanges_)
		m_recentChanges_.pop_front();
	m_recentChanges_.push_back({ type, path, m_scanCount_.load() });
}

void AssetManager::StartAutoSync(std::chrono::milliseconds interval, bool recursive) {
	if (m_watchRunning_.load()) return;
	if (m_root_.empty()) {
		ErrorLogger::Instance().LogError("AssetManager", "StartAutoSync failed: root not set.");
		return;
	}
	m_interval_ = interval;
	m_recursive_ = recursive;
	m_watchRunning_ = true;
	m_watchThread_ = std::thread(&AssetManager::WatchLoop, this);
	OutputDebugStringA("[AssetManager] AutoSync started.\n");
}

void AssetManager::StopAutoSync() {
	if (!m_watchRunning_.load()) return;
	m_watchRunning_ = false;
	if (m_watchThread_.joinable()) m_watchThread_.join();
	OutputDebugStringA("[AssetManager] AutoSync stopped.\n");
}

void AssetManager::WatchLoop() {
	PerformScan();
	while (m_watchRunning_.load()) {
		auto t0 = std::chrono::steady_clock::now();
		PerformScan();
		auto t1 = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
		m_lastScanDurationMs_.store((uint64_t)elapsed);
		std::this_thread::sleep_for(m_interval_);
	}
}

void AssetManager::PerformScan() {
	using namespace std::filesystem;

	std::unordered_map<std::string, FileMeta> current;
	std::vector<std::string> additions;
	std::vector<std::string> modifications;
	std::vector<std::string> deletions;

	const path rootPath(m_root_);
	if (!exists(rootPath)) {
		OutputDebugStringA("[AssetManager] Root path does not exist.\n");
		return;
	}

	auto scanStart = std::chrono::steady_clock::now();

	// ディレクトリ列挙
	std::error_code ec;
	if (m_recursive_) {
		for (recursive_directory_iterator it(rootPath, ec), end; it != end && !ec; ++it) {
			if (!it->is_regular_file()) continue;
			auto rel = relative(it->path(), rootPath, ec);
			if (ec) continue;
			std::string relStr = Normalize(rel.generic_string());
			FileMeta meta;
			meta.size = (uint64_t)file_size(it->path(), ec);
			meta.writeTime = last_write_time(it->path(), ec);
			current[relStr] = meta;
		}
	}
	else {
		for (directory_iterator it(rootPath, ec), end; it != end && !ec; ++it) {
			if (!it->is_regular_file()) continue;
			auto rel = relative(it->path(), rootPath, ec);
			if (ec) continue;
			std::string relStr = Normalize(rel.generic_string());
			FileMeta meta;
			meta.size = (uint64_t)file_size(it->path(), ec);
			meta.writeTime = last_write_time(it->path(), ec);
			current[relStr] = meta;
		}
	}

	// 差分判定
	{
		std::lock_guard<std::mutex> lk(m_mtx_);
		// 追加 / 変更
		for (auto& kv : current) {
			auto itOld = m_fileMeta_.find(kv.first);
			if (itOld == m_fileMeta_.end()) {
				additions.push_back(kv.first);
			}
			else {
				if (kv.second.size != itOld->second.size ||
					kv.second.writeTime != itOld->second.writeTime) {
					modifications.push_back(kv.first);
				}
			}
		}
		// 削除
		for (auto& old : m_fileMeta_) {
			if (current.find(old.first) == current.end()) {
				deletions.push_back(old.first);
			}
		}
	}

	// 追加/変更ファイルのロード（ロックを外した状態で IO）
	for (auto& add : additions) {
		std::vector<uint8_t> dummy;
		if (LoadAsset(add, dummy)) {
			PushChange(ChangeType::Added, add);
		}
		else {
			PushChange(ChangeType::ReloadFailed, add);
		}
	}
	for (auto& mod : modifications) {
		std::vector<uint8_t> dummy;
		if (LoadAsset(mod, dummy)) {
			PushChange(ChangeType::Modified, mod);
		}
		else {
			PushChange(ChangeType::ReloadFailed, mod);
		}
	}
	// 削除適用
	{
		std::lock_guard<std::mutex> lk(m_mtx_);
		for (auto& del : deletions) {
			m_cache_.erase(del);
			m_fileMeta_.erase(del);
			PushChange(ChangeType::Removed, del);
		}
		// ファイルメタ更新
		for (auto& kv : current) {
			m_fileMeta_[kv.first] = kv.second;
		}
	}

	m_lastDiffAdds_.store(additions.size());
	m_lastDiffMods_.store(modifications.size());
	m_lastDiffRemoves_.store(deletions.size());
	m_scanCount_.fetch_add(1);
}

void AssetManager::DrawDebugGUI()
{
	std::lock_guard<std::mutex> lk(m_mtx_);
	ImGui::TextUnformatted("AssetManager");
	ImGui::Separator();
	ImGui::Text("Root: %s", m_root_.c_str());
	ImGui::Text("Mode: %s", (m_mode_ == LoadMode::FromSource) ? "FromSource" : "FromArchive");
	ImGui::Text("Cached Raw Files: %zu", m_cache_.size());
	ImGui::Text("AutoSync: %s", m_watchRunning_.load() ? "Running" : "Stopped");
	ImGui::Text("ScanCount: %llu", (unsigned long long)m_scanCount_.load());
	ImGui::Text("LastDiff A=%llu M=%llu R=%llu",
		(unsigned long long)m_lastDiffAdds_.load(),
		(unsigned long long)m_lastDiffMods_.load(),
		(unsigned long long)m_lastDiffRemoves_.load());
	ImGui::Text("LastScanDuration: %llu ms", (unsigned long long)m_lastScanDurationMs_.load());

	static char filter[128] = "";
	ImGui::InputText("Filter (substring)", filter, sizeof(filter));

	if (ImGui::Button("Clear Raw Cache")) {
		m_cache_.clear();
		m_fileMeta_.clear();
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear Change Log")) {
		m_recentChanges_.clear();
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Recent Changes:");
	ImGui::BeginChild("AssetManagerChanges", ImVec2(0, 120), true);
	for (auto it = m_recentChanges_.rbegin(); it != m_recentChanges_.rend(); ++it) {
		const char* t = "";
		switch (it->type) {
		case ChangeType::Added: t = "ADD"; break;
		case ChangeType::Removed: t = "DEL"; break;
		case ChangeType::Modified: t = "MOD"; break;
		case ChangeType::ReloadFailed: t = "ERR"; break;
		}
		if (filter[0] && it->path.find(filter) == std::string::npos) continue;
		ImGui::Text("[%s] %s (scan=%llu)", t, it->path.c_str(), (unsigned long long)it->timestampFrame);
	}
	ImGui::EndChild();

	ImGui::Separator();
	ImGui::TextUnformatted("Cache Entries:");
	ImGui::BeginChild("AssetManagerCacheList", ImVec2(0, 160), true);
	for (auto& kv : m_cache_) {
		if (filter[0] && kv.first.find(filter) == std::string::npos) continue;
		ImGui::Text("%s (size=%zu bytes)", kv.first.c_str(), kv.second.size());
	}
	ImGui::EndChild();
}

std::vector<std::string> AssetManager::GetCachedAssetNames(bool onlyModelExt) const {
	std::vector<std::string> result;
	{
		std::lock_guard<std::mutex> lk(m_mtx_);
		result.reserve(m_cache_.size());
		for (auto& kv : m_cache_) {
			if (!onlyModelExt) {
				result.push_back(kv.first);
			}
			else {
				std::string lower = kv.first;
				for (auto& c : lower) c = (char)tolower(c);
				auto hasExt = [&](const char* ext)->bool {
					size_t Ls = lower.size(), Le = std::strlen(ext);
					if (Ls < Le) return false;
					return lower.compare(Ls - Le, Le, ext) == 0;
					};
				if (hasExt(".fbx") || hasExt(".obj") || hasExt(".gltf") || hasExt(".glb"))
					result.push_back(kv.first);
			}
		}
	}
	std::sort(result.begin(), result.end());
	return result;
}

std::vector<std::string> AssetManager::GetCachedTextureNames() const {
	static const char* exts[] = { ".png", ".jpg", ".jpeg", ".tga", ".dds", ".bmp", ".hdr" };
	std::vector<std::string> result;
	{
		std::lock_guard<std::mutex> lk(m_mtx_);
		result.reserve(m_cache_.size());
		for (auto& kv : m_cache_) {
			std::string lower = kv.first;
			for (auto& c : lower) c = (char)tolower(c);
			auto hasExt = [&](const char* ext)->bool {
				size_t Ls = lower.size(), Le = std::strlen(ext);
				if (Ls < Le) return false;
				return lower.compare(Ls - Le, Le, ext) == 0;
				};
			for (auto* e : exts) {
				if (hasExt(e)) { result.push_back(kv.first); break; }
			}
		}
	}
	std::sort(result.begin(), result.end());
	return result;
}