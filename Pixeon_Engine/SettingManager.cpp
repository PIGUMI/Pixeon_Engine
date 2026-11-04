/*
* ファイル名　SettingManager
* 説　　　明　エンジン設定 (config.json) の読み書き
*             読み込みは仮想ファイルシステム経由（エディタ: SceneRoot/config/、ゲーム: アーカイブ内）
*             書き込みはエディタのみ
*/
#include "SettingManager.h"
#include "AssetManager.h"
#include "../Common/FileUtil.h"
#include <nlohmann/json.hpp>

namespace {
	constexpr const char* CONFIG_FILE_NAME = "config.json";
}

SettingManager* SettingManager::instance = nullptr;

SettingManager* SettingManager::GetInstance()
{
	if (instance == nullptr)
	{
		instance = new SettingManager();
	}
	return instance;
}

void SettingManager::DestroyInstance()
{
	if (instance != nullptr)
	{
		delete instance;
		instance = nullptr;
	}
}

/*
* 関数名　LoadConfig
* 引　数　なし
* 戻り値　なし
* 説　明　config.json を読み込む。ファイルが無い・壊れている場合は既定値のまま
*/
void SettingManager::LoadConfig() {
	std::string text;
	if (!AssetManager::Instance()->TextRead(std::string(VfsPrefix::CONFIG) + CONFIG_FILE_NAME, text)) return;

	nlohmann::json configJson = nlohmann::json::parse(text, nullptr, false);
	if (configJson.is_discarded() || !configJson.is_object()) return;

	AssetsFilePath = configJson.value("AssetsFilePath", AssetsFilePath);
	ArchiveFilePath = configJson.value("ArchiveFilePath", ArchiveFilePath);
	SceneFilePath = configJson.value("SceneFilePath", SceneFilePath);
	ShaderFilePath = configJson.value("ShaderFilePath", ShaderFilePath);
	CSOFilePath = configJson.value("CSOFilePath", CSOFilePath);
	SPIRVFilePath = configJson.value("SPIRVFilePath", SPIRVFilePath);
	BuildOutputPath = configJson.value("BuildOutputPath", BuildOutputPath);
	bZBuffer = configJson.value("bZBuffer", bZBuffer);
	AutoSaveInterval = configJson.value("AutoSaveInterval", AutoSaveInterval);
	ExternelTool = configJson.value("ExternelTool", ExternelTool);
	MouseSensitivity = configJson.value("MouseSensitivity", MouseSensitivity);
	VcpkgFilePath = configJson.value("VcpkgFilePath", VcpkgFilePath);

	if (configJson.contains("BackgroundColor") && configJson["BackgroundColor"].is_array() && configJson["BackgroundColor"].size() == 4) {
		BackgroundColor.x = configJson["BackgroundColor"][0].get<float>();
		BackgroundColor.y = configJson["BackgroundColor"][1].get<float>();
		BackgroundColor.z = configJson["BackgroundColor"][2].get<float>();
		BackgroundColor.w = configJson["BackgroundColor"][3].get<float>();
	}
	if (configJson.contains("EditorGraphicsBackend")) {
		GraphicsBackendUtil::NameParse(configJson["EditorGraphicsBackend"].get<std::string>(), EditorGraphicsBackend);
	}
}

/*
* 関数名　SaveConfig
* 引　数　なし
* 戻り値　なし
* 説　明　config.json を書き込む（エディタのみ）
*/
void SettingManager::SaveConfig() {
	nlohmann::json configJson;
	configJson["AssetsFilePath"] = AssetsFilePath;
	configJson["ArchiveFilePath"] = ArchiveFilePath;
	configJson["SceneFilePath"] = SceneFilePath;
	configJson["ShaderFilePath"] = ShaderFilePath;
	configJson["CSOFilePath"] = CSOFilePath;
	configJson["SPIRVFilePath"] = SPIRVFilePath;
	configJson["BuildOutputPath"] = BuildOutputPath;
	configJson["bZBuffer"] = bZBuffer;
	configJson["AutoSaveInterval"] = AutoSaveInterval;
	configJson["BackgroundColor"] = { BackgroundColor.x, BackgroundColor.y, BackgroundColor.z, BackgroundColor.w };
	configJson["ExternelTool"] = ExternelTool;
	configJson["MouseSensitivity"] = MouseSensitivity;
	configJson["VcpkgFilePath"] = VcpkgFilePath;
	configJson["EditorGraphicsBackend"] = GraphicsBackendUtil::NameGet(EditorGraphicsBackend);

	const std::string text = configJson.dump(4);
	FileUtil::FileWriteAll(ConfigDirectoryPath + CONFIG_FILE_NAME, text.data(), text.size());
}
