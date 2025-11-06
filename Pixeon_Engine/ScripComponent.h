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
public:
    void Init(Object* owner) override;
    void BeginPlay() override;
    void InGameUpdate() override;
    void UInit() override;
    void DrawInspector() override;
    void SaveToFile(std::ostream& out) override;
    void LoadFromFile(std::istream& in) override;

    bool CreateScriptFiles(const std::string& scriptName);
    bool BuildScriptDll(const std::string& scriptName);

    void SetScriptName(const std::string& name) { _scriptName = name; }
    std::string GetScriptName() const { return _scriptName; }

    bool LoadScript(const std::string& scriptName);
    void UnLoadScript();

    IScript* GetScriptInstance() const { return _scriptInstance; }

private:
    IScript* _scriptInstance = nullptr;
    std::string _scriptName;
    HMODULE _dllHandle = nullptr;
};

