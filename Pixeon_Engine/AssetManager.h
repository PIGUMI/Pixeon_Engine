/*
* ファイル名　AssetManager
* 説　　　明　アセット入出力の窓口
*             仮想ファイルシステム (Common/VirtualFileSystem) をエンジン向けに包む
*
*   エディタ : SceneRoot/Assets などのディレクトリをマウントし、変更を監視してホットリロード
*   ゲーム   : ビルド時に作成した data.pak をマウント
*   どちらも同じ仮想パスで読めるため、呼び出し側はモードを意識しない
*
* 以前の実装からの変更点
*   - 起動時に Assets 以下の全ファイルを RAM に読み込んでいた処理を廃止（オンデマンド読み込み + LRU）
*   - 宣言だけで未実装だった FromArchive を、アーカイブのマウントとして実装
*   - 拡張子による種別判定を Common/AssetType に一元化
*   - 変更検出時にリスナーへ通知し、テクスチャ等を自動で再読込できるようにした
*/
#ifndef ASSETMANAGER_H
#define ASSETMANAGER_H

#include "../Common/AssetType.h"
#include "../Common/VirtualFileSystem.h"
#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

// 仮想パスのマウント先
namespace VfsPrefix {
	constexpr const char* ASSETS = "assets/";
	constexpr const char* SCENE = "scene/";
	constexpr const char* SHADER = "shader/";
	constexpr const char* CONFIG = "config/";
	constexpr const char* ANIM2D = "anim2d/";
}

class AssetManager
{
public:
	using ChangeListener = std::function<void(const FileChange& change)>;

	static AssetManager* Instance();
	static void DeleteInstance();

	void UnInit();

	// ---- マウント ----
	bool DirectoryMount(const std::string& prefix, const std::string& dir, bool watch);
	bool ArchiveMount(const std::string& archive_path, std::string* out_error = nullptr);

	// ---- アセット（assets/ 以下の論理名で扱う）----
	bool AssetLoad(const std::string& logical_name, std::vector<uint8_t>& out_data);
	bool AssetExists(const std::string& logical_name) const;
	std::vector<std::string> AssetListGet(AssetType type) const;
	std::vector<std::string> AssetListGet(std::initializer_list<AssetType> types) const;

	// ---- 汎用（仮想パスで扱う）----
	bool FileRead(const std::string& virtual_path, std::vector<uint8_t>& out_data);
	bool TextRead(const std::string& virtual_path, std::string& out_text);
	bool FileExists(const std::string& virtual_path) const;
	std::vector<FileInfo> FileListGet(const std::string& prefix) const;

	// ---- 変更監視 ----
	void WatchStart(std::chrono::milliseconds interval = std::chrono::milliseconds(1000));
	void WatchStop();
	bool IsWatching() const { return m_cls_Vfs.IsWatching(); }
	int  ListenerAdd(ChangeListener listener);
	void ListenerRemove(int id);
	void ChangesDispatch();   // メインスレッドから毎フレーム呼ぶ

	void CacheClear() { m_cls_Vfs.CacheClear(); }
	void DebugGUIDraw();

private:
	AssetManager() = default;
	~AssetManager();
	static std::string AssetPathMake(const std::string& logical_name);

	struct ChangeLog {
		FileChange change;
		uint64_t i_Scan = 0;
	};

	VirtualFileSystem m_cls_Vfs;
	std::vector<std::pair<int, ChangeListener>> m_Listeners;
	int m_i_NextListenerId = 1;
	std::deque<ChangeLog> m_RecentChanges;
	std::mutex m_Mutex;

	static AssetManager* m_cls_Instance;
	static constexpr size_t MAX_RECENT_CHANGES = 64;
};

#endif // ASSETMANAGER_H
