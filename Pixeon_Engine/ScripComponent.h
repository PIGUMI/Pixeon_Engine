#pragma once
/*　実装概要
* 動的スクリプト用コンポーネント
* 前回のスクリプト用コンポーネントはファイルを複製し読み込んでいたが、i/o負荷が高いため
* 読み込んだスクリプトをメモリ上に保存し、動的にコンパイル・実行する方式に変更
*/

#include "Component.h"
#include <Windows.h>
#include <string>
#include "IScript.h"

class ScripComponent : public Component
{
public :
	void Init(Object* owner) override;
	void BeginPlay() override;
	void InGameUpdate() override;
	void UInit() override;
	void DrawInspector() override;

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

	bool LoadScript(const std::string& dllPath);
	void UnLoadScript();

	IScript* GetScriptInstance() const { return _scriptInstance; }
	std::string GetScriptName() const { return _dllName; }

private:
	IScript* _scriptInstance = nullptr;
	std::string _dllName;
	HMODULE _dllHandle = nullptr;
private:

	ScripComponent();
	~ScripComponent();
};

