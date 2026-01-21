#ifndef SCRIPT_COMPONENT_H
#define SCRIPT_COMPONENT_H

/*  TODO
*   ScriptComponent及び、ScriptManagerのリファクタリング
*   SettingMangerからファイルパスを取得するようにする
*/

/*　実装概要
* 動的スクリプト用コンポーネント
* 前回のスクリプト用コンポーネントはファイルを複製し読み込んでいたが、i/o負荷が高いため
* 読み込んだスクリプトをメモリ上に保存し、動的にコンパイル・実行する方式に変更
*/

#include "Component.h"
#include <Windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include "IScript.h"

class ScripComponent : public AbstractComponent
{
public:
	void Init(AbstractObject* owner) override;
	void BeginPlay() override;
	void InGameUpdate() override;
	void UInit() override;
	void DrawInspector() override;

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

	// スクリプト読み込み / 解放
	bool LoadScript(const std::string& scriptName);
	void UnLoadScript();

	// ユーティリティ（Inspector 用）
	bool LoadScriptByName(const std::string& scriptName);
	bool CreateScriptFiles(const std::string& scriptName);
	void RefreshScriptList();

	IScript* GetScriptInstance() const { return _scriptInstance; }
	std::string GetScriptName() const { return _scriptName; }

private:
	IScript* _scriptInstance = nullptr;
	std::string _scriptName;

	// Inspector state
	std::vector<std::string> _scriptList;
	int _selectedIndex = -1;
	char _newNameBuf[128] = {};
	char _callBuf[128] = {};
	std::string _buildLog;
	bool _showBuildLog = false;
	bool _buildSuccess = false;
	bool _StopOnError = false;
};

#endif // !SCRIPT_COMPONENT_H