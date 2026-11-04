/*
* ファイル名　VirtualFileSystem
* 説　　　明　仮想ファイルシステム
*             複数の供給元（ディレクトリ / アーカイブ）を仮想パスへマウントし、
*             エディタとビルド済みゲームで同じパスでファイルを読めるようにする
*
* 仮想パスの例
*   assets/lever_action_FBX/textures/B.jpeg   ← SceneRoot/Assets/ または data.pak
*   scene/SampleScene.scene                    ← SceneRoot/Scene/   または data.pak
*   shader/cso/VS_Base.cso                     ← SceneRoot/Shader/  または data.pak
*
* 方針
*   - 読み込みはオンデマンド。全ファイルをメモリへ常駐させない
*   - 生データのキャッシュは LRU で容量上限を設ける
*   - ディレクトリ監視は「メタデータ（サイズ・更新時刻）の比較のみ」で行い、
*     変更検出時はキャッシュを破棄して変更イベントを積む。
*     イベントはメインスレッドから ChangesPoll で受け取り、リソースの再読込に使う
*/
#ifndef VIRTUAL_FILE_SYSTEM_H
#define VIRTUAL_FILE_SYSTEM_H

#include "PixArchive.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// ファイルの変更通知
struct FileChange {
	enum class Type { Added, Removed, Modified };
	Type e_Type = Type::Modified;
	std::string path;   // 仮想パス
};

// ファイル情報（一覧・索引用）
struct FileInfo {
	std::string path;   // 仮想パス（大文字小文字は元のまま）
	uint64_t i_Size = 0;
};

// ================================================================
// 供給元インターフェース
// ================================================================
class IFileSource {
public:
	virtual ~IFileSource() = default;
	virtual const std::string& NameGet() const = 0;
	virtual bool FileExists(const std::string& key) const = 0;
	virtual bool FileRead(const std::string& key, std::vector<uint8_t>& out_data) = 0;
	// 正規化キー → ファイル情報 の一覧を追加する
	virtual void FileInfoCollect(std::unordered_map<std::string, FileInfo>& out_map) const = 0;
	// 変更を検出して out_changes に追加する（監視非対応なら何もしない）
	virtual void ChangesScan(std::vector<FileChange>& out_changes) { (void)out_changes; }
	virtual bool IsWatchable() const { return false; }
};

// ディスク上のディレクトリを prefix へマウントする
class DirectorySource : public IFileSource {
public:
	DirectorySource(const std::string& prefix, const std::string& root_dir, bool watch);
	const std::string& NameGet() const override { return m_Name; }
	bool FileExists(const std::string& key) const override;
	bool FileRead(const std::string& key, std::vector<uint8_t>& out_data) override;
	void FileInfoCollect(std::unordered_map<std::string, FileInfo>& out_map) const override;
	void ChangesScan(std::vector<FileChange>& out_changes) override;
	bool IsWatchable() const override { return m_b_Watch; }
	const std::string& RootGet() const { return m_Root; }

private:
	struct Meta {
		std::string path;   // 仮想パス
		uint64_t i_Size = 0;
		std::filesystem::file_time_type writeTime;
	};
	using Snapshot = std::unordered_map<std::string, Meta>;
	Snapshot SnapshotTake() const;
	bool DiskPathResolve(const std::string& key, std::filesystem::path& out_path) const;

	std::string m_Name;
	std::string m_Prefix;      // "assets" など（末尾 '/' 無し、空ならルート）
	std::string m_PrefixKey;   // 正規化済み prefix + '/'
	std::string m_Root;
	std::filesystem::path m_RootNative;
	bool m_b_Watch = false;
	Snapshot m_Snapshot;
	mutable std::mutex m_Mutex;
};

// PIXPAK アーカイブをマウントする（アーカイブ内パスがそのまま仮想パス）
class ArchiveSource : public IFileSource {
public:
	bool Open(const std::string& archive_path);
	const std::string& NameGet() const override { return m_Name; }
	bool FileExists(const std::string& key) const override;
	bool FileRead(const std::string& key, std::vector<uint8_t>& out_data) override;
	void FileInfoCollect(std::unordered_map<std::string, FileInfo>& out_map) const override;
	const std::string& GetLastError() const { return m_cls_Reader.GetLastError(); }

private:
	std::string m_Name;
	PixArchive::Reader m_cls_Reader;
};

// ================================================================
// 仮想ファイルシステム本体
// ================================================================
class VirtualFileSystem {
public:
	struct CacheStats {
		size_t i_Entries = 0;
		size_t i_Bytes = 0;
		size_t i_Budget = 0;
		uint64_t i_Hits = 0;
		uint64_t i_Misses = 0;
	};

	VirtualFileSystem() = default;
	~VirtualFileSystem();
	VirtualFileSystem(const VirtualFileSystem&) = delete;
	VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;

	// マウント（後からマウントしたものが優先される）
	void SourceMount(std::unique_ptr<IFileSource> source);
	bool DirectoryMount(const std::string& prefix, const std::string& root_dir, bool watch);
	bool ArchiveMount(const std::string& archive_path, std::string* out_error = nullptr);
	void SourcesClear();
	std::vector<std::string> SourceNamesGet() const;

	// 読み込み
	bool FileExists(const std::string& path) const;
	bool FileRead(const std::string& path, std::vector<uint8_t>& out_data);
	bool TextRead(const std::string& path, std::string& out_text);
	// prefix 以下のファイル一覧（優先度の高い供給元を採用、仮想パス昇順）
	std::vector<FileInfo> FileListGet(const std::string& prefix = "") const;

	// キャッシュ
	void CacheBudgetSet(size_t bytes);
	void CacheClear();
	CacheStats CacheStatsGet() const;

	// 監視
	void WatchStart(std::chrono::milliseconds interval);
	void WatchStop();
	bool IsWatching() const { return m_b_WatchRunning.load(); }
	void ChangesScanNow();
	size_t ChangesPoll(std::vector<FileChange>& out_changes);
	uint64_t ScanCountGet() const { return m_i_ScanCount.load(); }

private:
	void WatchLoop();
	void CacheInsert(const std::string& key, const std::vector<uint8_t>& data);
	bool CacheFind(const std::string& key, std::vector<uint8_t>& out_data);
	void CacheErase(const std::string& key);
	void CacheTrim();

	std::vector<std::shared_ptr<IFileSource>> m_Sources;   // 先頭ほど優先
	mutable std::mutex m_SourceMutex;

	// LRU キャッシュ（先頭が最新）
	struct CacheEntry {
		std::string key;
		std::vector<uint8_t> data;
	};
	std::list<CacheEntry> m_CacheList;
	std::unordered_map<std::string, std::list<CacheEntry>::iterator> m_CacheIndex;
	size_t m_i_CacheBytes = 0;
	size_t m_i_CacheBudget = 64ull * 1024 * 1024;
	uint64_t m_i_CacheHits = 0;
	uint64_t m_i_CacheMisses = 0;
	mutable std::mutex m_CacheMutex;

	std::vector<FileChange> m_PendingChanges;
	std::mutex m_ChangeMutex;
	std::thread m_cls_WatchThread;
	std::atomic<bool> m_b_WatchRunning{ false };
	std::chrono::milliseconds m_WatchInterval{ 1000 };
	std::atomic<uint64_t> m_i_ScanCount{ 0 };
};

#endif // !VIRTUAL_FILE_SYSTEM_H
