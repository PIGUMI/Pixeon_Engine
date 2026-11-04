/*
* ファイル名　AssetType
* 説　　　明　アセット種別の定義と、拡張子からの種別判定を一元化する
*             （以前は AssetManager / ResourceService / GUI などに拡張子リストが散在していた）
*/
#ifndef ASSET_TYPE_H
#define ASSET_TYPE_H

#include <cstdint>
#include <string>

enum class AssetType : uint8_t {
	Unknown = 0,
	Texture,       // png jpg jpeg tga dds bmp hdr
	Model,         // fbx obj gltf glb
	Sound,         // wav mp3 ogg
	Effect,        // efkefc efk
	Scene,         // scene
	ShaderSource,  // hlsl fx hlsli
	ShaderBinary,  // cso spv
	Script,        // dll cpp h
	Font,          // ttf otf
	Config,        // json sceneconfig ini
	Animation2D,   // anim2d
	Count
};

namespace AssetTypeUtil {
	// 拡張子（ドット無し・大小文字不問）から種別を判定する
	AssetType TypeFromExtension(const std::string& extension);

	// パスの拡張子から種別を判定する
	AssetType TypeFromPath(const std::string& path);

	// 種別の表示名（"Texture" など）
	const char* NameGet(AssetType type);

	// パスから小文字の拡張子（ドット無し）を取り出す
	std::string ExtensionGet(const std::string& path);

	// ゲームビルドのアーカイブに含めるべき種別か
	bool IsRuntimeAsset(AssetType type);
}

#endif // !ASSET_TYPE_H
