// 設定管理マネージャー
// シングルトンパターン
// jsonにて保存

#pragma once
#include <string>
#include "System.h"

class SettingManager
{
public:

	static SettingManager* GetInstance();
	static void DestroyInstance();

	void LoadConfig();
	void SaveConfig();

public:
	// 設定項目のゲッターとセッター
	std::string GetAssetsFilePath() const { return AssetsFilePath; }
	void SetAssetsFilePath(const std::string& path) { AssetsFilePath = path; }

	std::string GetArchiveFilePath() const { return ArchiveFilePath; }
	void SetArchiveFilePath(const std::string& path) { ArchiveFilePath = path; }

	std::string GetSceneFilePath() const { return SceneFilePath; }
	void SetSceneFilePath(const std::string& path) { SceneFilePath = path; }

	bool GetZBuffer() const { return bZBuffer; }
	void SetZBuffer(bool bEnable) { bZBuffer = bEnable; }

	int GetAutoSaveInterval() const { return AutoSaveInterval; }
	void SetAutoSaveInterval(int interval) { AutoSaveInterval = interval; }

	std::string GetPackingToolFilePath() const { return PackingTool; }
	void SetPackingToolFilePath(const std::string& path) { PackingTool = path; }

	DirectX::XMFLOAT4 GetBackgroundColor() const { return BackgroundColor; }
	void SetBackgroundColor(const DirectX::XMFLOAT4& color) { BackgroundColor = color; }

	std::string GetExternelToolPath() const { return ExternelTool; }
	void SetExternelToolPath(const std::string& path) { ExternelTool = path; }

	std::string GetShaderFilePath() const { return ShaderFilePath; }
	void SetShaderFilePath(const std::string& path) { ShaderFilePath = path; }

	std::string GetCSOFilePath() const { return CSOFilePath; }
	void SetCSOFilePath(const std::string& path) { CSOFilePath = path; }

	float GetMouseSensitivity() const { return MouseSensitivity; }
	void SetMouseSensitivity(float sensitivity) { MouseSensitivity = sensitivity; }

	std::string GetDLLFilePath() const { return DLLFilePath; }
	void SetDLLFilePath(const std::string& path) { DLLFilePath = path; }

	std::string GetScriptFilePath() const { return ScriptFilePath; }
	void SetScriptFilePath(const std::string& path) { ScriptFilePath = path; }

	std::string GetScriptLogFilePath() const { return ScriptLogFilePath; }
	void SetScriptLogFilePath(const std::string& path) { ScriptLogFilePath = path; }

	std::string GetAnimator2DProjectFilePath() const { return Animator2DProjectFilePath; }
	void SetAnimator2DProjectFilePath(const std::string& path) { Animator2DProjectFilePath = path; }

private:
	static SettingManager* instance;
private:
	// 設定項目
	std::string AssetsFilePath = "SceneRoot/Assets/";
	std::string ArchiveFilePath = "SceneRoot/Archive/";
	std::string SceneFilePath = "SceneRoot/Scene/";
	std::string PackingTool = "SceneRoot/Tool/Asset packaging tool.exe";
	std::string ExternelTool = "SceneRoot/Tool/IN/";
	std::string ShaderFilePath = "SceneRoot/Shader/hlsl/";
	std::string CSOFilePath = "SceneRoot/Shader/cso/";
	std::string DLLFilePath = "SceneRoot/Script/DLL/";
	std::string ScriptFilePath = "SceneRoot/Script/Source/";
	std::string ScriptLogFilePath = "SceneRoot/Script/Log/";
	std::string Animator2DProjectFilePath = "SceneRoot/Editor/Project/";
	DirectX::XMFLOAT4 BackgroundColor = DirectX::XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	float MouseSensitivity = 0.01f;

	bool bZBuffer = true;
	int AutoSaveInterval = 5; // 自動保存間隔（分）

	SettingManager() {}
	~SettingManager() {}
};
