/*
* ファイル名　: ResourceService
* 説　　　明　: リソース管理サービス実装
*/
#include "ResourceService.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "SoundManager.h"

ResourceService* ResourceService::_instance = nullptr;

/*
* 関数名　: Instance
* 引　数　: なし
* 戻り値　: シングルトンインスタンスポインター
* 説　明　: シングルトンインスタンスを取得する
*/
ResourceService& ResourceService::Instance() {
	if (!_instance) {
		_instance = new ResourceService();
	}
	return *_instance;
}

/*
* 関数名　: DeleteInstance
* 引　数　: なし
* 戻り値　: なし
* 説　明　: シングルトンインスタンスを破棄する
*/
void ResourceService::DeleteInstance() {
	if (_instance) {
		delete _instance;
		_instance = nullptr;
	}
}

/*
* 関数名　: GetTexture
* 引　数　: name		[in] ロジカル名
* 戻り値　: テクスチャリソースポインター
* 説　明　: テクスチャリソースを取得する
*/
std::shared_ptr<TextureResource> ResourceService::GetTexture(const std::string& name) {
	return TextureManager::Instance()->LoadOrGet(name);
}

/*
* 関数名　: GetModel
* 引　数　: name		[in] ロジカル名
* 戻り値　: モデル共有リソースポインター
* 説　明　: モデル共有リソースを取得する
*/
std::shared_ptr<ModelSharedResource> ResourceService::GetModel(const std::string& name) {
	return ModelManager::Instance()->LoadOrGet(name);
}

/*
* 関数名　: GetSound
* 引　数　: name		[in] ロジカル名
* 　　　　: streaming	[in] ストリーミング再生フラグ
* 戻り値　: サウンドリソースポインター
* 説　明　: サウンドリソースを取得する
*/
std::shared_ptr<SoundResource> ResourceService::GetSound(const std::string& name, bool streaming) {
	return SoundManager::Instance()->LoadOrGet(name, streaming);
}

/*
* 関数名　: AutoResolve
* 引　数　: name		[in] ロジカル名
* 戻り値　: 解決成功フラグ
* 説　明　: 拡張子から自動的にリソース種別を判別して取得する
*/
bool ResourceService::AutoResolve(const std::string& name) {
	std::string ext;
	if (auto p = name.find_last_of('.'); p != std::string::npos)
		ext = name.substr(p + 1);
	for (auto& c : ext) c = (char)tolower(c);

	if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "dds" || ext == "tga")
		return (bool)GetTexture(name);
	if (ext == "fbx" || ext == "obj" || ext == "gltf" || ext == "glb")
		return (bool)GetModel(name);
	if (ext == "wav" || ext == "mp3" || ext == "ogg")
		return (bool)GetSound(name);
	return false;
}