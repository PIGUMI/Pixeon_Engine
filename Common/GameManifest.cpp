/*
* ファイル名　GameManifest
* 説　　　明　game.json の読み書き
*/
#include "GameManifest.h"
#include "FileUtil.h"
#include <nlohmann/json.hpp>

namespace GraphicsBackendUtil {

	/*
	* 関数名　NameGet
	* 引　数　backend：バックエンド
	* 戻り値　表示名
	* 説　明　バックエンドの名前を返す
	*/
	const char* NameGet(GraphicsBackend backend)
	{
		switch (backend) {
		case GraphicsBackend::D3D12:  return "DX12";
		case GraphicsBackend::Vulkan: return "Vulkan";
		default:                      return "DX11";
		}
	}

	/*
	* 関数名　NameParse
	* 引　数　name：名前　out_backend：出力
	* 戻り値　解釈できたら true
	* 説　明　大小文字や表記揺れ（DX11 / D3D11 など）を許容して解釈する
	*/
	bool NameParse(const std::string& name, GraphicsBackend& out_backend)
	{
		std::string s;
		for (char c : name) if (c != ' ' && c != '_' && c != '-') s.push_back((char)tolower((unsigned char)c));
		if (s == "dx11" || s == "d3d11" || s == "directx11") { out_backend = GraphicsBackend::D3D11; return true; }
		if (s == "dx12" || s == "d3d12" || s == "directx12") { out_backend = GraphicsBackend::D3D12; return true; }
		if (s == "vulkan" || s == "vk") { out_backend = GraphicsBackend::Vulkan; return true; }
		return false;
	}
}

/*
* 関数名　JsonToString
* 引　数　なし
* 戻り値　JSON 文字列
* 説　明　マニフェストを JSON にする
*/
std::string GameManifest::JsonToString() const
{
	nlohmann::json j;
	j["Name"] = name;
	j["Version"] = version;
	j["StartScene"] = startScene;
	j["Archive"] = archive;
	j["Graphics"]["Backend"] = GraphicsBackendUtil::NameGet(backend);
	j["Graphics"]["Width"] = i_Width;
	j["Graphics"]["Height"] = i_Height;
	j["Graphics"]["Fullscreen"] = b_Fullscreen;
	j["Graphics"]["VSync"] = b_VSync;
	return j.dump(4);
}

/*
* 関数名　JsonFromString
* 引　数　text：JSON 文字列　out_error：エラー内容（任意）
* 戻り値　成功なら true
* 説　明　JSON を解釈する。存在しない項目は既定値のまま
*/
bool GameManifest::JsonFromString(const std::string& text, std::string* out_error)
{
	try {
		const nlohmann::json j = nlohmann::json::parse(text);
		name = j.value("Name", name);
		version = j.value("Version", version);
		startScene = j.value("StartScene", startScene);
		archive = j.value("Archive", archive);
		if (j.contains("Graphics")) {
			const auto& g = j["Graphics"];
			const std::string backend_name = g.value("Backend", std::string(GraphicsBackendUtil::NameGet(backend)));
			if (!GraphicsBackendUtil::NameParse(backend_name, backend)) {
				if (out_error) *out_error = "不明なバックエンド: " + backend_name;
				return false;
			}
			i_Width = g.value("Width", i_Width);
			i_Height = g.value("Height", i_Height);
			b_Fullscreen = g.value("Fullscreen", b_Fullscreen);
			b_VSync = g.value("VSync", b_VSync);
		}
		return true;
	}
	catch (const std::exception& e) {
		if (out_error) *out_error = e.what();
		return false;
	}
}

/*
* 関数名　FileLoad
* 引　数　path：ファイルパス　out_error：エラー内容（任意）
* 戻り値　成功なら true
* 説　明　game.json を読み込む
*/
bool GameManifest::FileLoad(const std::string& path, std::string* out_error)
{
	std::string text;
	if (!FileUtil::TextReadAll(path, text)) {
		if (out_error) *out_error = "読み込めません: " + path;
		return false;
	}
	return JsonFromString(text, out_error);
}

/*
* 関数名　FileSave
* 引　数　path：ファイルパス
* 戻り値　成功なら true
* 説　明　game.json を書き込む
*/
bool GameManifest::FileSave(const std::string& path) const
{
	const std::string text = JsonToString();
	return FileUtil::FileWriteAll(path, text.data(), text.size());
}
