#include "TextureManager.h"
#include "AssetManager.h"
#include "System.h"
#include "DirectXTex/DirectXTex.h"
#include "IMGUI/imgui.h"
#include <algorithm>
#include <Windows.h>

TextureManager* TextureManager::_instance = nullptr;

/*
* 関数名　： Instance
* 引　数　： なし
* 戻り値　： シングルトンインスタンス
* 概　要　： TextureManager のシングルトンインスタンスを取得
*/
TextureManager* TextureManager::Instance() {
	if (!_instance) _instance = new TextureManager();
	return _instance;
}

/*
* 関数名　： DeleteInstance
* 引　数　： なし
* 戻り値　： なし
* 概　要　： TextureManager のシングルトンインスタンスを破棄
*/
void TextureManager::DeleteInstance() {
	if (_instance) {
		_instance->UnInit();
		delete _instance;
		_instance = nullptr;
	}
}

/*
* 関数名　： UnInit
* 引　数　： なし
* 戻り値　： なし
* 概　要　： テクスチャマネージャの終了処理
*/
void TextureManager::UnInit() {
	std::lock_guard<std::mutex> lk(_mtx);
	_cache.clear();
	_pinned.clear();
	_failReasons.clear();
	_frame = 0;
}

/*
* 関数名　： SetFail
* 引　数　： name   失敗したテクスチャ名
*         ： reason 失敗理由
* 戻り値　： なし
* 概　要　： 指定したテクスチャの読み込み失敗理由を設定
*/
void TextureManager::SetFail(const std::string& name, const std::string& reason) {
	_failReasons[name] = reason;
}

/*
* 関数名　： GetLastFailReason
* 引　数　： name 失敗したテクスチャ名
* 戻り値　： 失敗理由文字列（存在しない場合は空文字列）
* 概　要　： 指定したテクスチャの最後の読み込み失敗理由を取得
*/
std::string TextureManager::GetLastFailReason(const std::string& name) const {
	auto it = _failReasons.find(name);
	return it == _failReasons.end() ? "" : it->second;
}

/*
* 関数名　： LoadOrGet
* 引　数　： logicalName 論理名（アセット名）
* 戻り値　： テクスチャリソース共有ポインタ（失敗時は nullptr）
* 概　要　： テクスチャを読み込み、もしくはキャッシュから取得
*/
std::shared_ptr<TextureResource> TextureManager::LoadOrGet(const std::string& logicalName) {
	std::lock_guard<std::mutex> lk(_mtx);
	_frame++;
	auto it = _cache.find(logicalName);
	if (it != _cache.end()) {
		if (auto sp = it->second.weak.lock()) {
			it->second.lastUse = _frame;
			return sp;
		}
	}
	auto tex = LoadInternal(logicalName);
	if (tex) {
		Entry e;
		e.weak = tex;
		e.lastUse = _frame;
		e.bytes = tex->gpuBytes;
		_cache[logicalName] = e;
		_failReasons.erase(logicalName);
	}
	return tex;
}

/*
* 関数名　： LoadTexture
* 引　数　： name テクスチャ名
* 戻り値　： 読み込み成功なら true、失敗なら false
* 概　要　： テクスチャを読み込み、ピン留めする
*/
bool TextureManager::LoadTexture(const std::string& name) {
	auto tex = LoadOrGet(name);
	if (tex && tex->srv) {
		std::lock_guard<std::mutex> lk(_mtx);
		_pinned[name] = tex;
		return true;
	}
	return false;
}

/*
* 関数名　： Reload
* 引　数　： name テクスチャ名
* 戻り値　： 再読み込み成功なら true、失敗なら false
* 概　要　： テクスチャを再読み込みする
*/
bool TextureManager::Reload(const std::string& name) {
	{
		std::lock_guard<std::mutex> lk(_mtx);
		_cache.erase(name);
		_pinned.erase(name);
		_failReasons.erase(name);
	}
	return LoadTexture(name);
}

/*
* 関数名　： RemoveFromCache
* 引　数　： name テクスチャ名
* 戻り値　： なし
* 概　要　： テクスチャをキャッシュから削除する
*/
bool TextureManager::RemoveFromCache(const std::string& name) {
	std::lock_guard<std::mutex> lk(_mtx);
	_cache.erase(name);
	_pinned.erase(name);
	_failReasons.erase(name);
	return true;
}

/*
* 関数名　： IsLoaded
* 引　数　： name テクスチャ名
* 戻り値　： 読み込み済みなら true、未読み込みなら false
* 概　要　： テクスチャが読み込み済みかどうかを取得
*/
bool TextureManager::IsLoaded(const std::string& name) {
	std::lock_guard<std::mutex> lk(_mtx);
	auto it = _cache.find(name);
	return (it != _cache.end() && !it->second.weak.expired());
}

/*
* 関数名　： Pin
* 引　数　： name テクスチャ名
* 戻り値　： ピン留め成功なら true、失敗なら false
* 概　要　： テクスチャをピン留めする
*/
bool TextureManager::Pin(const std::string& name) {
	std::lock_guard<std::mutex> lk(_mtx);
	auto it = _cache.find(name);
	if (it == _cache.end()) return false;
	if (auto sp = it->second.weak.lock()) {
		_pinned[name] = sp;
		return true;
	}
	return false;
}

/*
* 関数名　： Unpin
* 引　数　： name テクスチャ名
* 戻り値　： ピン留め解除成功なら true、失敗なら false
* 概　要　： テクスチャのピン留めを解除する
*/
bool TextureManager::Unpin(const std::string& name) {
	std::lock_guard<std::mutex> lk(_mtx);
	return _pinned.erase(name) > 0;
}

/*
* 関数名　： IsPinned
* 引　数　： name テクスチャ名
* 戻り値　： ピン留めされていれば true、されていなければ false
* 概　要　： テクスチャがピン留めされているかどうかを取得
*/
bool TextureManager::IsPinned(const std::string& name) {
	std::lock_guard<std::mutex> lk(_mtx);
	return _pinned.find(name) != _pinned.end();
}

/*
* 関数名　： LoadInternal
* 引　数　： logicalName 論理名（アセット名）
* 戻り値　： テクスチャリソース共有ポインタ（失敗時は nullptr）
* 概　要　： テクスチャを内部的に読み込み
*/
std::shared_ptr<TextureResource> TextureManager::LoadInternal(const std::string& logicalName) {
	// (1) Raw  ǂݍ
	std::vector<uint8_t> data;
	if (!AssetManager::Instance()->LoadAsset(logicalName, data) || data.empty()) {
		SetFail(logicalName, "RawLoadFailed(size=0 or not found)");
		return nullptr;
	}

	// (2)  g   q
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

	// (3) Mip      (   s ͌x   ̂ )
	if (img.GetMetadata().mipLevels <= 1) {
		DirectX::ScratchImage mip;
		HRESULT hrMip = DirectX::GenerateMipMaps(img.GetImages(), img.GetImageCount(), img.GetMetadata(),
			DirectX::TEX_FILTER_DEFAULT, 0, mip);
		if (SUCCEEDED(hrMip)) {
			img = std::move(mip);
		}
	}

	// (4)  f o C X m F
	auto dev = DirectX11::GetInstance()->GetDevice();
	if (!dev) {
		SetFail(logicalName, "DeviceNull");
		return nullptr;
	}

	// (5) SRV  쐬
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

/*
* 関数名　： GarbageCollect
* 引　数　： なし
* 戻り値　： なし
* 概　要　： ガベージコレクションを実行（参照されていないテクスチャをキャッシュから削除）
*/
void TextureManager::GarbageCollect() {
	std::lock_guard<std::mutex> lk(_mtx);
	for (auto it = _cache.begin(); it != _cache.end();) {
		if (it->second.weak.expired() && _pinned.find(it->first) == _pinned.end()) {
			it = _cache.erase(it);
		}
		else {
			++it;
		}
	}
}

/*
* 関数名　: DrawDebugGUI
* 引　数　： なし
* 戻り値　： なし
* 概　要　： デバッグ用 GUI を描画
*/
void TextureManager::DrawDebugGUI() {
	std::lock_guard<std::mutex> lk(_mtx);
	ImGui::TextUnformatted("TextureManager");
	ImGui::Separator();

	size_t alive = 0;
	size_t totalBytes = 0;
	for (auto& kv : _cache) {
		if (auto sp = kv.second.weak.lock()) {
			alive++;
			totalBytes += kv.second.bytes;
		}
	}
	ImGui::Text("Cached Entries: %zu (alive=%zu)", _cache.size(), alive);
	ImGui::Text("Pinned: %zu", _pinned.size());
	ImGui::Text("GPU Approx Total: %.2f MB", totalBytes / (1024.0 * 1024.0));
	ImGui::Text("Memory Budget: %.2f MB", _budget / (1024.0 * 1024.0));

	if (ImGui::Button("GC (Dead Only)")) {
		for (auto it = _cache.begin(); it != _cache.end();) {
			if (it->second.weak.expired() && _pinned.find(it->first) == _pinned.end())
				it = _cache.erase(it);
			else
				++it;
		}
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Cached List:");
	static char filterCache[128] = "";
	ImGui::InputText("Filter Cached", filterCache, sizeof(filterCache));
	ImGui::BeginChild("TM_CachedList", ImVec2(0, 110), true);
	for (auto& kv : _cache) {
		if (filterCache[0] && kv.first.find(filterCache) == std::string::npos) continue;
		bool aliveOne = !kv.second.weak.expired();
		bool pinned = (_pinned.find(kv.first) != _pinned.end());
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
		auto it = _cache.find(n);
		if (it == _cache.end() || it->second.weak.expired()) unloadedCount++;
	}
	ImGui::Text("Total Raw=%d  Loaded=%zu  Unloaded=%d  Failed=%zu",
		(int)assetTexList.size(), _cache.size(), unloadedCount, _failReasons.size());

	static int sel = -1;

	if (ImGui::Button("Load All Unloaded")) {
		for (auto& n : assetTexList) {
			auto it = _cache.find(n);
			bool need = (it == _cache.end()) || it->second.weak.expired();
			if (need) {
				auto tex = LoadInternal(n);
				if (tex) {
					Entry e;
					e.weak = tex;
					e.lastUse = ++_frame;
					e.bytes = tex->gpuBytes;
					_cache[n] = e;
					_pinned[n] = tex;
					_failReasons.erase(n);
				}
			}
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload Selected") && sel >= 0 && sel < (int)assetTexList.size()) {
		std::string name = assetTexList[sel];
		_cache.erase(name);
		_pinned.erase(name);
		_failReasons.erase(name);
		auto tex = LoadInternal(name);
		if (tex) {
			Entry e;
			e.weak = tex;
			e.lastUse = ++_frame;
			e.bytes = tex->gpuBytes;
			_cache[name] = e;
			_pinned[name] = tex;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Remove Selected") && sel >= 0 && sel < (int)assetTexList.size()) {
		std::string name = assetTexList[sel];
		_cache.erase(name);
		_pinned.erase(name);
		_failReasons.erase(name);
	}

	ImGui::BeginChild("TM_AssetList", ImVec2(0, 210), true);
	for (int i = 0; i < (int)assetTexList.size(); ++i) {
		const std::string& n = assetTexList[i];
		if (filterAsset[0] && n.find(filterAsset) == std::string::npos) continue;
		bool loaded = (_cache.find(n) != _cache.end()) && !_cache[n].weak.expired();
		bool pinned = (_pinned.find(n) != _pinned.end());
		bool failed = (_failReasons.find(n) != _failReasons.end());

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
				e.lastUse = ++_frame;
				e.bytes = tex->gpuBytes;
				_cache[n] = e;
				_pinned[n] = tex;
				_failReasons.erase(n);
			}
		}
	}
	ImGui::EndChild();

	if (sel >= 0 && sel < (int)assetTexList.size()) {
		ImGui::Separator();
		std::string selName = assetTexList[sel];
		ImGui::Text("Selected: %s", selName.c_str());
		bool loaded = (_cache.find(selName) != _cache.end()) && !_cache[selName].weak.expired();
		ImGui::Text("Loaded: %s  Pinned: %s", loaded ? "Yes" : "No",
			(_pinned.find(selName) != _pinned.end()) ? "Yes" : "No");
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
					e.lastUse = ++_frame;
					e.bytes = tex->gpuBytes;
					_cache[selName] = e;
					_pinned[selName] = tex;
					_failReasons.erase(selName);
				}
			}
		}
		else {
			if (ImGui::Button("Pin") && _pinned.find(selName) == _pinned.end()) {
				if (auto sp = _cache[selName].weak.lock())
					_pinned[selName] = sp;
			}
			ImGui::SameLine();
			if (ImGui::Button("Unpin")) {
				_pinned.erase(selName);
			}
			ImGui::SameLine();
			if (ImGui::Button("Reload")) {
				_cache.erase(selName);
				_pinned.erase(selName);
				_failReasons.erase(selName);
				auto tex = LoadInternal(selName);
				if (tex) {
					Entry e;
					e.weak = tex;
					e.lastUse = ++_frame;
					e.bytes = tex->gpuBytes;
					_cache[selName] = e;
					_pinned[selName] = tex;
				}
			}
		}
	}
}