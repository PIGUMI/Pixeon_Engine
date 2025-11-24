// AssetManager
// アセットのi/oを管理するクラス

#ifndef ASSETMANAGER_H
#define ASSETMANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>

class AssetManager
{
public:
	enum class LoadMode { FromSource, FromArchive };

	static AssetManager* Instance();
	static void DeleteInstance();

	void UnInit();

	void SetRoot(const std::string& root);
	void SetLoadMode(LoadMode m);
	bool LoadAsset(const std::string& logicalName, std::vector<uint8_t>& outData); // 生バイト取得
	bool Exists(const std::string& logicalName);
	void ClearRawCache();

	void DrawDebugGUI();

	void StartAutoSync(std::chrono::milliseconds interval = std::chrono::milliseconds(1000),
		bool recursive = true);

	void StopAutoSync();

	bool IsAutoSyncRunning() const { return m_watchRunning_.load(); }

	std::vector<std::string> GetCachedAssetNames(bool onlyModelExt = false) const;
	std::vector<std::string> GetCachedTextureNames() const;
private:
	AssetManager() = default;
	~AssetManager();
	std::string Normalize(const std::string& name) const;

	void WatchLoop();

	void PerformScan();

	struct FileMeta {
		uint64_t size = 0;
		std::filesystem::file_time_type writeTime;
	};

	enum class ChangeType { Added, Removed, Modified, ReloadFailed };
	struct ChangeLog {
		ChangeType type;
		std::string path;
		uint64_t timestampFrame = 0;
	};

	void PushChange(ChangeType type, const std::string& path);

private:

	std::string m_root_;
	LoadMode m_mode_ = LoadMode::FromSource;

	std::unordered_map<std::string, std::vector<uint8_t>> m_cache_;
	std::unordered_map<std::string, FileMeta> m_fileMeta_;

	std::deque<ChangeLog> m_recentChanges_;

	std::thread m_watchThread_;
	std::atomic<bool> m_watchRunning_{ false };
	std::chrono::milliseconds m_interval_{ 1000 };
	bool m_recursive_ = true;

	std::atomic<uint64_t> m_scanCount_{ 0 };
	std::atomic<uint64_t> m_lastDiffAdds_{ 0 };
	std::atomic<uint64_t> m_lastDiffRemoves_{ 0 };
	std::atomic<uint64_t> m_lastDiffMods_{ 0 };
	std::atomic<uint64_t> m_lastScanDurationMs_{ 0 };

	mutable std::mutex m_mtx_;

	static AssetManager* s_instance_;
	static constexpr size_t kMaxRecentChanges_ = 64;
};

#endif // ASSETMANAGER_H