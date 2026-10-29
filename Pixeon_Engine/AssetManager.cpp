/*
* ファイル名　AssetManager
* 説　　　明　アセット管理クラス
*/
#include "AssetManager.h"
#include "IMGUI/imgui.h"
#include <algorithm>

AssetManager* AssetManager::m_cls_Instance = nullptr;

/*
* 関数名　Instance
* 引　数　なし
* 戻り値　インスタンスのポインタ
* 説　明　AssetManagerのシングルトンインスタンスを取得する
*/
AssetManager* AssetManager::Instance()
{
	if (!m_cls_Instance) m_cls_Instance = new AssetManager();
	return m_cls_Instance;
}

/*
* 関数名　DeleteInstance
* 引　数　なし
* 戻り値　なし
* 説　明　AssetManagerのシングルトンインスタンスを破棄する
*/
void AssetManager::DeleteInstance()
{
	if (m_cls_Instance) {
		m_cls_Instance->UnInit();
		delete m_cls_Instance;
		m_cls_Instance = nullptr;
	}
}

AssetManager::~AssetManager()
{
	WatchStop();
}

/*
* 関数名　UnInit
* 引　数　なし
* 戻り値　なし
* 説　明　監視を止め、マウントとキャッシュを解放する
*/
void AssetManager::UnInit()
{
	WatchStop();
	m_cls_Vfs.SourcesClear();
	std::lock_guard<std::mutex> lk(m_Mutex);
	m_RecentChanges.clear();
	m_Listeners.clear();
}

/*
* 関数名　DirectoryMount
* 引　数　prefix：マウント先 (VfsPrefix)　dir：実ディレクトリ　watch：変更監視するか
* 戻り値　ディレクトリが存在すれば true
* 説　明　ディレクトリをマウントする
*/
bool AssetManager::DirectoryMount(const std::string& prefix, const std::string& dir, bool watch)
{
	return m_cls_Vfs.DirectoryMount(prefix, dir, watch);
}

/*
* 関数名　ArchiveMount
* 引　数　archive_path：アーカイブ　out_error：エラー内容
* 戻り値　成功なら true
* 説　明　ビルド済みアーカイブをマウントする（ディレクトリより優先される）
*/
bool AssetManager::ArchiveMount(const std::string& archive_path, std::string* out_error)
{
	return m_cls_Vfs.ArchiveMount(archive_path, out_error);
}

/*
* 関数名　AssetPathMake
* 引　数　logical_name：Assets ルートからの相対名
* 戻り値　仮想パス
* 説　明　論理名を assets/ 以下の仮想パスへ変換する
*/
std::string AssetManager::AssetPathMake(const std::string& logical_name)
{
	return std::string(VfsPrefix::ASSETS) + PixArchive::PathClean(logical_name);
}

/*
* 関数名　AssetLoad
* 引　数　logical_name：論理アセット名　out_data：読み込み先
* 戻り値　成功なら true
* 説　明　アセットの生データを読み込む
*/
bool AssetManager::AssetLoad(const std::string& logical_name, std::vector<uint8_t>& out_data)
{
	return m_cls_Vfs.FileRead(AssetPathMake(logical_name), out_data);
}

/*
* 関数名　AssetExists
* 引　数　logical_name：論理アセット名
* 戻り値　存在すれば true
* 説　明　アセットの存在を確認する
*/
bool AssetManager::AssetExists(const std::string& logical_name) const
{
	return m_cls_Vfs.FileExists(AssetPathMake(logical_name));
}

/*
* 関数名　AssetListGet
* 引　数　types：取得する種別
* 戻り値　論理アセット名の一覧（昇順）
* 説　明　索引から指定種別のアセット名を列挙する（ファイル内容は読まない）
*/
std::vector<std::string> AssetManager::AssetListGet(std::initializer_list<AssetType> types) const
{
	const size_t prefix_len = std::char_traits<char>::length(VfsPrefix::ASSETS);
	std::vector<std::string> result;
	for (auto& info : m_cls_Vfs.FileListGet(VfsPrefix::ASSETS)) {
		const AssetType t = AssetTypeUtil::TypeFromPath(info.path);
		if (std::find(types.begin(), types.end(), t) != types.end()) {
			result.push_back(info.path.substr(prefix_len));
		}
	}
	return result;
}

std::vector<std::string> AssetManager::AssetListGet(AssetType type) const
{
	return AssetListGet({ type });
}

bool AssetManager::FileRead(const std::string& virtual_path, std::vector<uint8_t>& out_data)
{
	return m_cls_Vfs.FileRead(virtual_path, out_data);
}

bool AssetManager::TextRead(const std::string& virtual_path, std::string& out_text)
{
	return m_cls_Vfs.TextRead(virtual_path, out_text);
}

bool AssetManager::FileExists(const std::string& virtual_path) const
{
	return m_cls_Vfs.FileExists(virtual_path);
}

std::vector<FileInfo> AssetManager::FileListGet(const std::string& prefix) const
{
	return m_cls_Vfs.FileListGet(prefix);
}

/*
* 関数名　WatchStart
* 引　数　interval：走査間隔
* 戻り値　なし
* 説　明　監視対象ディレクトリの変更検出を開始する
*/
void AssetManager::WatchStart(std::chrono::milliseconds interval)
{
	m_cls_Vfs.WatchStart(interval);
}

void AssetManager::WatchStop()
{
	m_cls_Vfs.WatchStop();
}

/*
* 関数名　ListenerAdd
* 引　数　listener：変更通知を受け取る関数
* 戻り値　解除用の ID
* 説　明　ファイル変更の通知先を登録する
*/
int AssetManager::ListenerAdd(ChangeListener listener)
{
	std::lock_guard<std::mutex> lk(m_Mutex);
	const int id = m_i_NextListenerId++;
	m_Listeners.emplace_back(id, std::move(listener));
	return id;
}

void AssetManager::ListenerRemove(int id)
{
	std::lock_guard<std::mutex> lk(m_Mutex);
	m_Listeners.erase(std::remove_if(m_Listeners.begin(), m_Listeners.end(),
		[id](const auto& p) { return p.first == id; }), m_Listeners.end());
}

/*
* 関数名　ChangesDispatch
* 引　数　なし
* 戻り値　なし
* 説　明　監視スレッドが検出した変更をメインスレッドでリスナーへ配信する
*         （GPU リソースの再生成はメインスレッドで行う必要があるため）
*/
void AssetManager::ChangesDispatch()
{
	std::vector<FileChange> changes;
	if (m_cls_Vfs.ChangesPoll(changes) == 0) return;

	std::vector<std::pair<int, ChangeListener>> listeners;
	{
		std::lock_guard<std::mutex> lk(m_Mutex);
		listeners = m_Listeners;
		for (auto& c : changes) {
			if (m_RecentChanges.size() >= MAX_RECENT_CHANGES) m_RecentChanges.pop_front();
			m_RecentChanges.push_back({ c, m_cls_Vfs.ScanCountGet() });
		}
	}
	for (auto& c : changes) {
		for (auto& l : listeners) l.second(c);
	}
}

/*
* 関数名　DebugGUIDraw
* 引　数　なし
* 戻り値　なし
* 説　明　デバッグ用GUIを描画する
*/
void AssetManager::DebugGUIDraw()
{
	ImGui::TextUnformatted("AssetManager");
	ImGui::Separator();
	ImGui::TextUnformatted("Mounts (上ほど優先):");
	for (auto& name : m_cls_Vfs.SourceNamesGet()) ImGui::BulletText("%s", name.c_str());

	const auto st = m_cls_Vfs.CacheStatsGet();
	ImGui::Text("Raw Cache: %zu files, %.2f / %.2f MB (hit %llu / miss %llu)",
		st.i_Entries, st.i_Bytes / (1024.0 * 1024.0), st.i_Budget / (1024.0 * 1024.0),
		(unsigned long long)st.i_Hits, (unsigned long long)st.i_Misses);
	ImGui::Text("AutoSync: %s  ScanCount: %llu", IsWatching() ? "Running" : "Stopped",
		(unsigned long long)m_cls_Vfs.ScanCountGet());

	static char filter[128] = "";
	ImGui::InputText("Filter (substring)", filter, sizeof(filter));
	if (ImGui::Button("Clear Raw Cache")) m_cls_Vfs.CacheClear();
	ImGui::SameLine();
	std::lock_guard<std::mutex> lk(m_Mutex);
	if (ImGui::Button("Clear Change Log")) m_RecentChanges.clear();

	ImGui::Separator();
	ImGui::TextUnformatted("Recent Changes:");
	ImGui::BeginChild("AssetManagerChanges", ImVec2(0, 120), true);
	for (auto it = m_RecentChanges.rbegin(); it != m_RecentChanges.rend(); ++it) {
		const char* t = (it->change.e_Type == FileChange::Type::Added) ? "ADD"
			: (it->change.e_Type == FileChange::Type::Removed) ? "DEL" : "MOD";
		if (filter[0] && it->change.path.find(filter) == std::string::npos) continue;
		ImGui::Text("[%s] %s (scan=%llu)", t, it->change.path.c_str(), (unsigned long long)it->i_Scan);
	}
	ImGui::EndChild();

	ImGui::Separator();
	ImGui::TextUnformatted("Files:");
	ImGui::BeginChild("AssetManagerFileList", ImVec2(0, 160), true);
	for (auto& info : m_cls_Vfs.FileListGet("")) {
		if (filter[0] && info.path.find(filter) == std::string::npos) continue;
		ImGui::Text("%-12s %s (%llu bytes)", AssetTypeUtil::NameGet(AssetTypeUtil::TypeFromPath(info.path)),
			info.path.c_str(), (unsigned long long)info.i_Size);
	}
	ImGui::EndChild();
}
