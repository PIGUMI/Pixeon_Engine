/*
* ファイル名　AssetType
* 説　　　明　アセット種別判定の実装
*/
#include "AssetType.h"
#include <unordered_map>

namespace AssetTypeUtil {

	/*
	* 関数名　ExtensionGet
	* 引　数　path：パス
	* 戻り値　小文字の拡張子（ドット無し）
	* 説　明　パスから拡張子を取り出す
	*/
	std::string ExtensionGet(const std::string& path)
	{
		const size_t slash = path.find_last_of("/\\");
		const size_t dot = path.find_last_of('.');
		if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return "";
		std::string ext = path.substr(dot + 1);
		for (auto& c : ext) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
		return ext;
	}

	/*
	* 関数名　TypeFromExtension
	* 引　数　extension：拡張子
	* 戻り値　アセット種別
	* 説　明　拡張子表を引いて種別を返す
	*/
	AssetType TypeFromExtension(const std::string& extension)
	{
		static const std::unordered_map<std::string, AssetType> table = {
			{ "png", AssetType::Texture }, { "jpg", AssetType::Texture }, { "jpeg", AssetType::Texture },
			{ "tga", AssetType::Texture }, { "dds", AssetType::Texture }, { "bmp", AssetType::Texture },
			{ "hdr", AssetType::Texture },
			{ "fbx", AssetType::Model }, { "obj", AssetType::Model }, { "gltf", AssetType::Model },
			{ "glb", AssetType::Model },
			{ "wav", AssetType::Sound }, { "mp3", AssetType::Sound }, { "ogg", AssetType::Sound },
			{ "efkefc", AssetType::Effect }, { "efk", AssetType::Effect },
			{ "scene", AssetType::Scene },
			{ "hlsl", AssetType::ShaderSource }, { "hlsli", AssetType::ShaderSource }, { "fx", AssetType::ShaderSource },
			{ "cso", AssetType::ShaderBinary }, { "spv", AssetType::ShaderBinary },
			{ "dll", AssetType::Script }, { "cpp", AssetType::Script }, { "h", AssetType::Script },
			{ "ttf", AssetType::Font }, { "otf", AssetType::Font },
			{ "json", AssetType::Config }, { "sceneconfig", AssetType::Config }, { "ini", AssetType::Config },
			{ "anim2d", AssetType::Animation2D },
		};
		std::string ext = extension;
		if (!ext.empty() && ext.front() == '.') ext.erase(0, 1);
		for (auto& c : ext) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
		auto it = table.find(ext);
		return (it != table.end()) ? it->second : AssetType::Unknown;
	}

	/*
	* 関数名　TypeFromPath
	* 引　数　path：パス
	* 戻り値　アセット種別
	* 説　明　パスの拡張子から種別を判定する
	*/
	AssetType TypeFromPath(const std::string& path)
	{
		return TypeFromExtension(ExtensionGet(path));
	}

	/*
	* 関数名　NameGet
	* 引　数　type：種別
	* 戻り値　表示名
	* 説　明　種別の表示名を返す
	*/
	const char* NameGet(AssetType type)
	{
		switch (type) {
		case AssetType::Texture:      return "Texture";
		case AssetType::Model:        return "Model";
		case AssetType::Sound:        return "Sound";
		case AssetType::Effect:       return "Effect";
		case AssetType::Scene:        return "Scene";
		case AssetType::ShaderSource: return "ShaderSource";
		case AssetType::ShaderBinary: return "ShaderBinary";
		case AssetType::Script:       return "Script";
		case AssetType::Font:         return "Font";
		case AssetType::Config:       return "Config";
		case AssetType::Animation2D:  return "Animation2D";
		default:                      return "Unknown";
		}
	}

	/*
	* 関数名　IsRuntimeAsset
	* 引　数　type：種別
	* 戻り値　ゲーム実行時に必要なら true
	* 説　明　ソースコードやシェーダーソースなど、実行時に不要な種別を除外する
	*         （スクリプト DLL は LoadLibrary で読むためアーカイブには入れず別途コピーする）
	*/
	bool IsRuntimeAsset(AssetType type)
	{
		switch (type) {
		case AssetType::ShaderSource:
		case AssetType::Script:
			return false;
		default:
			return true;
		}
	}
}
