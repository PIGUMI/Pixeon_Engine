/*
* ファイル名　VirtualFileSystem
* 説　　　明　仮想ファイルシステムの実装
*/
#include "VirtualFileSystem.h"
#include "FileUtil.h"
#include <algorithm>

namespace fs = std::filesystem;

namespace {
	/*
	* 関数名　PrefixClean
	* 引　数　prefix：マウント先
	* 戻り値　前後の '/' を除いた prefix
	* 説　明　マウント先の表記を統一する
	*/
	std::string PrefixClean(const std::string& prefix)
	{
		std::string p = PixArchive::PathClean(prefix);
		while (!p.empty() && p.back() == '/') p.pop_back();
		return p;
	}
}

// ================================================================
// DirectorySource
// ================================================================

/*
* 関数名　DirectorySource
* 引　数　prefix：マウント先　root_dir：実ディレクトリ　watch：変更監視の有無
* 戻り値　なし
* 説　明　初回のスナップショットを取得し、一覧を即座に返せるようにする
*/
DirectorySource::DirectorySource(const std::string& prefix, const std::string& root_dir, bool watch)
	: m_Prefix(PrefixClean(prefix)), m_Root(root_dir), m_b_Watch(watch)
{
	m_PrefixKey = m_Prefix.empty() ? "" : PixArchive::PathNormalize(m_Prefix) + "/";
	m_RootNative = FileUtil::PathToNative(root_dir);
	m_Name = "dir:" + root_dir + " -> " + (m_Prefix.empty() ? "/" : m_Prefix);
	m_Snapshot = SnapshotTake();
}

/*
* 関数名　SnapshotTake
* 引　数　なし
* 戻り値　ディレクトリ内の全ファイルのメタデータ
* 説　明　ファイル内容は読まず、サイズと更新時刻のみ取得する
*/
DirectorySource::Snapshot DirectorySource::SnapshotTake() const
{
	Snapshot snap;
	std::error_code ec;
	if (!fs::is_directory(m_RootNative, ec)) return snap;
	for (auto it = fs::recursive_directory_iterator(m_RootNative, fs::directory_options::skip_permission_denied, ec);
		!ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
		std::error_code ec2;
		if (!it->is_regular_file(ec2)) continue;
		const std::string rel = FileUtil::PathToUtf8(fs::relative(it->path(), m_RootNative, ec2));
		if (ec2) continue;
		Meta m;
		m.path = m_Prefix.empty() ? rel : (m_Prefix + "/" + rel);
		m.i_Size = (uint64_t)it->file_size(ec2);
		m.writeTime = it->last_write_time(ec2);
		snap[PixArchive::PathNormalize(m.path)] = std::move(m);
	}
	return snap;
}

/*
* 関数名　DiskPathResolve
* 引　数　key：正規化キー　out_path：実ファイルパス
* 戻り値　このマウントの管轄なら true
* 説　明　スナップショットに元の大文字小文字が残っているので、それを使って実パスを組み立てる
*/
bool DirectorySource::DiskPathResolve(const std::string& key, fs::path& out_path) const
{
	if (key.rfind(m_PrefixKey, 0) != 0) return false;
	std::string rel;
	{
		std::lock_guard<std::mutex> lk(m_Mutex);
		auto it = m_Snapshot.find(key);
		if (it != m_Snapshot.end()) rel = it->second.path.substr(m_Prefix.empty() ? 0 : m_Prefix.size() + 1);
	}
	if (rel.empty()) {
		// 監視前に作られたファイルなど、スナップショットに無い場合はキーをそのまま使う
		rel = key.substr(m_PrefixKey.size());
	}
	out_path = m_RootNative / FileUtil::PathToNative(rel);
	return true;
}

/*
* 関数名　FileExists
* 引　数　key：正規化キー
* 戻り値　存在すれば true
* 説　明　ディスク上にファイルがあるか調べる
*/
bool DirectorySource::FileExists(const std::string& key) const
{
	fs::path p;
	if (!DiskPathResolve(key, p)) return false;
	std::error_code ec;
	return fs::is_regular_file(p, ec);
}

/*
* 関数名　FileRead
* 引　数　key：正規化キー　out_data：読み込み先
* 戻り値　成功なら true
* 説　明　ディスクからファイルを読む
*/
bool DirectorySource::FileRead(const std::string& key, std::vector<uint8_t>& out_data)
{
	fs::path p;
	if (!DiskPathResolve(key, p)) return false;
	return FileUtil::FileReadAll(FileUtil::PathToUtf8(p), out_data);
}

/*
* 関数名　FileInfoCollect
* 引　数　out_map：追加先
* 戻り値　なし
* 説　明　スナップショットから一覧を作る（未登録のキーのみ追加）
*/
void DirectorySource::FileInfoCollect(std::unordered_map<std::string, FileInfo>& out_map) const
{
	std::lock_guard<std::mutex> lk(m_Mutex);
	for (auto& kv : m_Snapshot) {
		out_map.emplace(kv.first, FileInfo{ kv.second.path, kv.second.i_Size });
	}
}

/*
* 関数名　ChangesScan
* 引　数　out_changes：変更の追加先
* 戻り値　なし
* 説　明　新しいスナップショットと比較し、追加・変更・削除を検出する
*/
void DirectorySource::ChangesScan(std::vector<FileChange>& out_changes)
{
	Snapshot now = SnapshotTake();
	std::lock_guard<std::mutex> lk(m_Mutex);
	for (auto& kv : now) {
		auto old = m_Snapshot.find(kv.first);
		if (old == m_Snapshot.end()) {
			out_changes.push_back({ FileChange::Type::Added, kv.second.path });
		}
		else if (old->second.i_Size != kv.second.i_Size || old->second.writeTime != kv.second.writeTime) {
			out_changes.push_back({ FileChange::Type::Modified, kv.second.path });
		}
	}
	for (auto& kv : m_Snapshot) {
		if (now.find(kv.first) == now.end()) {
			out_changes.push_back({ FileChange::Type::Removed, kv.second.path });
		}
	}
	m_Snapshot = std::move(now);
}

// ================================================================
// ArchiveSource
// ================================================================

/*
* 関数名　Open
* 引　数　archive_path：アーカイブのパス
* 戻り値　成功なら true
* 説　明　アーカイブを開く
*/
bool ArchiveSource::Open(const std::string& archive_path)
{
	m_Name = "pak:" + archive_path;
	return m_cls_Reader.Open(archive_path);
}

bool ArchiveSource::FileExists(const std::string& key) const
{
	return m_cls_Reader.FileExists(key);
}

bool ArchiveSource::FileRead(const std::string& key, std::vector<uint8_t>& out_data)
{
	return m_cls_Reader.FileRead(key, out_data);
}

/*
* 関数名　FileInfoCollect
* 引　数　out_map：追加先
* 戻り値　なし
* 説　明　TOC から一覧を作る
*/
void ArchiveSource::FileInfoCollect(std::unordered_map<std::string, FileInfo>& out_map) const
{
	for (auto& path : m_cls_Reader.FileListGet()) {
		const PixArchive::Entry* e = m_cls_Reader.EntryFind(path);
		out_map.emplace(PixArchive::PathNormalize(path), FileInfo{ path, e ? e->originalSize : 0 });
	}
}

// ================================================================
// VirtualFileSystem
// ================================================================

VirtualFileSystem::~VirtualFileSystem()
{
	WatchStop();
}

/*
* 関数名　SourceMount
* 引　数　source：供給元
* 戻り値　なし
* 説　明　供給元を最優先でマウントする
*/
void VirtualFileSystem::SourceMount(std::unique_ptr<IFileSource> source)
{
	if (!source) return;
	{
		std::lock_guard<std::mutex> lk(m_SourceMutex);
		m_Sources.insert(m_Sources.begin(), std::shared_ptr<IFileSource>(std::move(source)));
	}
	CacheClear();
}

/*
* 関数名　DirectoryMount
* 引　数　prefix：マウント先　root_dir：実ディレクトリ　watch：変更監視
* 戻り値　ディレクトリが存在すれば true（存在しなくてもマウントはする）
* 説　明　ディレクトリをマウントする
*/
bool VirtualFileSystem::DirectoryMount(const std::string& prefix, const std::string& root_dir, bool watch)
{
	std::error_code ec;
	const bool exists = fs::is_directory(FileUtil::PathToNative(root_dir), ec);
	SourceMount(std::make_unique<DirectorySource>(prefix, root_dir, watch));
	return exists;
}

/*
* 関数名　ArchiveMount
* 引　数　archive_path：アーカイブ　out_error：エラー内容（任意）
* 戻り値　成功なら true
* 説　明　アーカイブをマウントする
*/
bool VirtualFileSystem::ArchiveMount(const std::string& archive_path, std::string* out_error)
{
	auto src = std::make_unique<ArchiveSource>();
	if (!src->Open(archive_path)) {
		if (out_error) *out_error = src->GetLastError();
		return false;
	}
	SourceMount(std::move(src));
	return true;
}

/*
* 関数名　SourcesClear
* 引　数　なし
* 戻り値　なし
* 説　明　全マウントを解除する
*/
void VirtualFileSystem::SourcesClear()
{
	{
		std::lock_guard<std::mutex> lk(m_SourceMutex);
		m_Sources.clear();
	}
	CacheClear();
}

std::vector<std::string> VirtualFileSystem::SourceNamesGet() const
{
	std::lock_guard<std::mutex> lk(m_SourceMutex);
	std::vector<std::string> names;
	for (auto& s : m_Sources) names.push_back(s->NameGet());
	return names;
}

/*
* 関数名　FileExists
* 引　数　path：仮想パス
* 戻り値　いずれかの供給元に存在すれば true
* 説　明　ファイルの存在確認
*/
bool VirtualFileSystem::FileExists(const std::string& path) const
{
	const std::string key = PixArchive::PathNormalize(path);
	std::lock_guard<std::mutex> lk(m_SourceMutex);
	for (auto& s : m_Sources) {
		if (s->FileExists(key)) return true;
	}
	return false;
}

/*
* 関数名　FileRead
* 引　数　path：仮想パス　out_data：読み込み先
* 戻り値　成功なら true
* 説　明　キャッシュ → 優先度順の供給元 の順に探して読む
*/
bool VirtualFileSystem::FileRead(const std::string& path, std::vector<uint8_t>& out_data)
{
	const std::string key = PixArchive::PathNormalize(path);
	if (CacheFind(key, out_data)) return true;

	std::vector<std::shared_ptr<IFileSource>> sources;
	{
		std::lock_guard<std::mutex> lk(m_SourceMutex);
		sources = m_Sources;
	}
	for (auto& s : sources) {
		if (s->FileRead(key, out_data)) {
			CacheInsert(key, out_data);
			return true;
		}
	}
	return false;
}

/*
* 関数名　TextRead
* 引　数　path：仮想パス　out_text：読み込み先
* 戻り値　成功なら true
* 説　明　テキストとして読む（UTF-8 BOM は除去）
*/
bool VirtualFileSystem::TextRead(const std::string& path, std::string& out_text)
{
	std::vector<uint8_t> data;
	if (!FileRead(path, data)) return false;
	const size_t start = (data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) ? 3 : 0;
	out_text.assign((const char*)data.data() + start, data.size() - start);
	return true;
}

/*
* 関数名　FileListGet
* 引　数　prefix：仮想パスの接頭辞
* 戻り値　ファイル一覧（仮想パス昇順）
* 説　明　全供給元の一覧を統合する。同じパスは優先度の高い供給元を採用する
*/
std::vector<FileInfo> VirtualFileSystem::FileListGet(const std::string& prefix) const
{
	std::unordered_map<std::string, FileInfo> merged;
	{
		std::lock_guard<std::mutex> lk(m_SourceMutex);
		for (auto& s : m_Sources) s->FileInfoCollect(merged);
	}
	const std::string pkey = PixArchive::PathNormalize(prefix);
	std::vector<FileInfo> result;
	for (auto& kv : merged) {
		if (kv.first.rfind(pkey, 0) == 0) result.push_back(kv.second);
	}
	std::sort(result.begin(), result.end(), [](const FileInfo& a, const FileInfo& b) { return a.path < b.path; });
	return result;
}

// ---- キャッシュ ----------------------------------------------------------

void VirtualFileSystem::CacheBudgetSet(size_t bytes)
{
	std::lock_guard<std::mutex> lk(m_CacheMutex);
	m_i_CacheBudget = bytes;
	CacheTrim();
}

void VirtualFileSystem::CacheClear()
{
	std::lock_guard<std::mutex> lk(m_CacheMutex);
	m_CacheList.clear();
	m_CacheIndex.clear();
	m_i_CacheBytes = 0;
}

VirtualFileSystem::CacheStats VirtualFileSystem::CacheStatsGet() const
{
	std::lock_guard<std::mutex> lk(m_CacheMutex);
	return CacheStats{ m_CacheList.size(), m_i_CacheBytes, m_i_CacheBudget, m_i_CacheHits, m_i_CacheMisses };
}

/*
* 関数名　CacheFind
* 引　数　key：正規化キー　out_data：コピー先
* 戻り値　ヒットすれば true
* 説　明　キャッシュを検索し、ヒットしたら最新として先頭へ移動する
*/
bool VirtualFileSystem::CacheFind(const std::string& key, std::vector<uint8_t>& out_data)
{
	std::lock_guard<std::mutex> lk(m_CacheMutex);
	auto it = m_CacheIndex.find(key);
	if (it == m_CacheIndex.end()) {
		++m_i_CacheMisses;
		return false;
	}
	m_CacheList.splice(m_CacheList.begin(), m_CacheList, it->second);
	out_data = it->second->data;
	++m_i_CacheHits;
	return true;
}

/*
* 関数名　CacheInsert
* 引　数　key：正規化キー　data：内容
* 戻り値　なし
* 説　明　予算の 1/4 を超える巨大ファイルはキャッシュしない
*/
void VirtualFileSystem::CacheInsert(const std::string& key, const std::vector<uint8_t>& data)
{
	std::lock_guard<std::mutex> lk(m_CacheMutex);
	if (data.size() > m_i_CacheBudget / 4) return;
	auto it = m_CacheIndex.find(key);
	if (it != m_CacheIndex.end()) {
		m_i_CacheBytes -= it->second->data.size();
		m_CacheList.erase(it->second);
		m_CacheIndex.erase(it);
	}
	m_CacheList.push_front(CacheEntry{ key, data });
	m_CacheIndex[key] = m_CacheList.begin();
	m_i_CacheBytes += data.size();
	CacheTrim();
}

void VirtualFileSystem::CacheErase(const std::string& key)
{
	std::lock_guard<std::mutex> lk(m_CacheMutex);
	auto it = m_CacheIndex.find(key);
	if (it == m_CacheIndex.end()) return;
	m_i_CacheBytes -= it->second->data.size();
	m_CacheList.erase(it->second);
	m_CacheIndex.erase(it);
}

// 呼び出し側で m_CacheMutex をロックしていること
void VirtualFileSystem::CacheTrim()
{
	while (m_i_CacheBytes > m_i_CacheBudget && !m_CacheList.empty()) {
		auto& back = m_CacheList.back();
		m_i_CacheBytes -= back.data.size();
		m_CacheIndex.erase(back.key);
		m_CacheList.pop_back();
	}
}

// ---- 監視 ----------------------------------------------------------------

/*
* 関数名　ChangesScanNow
* 引　数　なし
* 戻り値　なし
* 説　明　監視対象の供給元を走査し、変更があればキャッシュを破棄してイベントを積む
*/
void VirtualFileSystem::ChangesScanNow()
{
	std::vector<std::shared_ptr<IFileSource>> sources;
	{
		std::lock_guard<std::mutex> lk(m_SourceMutex);
		sources = m_Sources;
	}
	std::vector<FileChange> changes;
	for (auto& s : sources) {
		if (s->IsWatchable()) s->ChangesScan(changes);
	}
	for (auto& c : changes) CacheErase(PixArchive::PathNormalize(c.path));
	if (!changes.empty()) {
		std::lock_guard<std::mutex> lk(m_ChangeMutex);
		m_PendingChanges.insert(m_PendingChanges.end(), changes.begin(), changes.end());
	}
	m_i_ScanCount.fetch_add(1);
}

/*
* 関数名　ChangesPoll
* 引　数　out_changes：取り出し先
* 戻り値　取り出した件数
* 説　明　溜まった変更イベントを取り出す（メインスレッドから毎フレーム呼ぶ想定）
*/
size_t VirtualFileSystem::ChangesPoll(std::vector<FileChange>& out_changes)
{
	std::lock_guard<std::mutex> lk(m_ChangeMutex);
	const size_t n = m_PendingChanges.size();
	out_changes.insert(out_changes.end(), m_PendingChanges.begin(), m_PendingChanges.end());
	m_PendingChanges.clear();
	return n;
}

/*
* 関数名　WatchStart
* 引　数　interval：走査間隔
* 戻り値　なし
* 説　明　バックグラウンドで変更監視を開始する
*/
void VirtualFileSystem::WatchStart(std::chrono::milliseconds interval)
{
	if (m_b_WatchRunning.load()) return;
	m_WatchInterval = interval;
	m_b_WatchRunning = true;
	m_cls_WatchThread = std::thread(&VirtualFileSystem::WatchLoop, this);
}

void VirtualFileSystem::WatchStop()
{
	if (!m_b_WatchRunning.exchange(false)) return;
	if (m_cls_WatchThread.joinable()) m_cls_WatchThread.join();
}

/*
* 関数名　WatchLoop
* 引　数　なし
* 戻り値　なし
* 説　明　停止要求に素早く応じられるよう、待機は短い間隔で区切る
*/
void VirtualFileSystem::WatchLoop()
{
	while (m_b_WatchRunning.load()) {
		ChangesScanNow();
		const auto until = std::chrono::steady_clock::now() + m_WatchInterval;
		while (m_b_WatchRunning.load() && std::chrono::steady_clock::now() < until) {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
	}
}
