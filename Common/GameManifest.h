/*
* ファイル名　GameManifest
* 説　　　明　ビルドされたゲームの起動設定 (game.json)
*             ランタイム (Exe) が起動時に読み込み、エンジンへ渡す
*/
#ifndef GAME_MANIFEST_H
#define GAME_MANIFEST_H

#include <string>

// 描画バックエンド
enum class GraphicsBackend : int {
	D3D11 = 0,
	D3D12 = 1,
	Vulkan = 2,
};

namespace GraphicsBackendUtil {
	const char* NameGet(GraphicsBackend backend);
	// "DX11" "D3D11" "dx12" "vulkan" などを解釈する。不明なら false
	bool NameParse(const std::string& name, GraphicsBackend& out_backend);
}

struct GameManifest {
	std::string name = "MyGame";
	std::string version = "1.0.0";
	std::string startScene = "SampleScene";
	std::string archive = "data.pak";             // exe からの相対パス
	GraphicsBackend backend = GraphicsBackend::D3D11;
	int  i_Width = 1920;
	int  i_Height = 1080;
	bool b_Fullscreen = false;
	bool b_VSync = true;

	bool FileLoad(const std::string& path, std::string* out_error = nullptr);
	bool FileSave(const std::string& path) const;
	std::string JsonToString() const;
	bool JsonFromString(const std::string& text, std::string* out_error = nullptr);
};

#endif // !GAME_MANIFEST_H
