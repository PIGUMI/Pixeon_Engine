/*
* ファイル名 : SoundManager
* 説　　　明 : サウンドリソース管理クラス
*/
#include "SoundManager.h"
#include "AssetManager.h"
#include <Windows.h>
#include "IMGUI/imgui.h"

SoundManager* SoundManager::_instance = nullptr;

/*
* 関数名　: Instance
* 引　数　: なし
* 戻り値　: シングルトンインスタンスポインター
* 説　明　: インスタンス取得
*/
SoundManager* SoundManager::Instance()
{
	if (!_instance) {
		_instance = new SoundManager();
	}
	return _instance;
}

/*
* 関数名　: DeleteInstance
* 引　数　: なし
* 戻り値　: なし
* 説　明　: インスタンス破棄
*/
void SoundManager::DeleteInstance() {
	if (_instance) {
		_instance->UnInit();
		delete _instance;
		_instance = nullptr;
	}
}

/*
* 関数名　: UnInit
* 引　数　: なし
* 戻り値　: なし
* 説　明　: 破棄処理
*/
void SoundManager::UnInit() {
	std::lock_guard<std::mutex> lk(_mtx);
	_cache.clear();
	_frame = 0;
}

/*
* 関数名　: LoadOrGet
* 引　数　: logicalName  論理名
* 　　　　: streaming    ストリーミングフラグ
* 戻り値　: サウンドリソースポインター
* 説　明　: サウンドリソースの取得（キャッシュ有効）
*/
std::shared_ptr<SoundResource> SoundManager::LoadOrGet(const std::string& logicalName, bool streaming) {
	std::lock_guard<std::mutex> lk(_mtx);
	_frame++;
	auto it = _cache.find(logicalName);
	if (it != _cache.end()) {
		if (auto sp = it->second.weak.lock()) {
			it->second.lastUse = _frame;
			return sp;
		}
	}
	auto snd = LoadInternal(logicalName, streaming);
	if (snd) {
		Entry e;
		e.weak = snd;
		e.lastUse = _frame;
		e.bytes = snd->pcmData.size();
		e.streaming = streaming;
		_cache[logicalName] = e;
	}
	return snd;
}

/*
* 関数名　: LoadInternal
* 引　数　: logicalName  論理名
* 　　　　: streaming    ストリーミングフラグ
* 戻り値　: サウンドリソースポインター
* 説　明　: サウンドリソースの内部読み込み処理
*/
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

/*
* 関数名　: GarbageCollect
* 引　数　: なし
* 戻り値　: なし
* 説　明　: ガベージコレクション
*/
void SoundManager::GarbageCollect() {
	std::lock_guard<std::mutex> lk(_mtx);
	for (auto it = _cache.begin(); it != _cache.end(); ) {
		if (it->second.weak.expired()) {
			it = _cache.erase(it);
		}
		else {
			++it;
		}
	}
}

/*
* 関数名　: DrawDebugGUI
* 引　数　: なし
* 戻り値　: なし
* 説　明　: デバッグGUI描画
*/
void SoundManager::DrawDebugGUI() {
	std::lock_guard<std::mutex> lk(_mtx);
	ImGui::TextUnformatted("SoundManager");
	ImGui::Separator();
	size_t alive = 0;
	size_t total = 0;
	for (auto& kv : _cache) {
		if (!kv.second.weak.expired()) {
			alive++;
			total += kv.second.bytes;
		}
	}
	ImGui::Text("Cached: %zu (alive=%zu)", _cache.size(), alive);
	ImGui::Text("PCM Approx: %.2f MB", total / (1024.0 * 1024.0));
	static char filter[128] = "";
	ImGui::InputText("Filter##Sound", filter, sizeof(filter));
	if (ImGui::Button("GC Dead##Sound")) {
		for (auto it = _cache.begin(); it != _cache.end();) {
			if (it->second.weak.expired()) it = _cache.erase(it);
			else ++it;
		}
	}
	ImGui::Separator();
	ImGui::BeginChild("SoundList", ImVec2(0, 160), true);
	for (auto& kv : _cache) {
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