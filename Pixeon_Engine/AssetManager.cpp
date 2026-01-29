/*
* ファイル名　AssetManager
* 説　　　明　アセット管理クラス
*/
#include "AssetManager.h"
#include <fstream>
#include <Windows.h>
#include "IMGUI/imgui.h"

AssetManager* AssetManager::_instance = nullptr;

/*
* 関数名　Instance
* 引　数　なし
* 戻り値　インスタンスのポインタ
* 説　明　AssetManagerのシングルトンインスタンスを取得する
*/
AssetManager* AssetManager::Instance()
{
	if (!_instance) {
		_instance = new AssetManager();
	}
	return _instance;
}

/*
* 関数名　DeleteInstance
* 引　数　なし
* 戻り値　なし
* 説　明　AssetManagerのシングルトンインスタンスを破棄する
*/
void AssetManager::DeleteInstance()
{
	if (_instance) {
		_instance->UnInit();
		delete _instance;
		_instance = nullptr;
	}
}

/*
* 関数名　UnInit
* 引　数　なし
* 戻り値　なし
* 説　明　AssetManagerのアンイニシャライズ処理
*/
void AssetManager::UnInit()
{
	StopAutoSync();
	ClearRawCache();
	{
		std::lock_guard<std::mutex> lk(_mtx);
		_recentChanges.clear();
	}
}

/*
* 関数名　~AssetManager
* 引　数　なし
* 戻り値　なし
* 説　明　AssetManagerのデストラクタ
*/
AssetManager::~AssetManager() {
	StopAutoSync();
}

/*
* 関数名　SetRoot
* 引　数　root：アセットルートディレクトリのパス
* 戻り値　なし
* 説　明　アセットルートディレクトリを設定する
*/
void AssetManager::SetRoot(const std::string& root) { _root = root; }

/*
* 関数名　SetLoadMode
* 引　数　m：ロードモード
* 戻り値　なし
* 説　明　アセットのロードモードを設定する
*/
void AssetManager::SetLoadMode(LoadMode m) { _mode = m; }

/*
* 関数名　Normalize
* 引　数　name：正規化するアセット名
* 戻り値　正規化されたアセット名
* 説　明　アセット名のパス区切り文字を統一する
*/
std::string AssetManager::Normalize(const std::string& name) const {
	std::string s = name;
	for (auto& c : s) if (c == '\\') c = '/';
	return s;
}

/*
* 関数名　Exists
* 引　数　logicalName：論理アセット名
* 戻り値　存在する場合true、存在しない場合false
* 説　明　アセットが存在するかどうかを確認する
*/
bool AssetManager::Exists(const std::string& logicalName) {
	std::string norm = Normalize(logicalName);
	{
		std::lock_guard<std::mutex> lk(_mtx);
		if (_cache.find(norm) != _cache.end()) return true;
	}
	std::filesystem::path p = std::filesystem::path(_root) / norm;
	return std::filesystem::exists(p);
}

/*
* 関数名　LoadAsset
* 引　数　logicalName：論理アセット名
* 　　　　outData：読み込んだアセットデータの出力先
* 戻り値　読み込みに成功した場合true、失敗した場合false
* 説　明　アセットを読み込み、生バイトデータを取得する
*/
bool AssetManager::LoadAsset(const std::string& logicalName, std::vector<uint8_t>& outData) {
	std::string norm = Normalize(logicalName);
	{
		std::lock_guard<std::mutex> lk(_mtx);
		auto it = _cache.find(norm);
		if (it != _cache.end()) {
			outData = it->second;
			return true;
		}
	}

	std::filesystem::path p = std::filesystem::path(_root) / norm;
	std::ifstream ifs(p, std::ios::binary);
	if (!ifs) {
		return false;
	}
	ifs.seekg(0, std::ios::end);
	size_t sz = (size_t)ifs.tellg();
	ifs.seekg(0, std::ios::beg);
	outData.resize(sz);
	ifs.read((char*)outData.data(), sz);
	if (!ifs) {
		return false;
	}
	{
		std::lock_guard<std::mutex> lk(_mtx);
		_cache[norm] = outData;
	}
	return true;
}

/*
* 関数名　ClearRawCache
* 引　数　なし
* 戻り値　なし
* 説　明　生バイトキャッシュをクリアする
*/
void AssetManager::ClearRawCache() {
	std::lock_guard<std::mutex> lk(_mtx);
	_cache.clear();
	_fileMeta.clear();
}

/*
* 関数名　PushChange
* 引　数　type：変更タイプ
* 　　　　path：変更されたアセットのパス
* 戻り値　なし
* 説　明　変更ログを追加する
*/
void AssetManager::PushChange(ChangeType type, const std::string& path) {
	std::lock_guard<std::mutex> lk(_mtx);
	if (_recentChanges.size() >= _kMaxRecentChanges)
		_recentChanges.pop_front();
	_recentChanges.push_back({ type, path, _scanCount.load() });
}

/*
* 関数名　StartAutoSync
* 引　数　interval：スキャン間隔
* 　　　　recursive：再帰的にサブディレクトリも監視するかどうか
* 戻り値　なし
* 説　明　自動同期監視を開始する
*/
void AssetManager::StartAutoSync(std::chrono::milliseconds interval, bool recursive) {
	if (_watchRunning.load()) return;
	if (_root.empty()) {
		return;
	}
	_interval = interval;
	_recursive = recursive;
	_watchRunning = true;
	_watchThread = std::thread(&AssetManager::WatchLoop, this);
}

/*
* 関数名　StopAutoSync
* 引　数　なし
* 戻り値　なし
* 説　明　自動同期監視を停止する
*/
void AssetManager::StopAutoSync() {
	if (!_watchRunning.load()) return;
	_watchRunning = false;
	if (_watchThread.joinable()) _watchThread.join();
}

/*
* 関数名　WatchLoop
* 引　数　なし
* 戻り値　なし
* 説　明　自動同期監視ループ
*/
void AssetManager::WatchLoop() {
	PerformScan();
	while (_watchRunning.load()) {
		auto t0 = std::chrono::steady_clock::now();
		PerformScan();
		auto t1 = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
		_lastScanDurationMs.store((uint64_t)elapsed);
		std::this_thread::sleep_for(_interval);
	}
}

/*
* 関数名　PerformScan
* 引　数　なし
* 戻り値　なし
* 説　明　アセットディレクトリのスキャンを実行し、変更を検出して適用する
*/
void AssetManager::PerformScan() {
	using namespace std::filesystem;

	std::unordered_map<std::string, FileMeta> current;
	std::vector<std::string> additions;
	std::vector<std::string> modifications;
	std::vector<std::string> deletions;

	const path root_Path(_root);
	if (!exists(root_Path)) {
		return;
	}

	auto scan_Start = std::chrono::steady_clock::now();

	std::error_code ec;
	if (_recursive) {
		for (recursive_directory_iterator it(root_Path, ec), end; it != end && !ec; ++it) {
			if (!it->is_regular_file()) continue;
			auto rel = relative(it->path(), root_Path, ec);
			if (ec) continue;
			std::string relStr = Normalize(rel.generic_string());
			FileMeta meta;
			meta.size = (uint64_t)file_size(it->path(), ec);
			meta.writeTime = last_write_time(it->path(), ec);
			current[relStr] = meta;
		}
	}
	else {
		for (directory_iterator it(root_Path, ec), end; it != end && !ec; ++it) {
			if (!it->is_regular_file()) continue;
			auto rel = relative(it->path(), root_Path, ec);
			if (ec) continue;
			std::string relStr = Normalize(rel.generic_string());
			FileMeta meta;
			meta.size = (uint64_t)file_size(it->path(), ec);
			meta.writeTime = last_write_time(it->path(), ec);
			current[relStr] = meta;
		}
	}

	{
		std::lock_guard<std::mutex> lk(_mtx);
		for (auto& kv : current) {
			auto itOld = _fileMeta.find(kv.first);
			if (itOld == _fileMeta.end()) {
				additions.push_back(kv.first);
			}
			else {
				if (kv.second.size != itOld->second.size ||
					kv.second.writeTime != itOld->second.writeTime) {
					modifications.push_back(kv.first);
				}
			}
		}
		for (auto& old : _fileMeta) {
			if (current.find(old.first) == current.end()) {
				deletions.push_back(old.first);
			}
		}
	}

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

	{
		std::lock_guard<std::mutex> lk(_mtx);
		for (auto& del : deletions) {
			_cache.erase(del);
			_fileMeta.erase(del);
			PushChange(ChangeType::Removed, del);
		}
		for (auto& kv : current) {
			_fileMeta[kv.first] = kv.second;
		}
	}

	_lastDiffAdds.store(additions.size());
	_lastDiffMods.store(modifications.size());
	_lastDiffRemoves.store(deletions.size());
	_scanCount.fetch_add(1);
}

/*
* 関数名　DrawDebugGUI
* 引　数　なし
* 戻り値　なし
* 説　明　デバッグ用GUIを描画する
*/
void AssetManager::DrawDebugGUI()
{
	std::lock_guard<std::mutex> lk(_mtx);
	ImGui::TextUnformatted("AssetManager");
	ImGui::Separator();
	ImGui::Text("Root: %s", _root.c_str());
	ImGui::Text("Mode: %s", (_mode == LoadMode::FromSource) ? "FromSource" : "FromArchive");
	ImGui::Text("Cached Raw Files: %zu", _cache.size());
	ImGui::Text("AutoSync: %s", _watchRunning.load() ? "Running" : "Stopped");
	ImGui::Text("ScanCount: %llu", (unsigned long long)_scanCount.load());
	ImGui::Text("LastDiff A=%llu M=%llu R=%llu",
		(unsigned long long)_lastDiffAdds.load(),
		(unsigned long long)_lastDiffMods.load(),
		(unsigned long long)_lastDiffRemoves.load());
	ImGui::Text("LastScanDuration: %llu ms", (unsigned long long)_lastScanDurationMs.load());

	static char filter[128] = "";
	ImGui::InputText("Filter (substring)", filter, sizeof(filter));

	if (ImGui::Button("Clear Raw Cache")) {
		_cache.clear();
		_fileMeta.clear();
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear Change Log")) {
		_recentChanges.clear();
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Recent Changes:");
	ImGui::BeginChild("AssetManagerChanges", ImVec2(0, 120), true);
	for (auto it = _recentChanges.rbegin(); it != _recentChanges.rend(); ++it) {
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
	for (auto& kv : _cache) {
		if (filter[0] && kv.first.find(filter) == std::string::npos) continue;
		ImGui::Text("%s (size=%zu bytes)", kv.first.c_str(), kv.second.size());
	}
	ImGui::EndChild();
}

/*
* 関数名　GetCachedAssetNames
* 引　数　onlyModelExt：モデル拡張子のみ取得するかどうか
* 戻り値　キャッシュされているアセット名のリスト
* 説　明　キャッシュされているアセット名のリストを取得する
*/
std::vector<std::string> AssetManager::GetCachedAssetNames(bool onlyModelExt) const {
	std::vector<std::string> result;
	{
		std::lock_guard<std::mutex> lk(_mtx);
		result.reserve(_cache.size());
		for (auto& kv : _cache) {
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
				if (hasExt(".fbx") || hasExt(".obj") || hasExt(".glb"))
					result.push_back(kv.first);
			}
		}
	}
	std::sort(result.begin(), result.end());
	return result;
}

/*
* 関数名　GetCachedTextureNames
* 引　数　なし
* 戻り値　キャッシュされているテクスチャアセット名のリスト
* 説　明　キャッシュされているテクスチャアセット名のリストを取得する
*/
std::vector<std::string> AssetManager::GetCachedTextureNames() const {
	static const char* exts[] = { ".png", ".jpg", ".jpeg", ".tga", ".dds", ".bmp", ".hdr" };
	std::vector<std::string> result;
	{
		std::lock_guard<std::mutex> lk(_mtx);
		result.reserve(_cache.size());
		for (auto& kv : _cache) {
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

/*
* 関数名　GetCachedEffectNames
* 引　数　なし
* 戻り値　キャッシュされているエフェクトアセット名のリスト
* 説　明　キャッシュされているエフェクトアセット名のリストを取得する
*/
std::vector<std::string> AssetManager::GetCachedEffectNames() const
{
	static const char* exts[] = { ".efkefc", ".efk" };
	std::vector<std::string> result;
	{
		std::lock_guard<std::mutex> lk(_mtx);
		result.reserve(_cache.size());
		for (auto& kv : _cache) {
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