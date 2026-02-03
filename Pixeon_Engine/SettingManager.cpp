#include "SettingManager.h"
#include "MainFrame.h"
#include <nlohmann/json.hpp>
#include <fstream>

#define CONFIG_FILE_PATH "SceneRoot/config/config.json"

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

// Configの読み込み
void SettingManager::LoadConfig() {
	// JSONファイルを読み込む
	std::ifstream configFile(CONFIG_FILE_PATH);
	if (!configFile.is_open()) {
		// ファイルが存在しない場合、デフォルト設定を使用
		return;
	}
	nlohmann::json configJson;
	configFile >> configJson;
	// 設定項目を読み込む
	if (configJson.contains("AssetsFilePath")) {
		AssetsFilePath = configJson["AssetsFilePath"].get<std::string>();
	}
	if (configJson.contains("ArchiveFilePath")) {
		ArchiveFilePath = configJson["ArchiveFilePath"].get<std::string>();
	}
	if (configJson.contains("SceneFilePath")) {
		SceneFilePath = configJson["SceneFilePath"].get<std::string>();
	}
	if (configJson.contains("bZBuffer")) {
		bZBuffer = configJson["bZBuffer"].get<bool>();
	}
	if (configJson.contains("AutoSaveInterval")) {
		AutoSaveInterval = configJson["AutoSaveInterval"].get<int>();
	}
	if (configJson.contains("BackgroundColor") && configJson["BackgroundColor"].is_array() && configJson["BackgroundColor"].size() == 4) {
		BackgroundColor.x = configJson["BackgroundColor"][0].get<float>();
		BackgroundColor.y = configJson["BackgroundColor"][1].get<float>();
		BackgroundColor.z = configJson["BackgroundColor"][2].get<float>();
		BackgroundColor.w = configJson["BackgroundColor"][3].get<float>();
	}
	if (configJson.contains("PixelPostEffect")) {
		MainFrame::GetInstance()->setPixelated(configJson["PixelPostEffect"].get<bool>());
	}
	if (configJson.contains("ExternelTool")) {
		ExternelTool = configJson["ExternelTool"].get<std::string>();
	}
	if (configJson.contains("MouseSensitivity")) {
		MouseSensitivity = configJson["MouseSensitivity"].get<float>();
	}
	if (configJson.contains("VcpkgFilePath")) {
		VcpkgFilePath = configJson["VcpkgFilePath"].get<std::string>();
	}
	
}

void SettingManager::SaveConfig() {
	// 設定項目をJSONオブジェクトに保存
	nlohmann::json configJson;
	configJson["AssetsFilePath"] = AssetsFilePath;
	configJson["ArchiveFilePath"] = ArchiveFilePath;
	configJson["SceneFilePath"] = SceneFilePath;
	configJson["bZBuffer"] = bZBuffer;
	configJson["AutoSaveInterval"] = AutoSaveInterval;
	configJson["BackgroundColor"] = { BackgroundColor.x, BackgroundColor.y, BackgroundColor.z, BackgroundColor.w };
	configJson["ExternelTool"] = ExternelTool;
	configJson["PixelPostEffect"] = MainFrame::GetInstance()->isPixelated();
	configJson["MouseSensitivity"] = MouseSensitivity;
	configJson["VcpkgFilePath"] = VcpkgFilePath;

	// JSONファイルに書き込む
	std::ofstream configFile(CONFIG_FILE_PATH);
	if (configFile.is_open()) {
		configFile << configJson.dump(4); // インデントを4スペースに設定して保存
	}
}