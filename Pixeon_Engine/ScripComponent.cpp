#include "ScripComponent.h"
#include "ScriptManager.h"

// スクリプトインスタンス生成関数ポインタ型
typedef IScript* (*CreateScriptInstanceFunc)();
typedef void (*DestroyScriptInstanceFunc)(IScript*);

ScripComponent::ScripComponent() {}

ScripComponent::~ScripComponent() { UnLoadScript(); }

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

// インスペクター描画
void ScripComponent::DrawInspector()
{
}

// ファイル保存
void ScripComponent::SaveToFile(std::ostream& out)
{
}

// ファイル読み込み
void ScripComponent::LoadFromFile(std::istream& in)
{
}

bool ScripComponent::LoadScript(const std::string& dllPath){
	_dllName = dllPath;
	// DLL読み込み
	_dllHandle = ScriptManager::Instance().LoadScriptDll(dllPath);
	if (!_dllHandle)  return false;
	CreateScriptInstanceFunc createFunc = (CreateScriptInstanceFunc)GetProcAddress(_dllHandle, "CreateScriptInstance");
	if (!createFunc) return false;
	_scriptInstance = createFunc();
	return _scriptInstance != nullptr;
}

void ScripComponent::UnLoadScript(){
	if (!_dllName.empty()) {
		ScriptManager::Instance().ReleaseScriptDll(_dllName);
		_dllName.clear();
		_dllHandle = nullptr;
	}
}
