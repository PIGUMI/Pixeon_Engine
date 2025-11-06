#include "ScripComponent.h"
#include "ScriptManager.h"
#include <filesystem>
#include <iostream>

// DLLインスタンス生成関数型
typedef IScript* (*CreateScriptInstanceFunc)();
typedef void (*DestroyScriptInstanceFunc)(IScript*);

namespace fs = std::filesystem;

void ScripComponent::Init(Object* owner) {
    _Parent = owner;
    _ComponentName = "ScripComponent";
    _Type = ComponentManager::COMPONENT_TYPE::SCRIPT;
}

void ScripComponent::BeginPlay() {
    if (_scriptInstance) {
        _scriptInstance->BeginPlay();
    }
}

void ScripComponent::InGameUpdate() {
    if (_scriptInstance) {
        _scriptInstance->Update();
    }
}

void ScripComponent::UInit() {
    UnLoadScript();
}

void ScripComponent::DrawInspector() {}

void ScripComponent::SaveToFile(std::ostream& out) {
    out << "ScriptName " << _scriptName << std::endl;
}

void ScripComponent::LoadFromFile(std::istream& in) {
    std::string key, value;
    while (in >> key >> value) {
        if (key == "ScriptName") {
            _scriptName = value;
        }
    }
}

bool ScripComponent::CreateScriptFiles(const std::string& scriptName) {
    std::string className = "Script_" + scriptName;
    std::string headerPath = "Script/Src/" + scriptName + ".h";
    std::string cppPath = "Script/Src/" + scriptName + ".cpp";

    // ヘッダ&cpp自動生成
    std::string HeaderContent =
        "#pragma once\n"
        "#include \"../Include/IScript.h\"\n"
        "class " + className + " : public IScript {\n"
        "public:\n"
        "    void BeginPlay() override {}\n"
        "    void Update() override {}\n"
        "    void EndPlay() override {}\n"
        "};\n"
        "extern \"C\" __declspec(dllexport) IScript* CreateScriptInstance() { return new " + className + "(); }\n"
        "extern \"C\" __declspec(dllexport) void DestroyScriptInstance(IScript* script) { delete script; }\n";

    std::string CppContent =
        "#include \"" + scriptName + ".h\"\n"
        "// 追加の実装内容はここに書く\n";

    std::ofstream headerFile(headerPath);
    std::ofstream cppFile(cppPath);
    if (!headerFile || !cppFile) return false;
    headerFile << HeaderContent;
    cppFile << CppContent;
    return true;
}

bool ScripComponent::BuildScriptDll(const std::string& scriptName) {
    // VisualStudioのPath取得
    std::string vcvarsPath = ScriptManager::Instance().GetVSDevEnvPath();
    if (vcvarsPath.empty()) return false;

    std::string srcDir = "Script/Src/";
    std::string binDir = "Script/Bin/";
    std::string includeDir = "Script/Include/";

    std::string cppPath = srcDir + scriptName + ".cpp";
    std::string dllPath = binDir + scriptName + ".dll";
    std::string libPath = binDir + scriptName + ".lib";
    std::string expPath = binDir + scriptName + ".exp";
    std::string engineLibPath = includeDir + "Engine.lib"; // エンジン依存lib

    std::string command =
        "cmd /C \"call \"" + vcvarsPath + "\" && "
        "cl /LD /EHsc /MD "
        "\"" + cppPath + "\" "
        "/Fe:\"" + dllPath + "\" "
        "/I\"" + includeDir + "\" "
        "/link /LIBPATH:\"" + includeDir + "\" \"" + engineLibPath + "\" user32.lib "
        "/IMPLIB:\"" + libPath + "\" "
        "\"";

    system(command.c_str()); // コマンド実行
    return fs::exists(dllPath);
}

bool ScripComponent::LoadScript(const std::string& scriptName) {
    UnLoadScript();

    // ScriptファイルやDLLが更新された場合は再ビルド
    std::string cppPath = "Script/Src/" + scriptName + ".cpp";
    std::string dllPath = "Script/Bin/" + scriptName + ".dll";
    if (!fs::exists(dllPath) || fs::last_write_time(cppPath) > fs::last_write_time(dllPath)) {
        if (!BuildScriptDll(scriptName)) return false;
    }

    // DLLロード
    _dllHandle = ScriptManager::Instance().LoadScriptDll(dllPath);
    if (!_dllHandle) return false;

    CreateScriptInstanceFunc createFunc = (CreateScriptInstanceFunc)GetProcAddress(_dllHandle, "CreateScriptInstance");
    if (!createFunc) return false;

    _scriptInstance = createFunc();
    _scriptName = scriptName;
    return _scriptInstance != nullptr;
}

void ScripComponent::UnLoadScript() {
    if (_scriptInstance) {
        DestroyScriptInstanceFunc destroyFunc =
            (DestroyScriptInstanceFunc)GetProcAddress(_dllHandle, "DestroyScriptInstance");
        if (destroyFunc) destroyFunc(_scriptInstance);
        _scriptInstance = nullptr;
    }
    if (!_scriptName.empty()) {
        std::string dllPath = "Script/Bin/" + _scriptName + ".dll";
        ScriptManager::Instance().ReleaseScriptDll(dllPath);
        _dllHandle = nullptr;
        _scriptName.clear();
    }
}