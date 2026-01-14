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

	void StartAutoSync(std::chrono::milliseconds interval = std::chrono::milliseconds(1000),bool recursive = true);

	void StopAutoSync();

	bool IsAutoSyncRunning() const { return _watchRunning.load(); }

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

	std::string _root;
	LoadMode _mode = LoadMode::FromSource;

	std::unordered_map<std::string, std::vector<uint8_t>> _cache;
	std::unordered_map<std::string, FileMeta> _fileMeta;

	std::deque<ChangeLog> _recentChanges;

	std::thread _watchThread;
	std::atomic<bool> _watchRunning{ false };
	std::chrono::milliseconds _interval{ 1000 };
	bool _recursive = true;

	std::atomic<uint64_t> _scanCount{ 0 };
	std::atomic<uint64_t> _lastDiffAdds{ 0 };
	std::atomic<uint64_t> _lastDiffRemoves{ 0 };
	std::atomic<uint64_t> _lastDiffMods{ 0 };
	std::atomic<uint64_t> _lastScanDurationMs{ 0 };

	mutable std::mutex _mtx;

	static AssetManager* _instance;
	static constexpr size_t _kMaxRecentChanges = 64;
};

#endif // ASSETMANAGER_H