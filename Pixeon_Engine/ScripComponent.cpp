#include "ScripComponent.h"
#include "ScriptManager.h"
#include "ComponentManager.h"
#include "GUI.h"
#include "IMGUI/imgui.h"
#include "SettingManager.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib> // system
#include <vector>

namespace fs = std::filesystem;

// Create/Destroy function types (DLL 側でエクスポートされていること)
typedef IScript* (*CreateScriptInstanceFunc)();
typedef void (*DestroyScriptInstanceFunc)(IScript*);

void ScripComponent::Init(AbstractObject* owner) {
	_Parent = owner;
	_ComponentName = "ScripComponent";
	_Type = ComponentManager::COMPONENT_TYPE::SCRIPT;
	RefreshScriptList();
}

void ScripComponent::BeginPlay() {
	if (_scriptInstance) _scriptInstance->BeginPlay();
}

void ScripComponent::InGameUpdate() {
	if (_scriptInstance) _scriptInstance->Update();
}

void ScripComponent::UInit() {
	UnLoadScript();
}

void ScripComponent::SaveToFile(std::ostream& out) {
	out << "ScriptName " << _scriptName << std::endl;
}

void ScripComponent::LoadFromFile(std::istream& in) {
	std::string key;
	while (in >> key) {
		if (key == "ScriptName") {
			in >> _scriptName;
		}
	}
	if (!_scriptName.empty()) {
		LoadScriptByName(_scriptName);
	}
}

void ScripComponent::RefreshScriptList() {
	_scriptList.clear();
	try {
		const fs::path srcDir = SettingManager::GetInstance()->GetScriptFilePath();;
		if (!fs::exists(srcDir)) {
			fs::create_directories(srcDir);
		}
		for (auto& p : fs::directory_iterator(srcDir)) {
			if (!p.is_regular_file()) continue;
			auto ext = p.path().extension().string();
			if (ext == ".cpp" || ext == ".CPP" || ext == ".cxx") {
				std::string name = p.path().stem().string();
				_scriptList.push_back(name);
			}
		}
	}
	catch (const std::exception& e) {
		std::cerr << "[ScripComponent] RefreshScriptList exception: " << e.what() << std::endl;
	}
	if (_scriptList.empty()) _selectedIndex = -1;
	else if (_selectedIndex < 0) _selectedIndex = 0;
}

bool ScripComponent::CreateScriptFiles(const std::string& scriptName) {
	if (scriptName.empty()) return false;
	try {
		const fs::path srcDir = SettingManager::GetInstance()->GetScriptFilePath();
		fs::create_directories(srcDir);
		std::string className = "Script_" + scriptName;
		std::string headerPath = (srcDir / (scriptName + ".h")).string();
		std::string cppPath = (srcDir / (scriptName + ".cpp")).string();

		std::ostringstream header;
		header <<
			"#pragma once\n"
			"#include \"Include/IScript.h\"\n\n"
			"class " << className << " : public IScript {\n"
			"public:\n"
			"    void BeginPlay() override;\n"
			"    void Update() override;\n"
			"    void EndPlay() override;\n"
			"};\n\n"
			"extern \"C\" __declspec(dllexport) IScript* CreateScriptInstance() {\n"
			"    return new " << className << "();\n"
			"}\n\n"
			"extern \"C\" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {\n"
			"    delete script;\n"
			"}\n";

		std::ostringstream cpp;
		cpp << "#include \"" << scriptName << ".h\"\n\n";
		cpp << "void " << className << "::BeginPlay() {\n    // BeginPlay\n}\n\n";
		cpp << "void " << className << "::Update() {\n    // Update\n}\n\n";
		cpp << "void " << className << "::EndPlay() {\n    // EndPlay\n}\n";

		if (!fs::exists(headerPath)) {
			std::ofstream h(headerPath);
			if (!h) return false;
			h << header.str();
		}
		if (!fs::exists(cppPath)) {
			std::ofstream c(cppPath);
			if (!c) return false;
			c << cpp.str();
		}

		RefreshScriptList();
		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "[ScripComponent] CreateScriptFiles exception: " << e.what() << std::endl;
		return false;
	}
}

std::string ScripComponent::GetVSDevEnvPath() const {
	const std::vector<std::string> vsPaths = {
		"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat",
		"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat",
		"C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Auxiliary\\Build\\vcvars64.bat"
	};
	for (auto& p : vsPaths) {
		if (fs::exists(p)) return p;
	}
	return "";
}

bool ScripComponent::BuildScriptDll(const std::string& scriptName) {
	// スクリプト名が空ならビルドしない
	if (scriptName.empty()) return false;
	std::string vcvars = GetVSDevEnvPath();
	if (vcvars.empty()) {
		_buildLog = "vcvars64.bat not found. Configure Visual Studio path.";
		_showBuildLog = true;
		return false;
	}

	const fs::path srcDir = SettingManager::GetInstance()->GetScriptFilePath();
	const fs::path includeDir = SettingManager::GetInstance()->GetScriptFilePath() + "Include";
	const fs::path binDir = SettingManager::GetInstance()->GetDLLFilePath();;
	fs::create_directories(binDir);

	std::string srcPath = (srcDir / (scriptName + ".cpp")).string();
	std::string dllPath = (binDir / (scriptName + ".dll")).string();
	std::string libPath = (binDir / (scriptName + ".lib")).string();
	std::string pdbPath = (binDir / (scriptName + ".pdb")).string();
	std::string engineLib = (includeDir / "Pixeon_Engine.lib").string();

	std::string logFile = SettingManager::GetInstance()->GetScriptLogFilePath() + "build_" + scriptName + ".log";
	std::ostringstream cmd;
	cmd << "cmd /C \"call \"" << vcvars << "\" && "
		<< "cl /LD /EHsc /MD "
		<< "\"" << srcPath << "\" "
		<< "\"" + SettingManager::GetInstance()->GetScriptFilePath() + "Include/IScript.cpp\" "
		<< "/Fe:\"" << dllPath << "\" "
		<< "/I\"" << includeDir.string() << "\" "
		<< "/link /LIBPATH:\"" << includeDir.string() << "\" \"" << engineLib << "\" user32.lib "
		<< "/IMPLIB:\"" << libPath << "\" "
		<< "/PDB:\"" << pdbPath << "\""
		<< " > \"" << logFile << "\" 2>&1\"";

	int res = std::system(cmd.str().c_str());

	_buildLog.clear();
	if (fs::exists(logFile)) {
		std::ifstream ifs(logFile);
		std::ostringstream ss;
		ss << ifs.rdbuf();
		_buildLog = ss.str();
		_showBuildLog = true;
	}
	else {
		_buildLog = "No build log produced.";
		_showBuildLog = true;
	}

	if (fs::exists(dllPath) && res == 0) {
		_buildLog = "Build succeeded.\n\n" + _buildLog;
		return true;
	}
	else {
		_buildLog = "Build failed.\n\n" + _buildLog;
		return false;
	}
}

bool ScripComponent::LoadScriptByName(const std::string& scriptName) {
	std::string dllPath = SettingManager::GetInstance()->GetDLLFilePath() + scriptName + ".dll";
	if (!fs::exists(dllPath)) {
		BuildScriptDll(scriptName);
	}
	return LoadScript(scriptName);
}

bool ScripComponent::LoadScript(const std::string& scriptName) {
	if (_scriptInstance) {
		UnLoadScript();
	}
	_scriptName = scriptName;
	IScript* inst = ScriptManager::Instance().CreateScriptInstance(scriptName, this);
	if (!inst) {
		std::cerr << "[ScripComponent] Failed to create script instance: " << scriptName << std::endl;
		_scriptInstance = nullptr;
		return false;
	}
	_scriptInstance = inst;
	//_scriptInstance->BeginPlay();
	return true;
}

void ScripComponent::UnLoadScript() {
	if (!_scriptName.empty()) {
		if (_scriptInstance) {
			ScriptManager::Instance().DestroyScriptInstance(_scriptName, this);
			_scriptInstance = nullptr;
		}
		else {
			ScriptManager::Instance().DestroyScriptInstance(_scriptName, this);
		}
		_scriptName.clear();
	}
}

// --- ImGui Inspector 実装 ---
void ScripComponent::DrawInspector() {
	std::string label = GUI::GetInstance()->ShiftJISToUTF8(_ComponentName);
	std::string Ptr = std::to_string((uintptr_t)this);
	label += "###" + Ptr;

	if (!ImGui::CollapsingHeader(GUI::GetInstance()->ShiftJISToUTF8(label).c_str())) return;

	// テーブル
	if (ImGui::BeginTable(("ScriptTable_" + Ptr).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
		// Available Scripts
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text("Available Scripts");
		ImGui::TableSetColumnIndex(1);
		std::vector<const char*> items;
		for (auto& s : _scriptList) items.push_back(s.c_str());
		if (items.empty()) {
			ImGui::TextDisabled("No scripts found in Script/Src");
		}
		else {
			if (_selectedIndex < 0) _selectedIndex = 0;
			ImGui::PushItemWidth(-1);
			if (ImGui::Combo(("##ScriptList_" + Ptr).c_str(), &_selectedIndex, items.data(), (int)items.size())) {
				// 選択変更時の処理はここに（特になし）
			}
			ImGui::PopItemWidth();
		}

		// New Script Name
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text("New Script Name");
		ImGui::TableSetColumnIndex(1);
		ImGui::PushItemWidth(-1);
		ImGui::InputText(("##NewScriptName_" + Ptr).c_str(), _newNameBuf, sizeof(_newNameBuf));
		ImGui::PopItemWidth();

		// Actions
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text("Actions");
		ImGui::TableSetColumnIndex(1);
		ImGui::BeginGroup();
		if (ImGui::Button(("Create Script##create_" + Ptr).c_str())) {
			std::string name = _newNameBuf;
			if (!name.empty()) {
				if (CreateScriptFiles(name)) {
					RefreshScriptList();
					for (size_t i = 0; i < _scriptList.size(); ++i) {
						if (_scriptList[i] == name) { _selectedIndex = (int)i; break; }
					}
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::Button(("Build##build_" + Ptr).c_str())) {
			std::string name;
			if (_selectedIndex >= 0 && _selectedIndex < (int)_scriptList.size()) name = _scriptList[_selectedIndex];
			if (name.empty()) name = std::string(_newNameBuf);
			if (!name.empty()) {
				BuildScriptDll(name);
				RefreshScriptList();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button(("Load##load_" + Ptr).c_str())) {
			std::string name;
			if (_selectedIndex >= 0 && _selectedIndex < (int)_scriptList.size()) name = _scriptList[_selectedIndex];
			if (!name.empty()) {
				LoadScriptByName(name);
			}
		}
		if (ImGui::Button(("Unload##unload_" + Ptr).c_str())) {
			UnLoadScript();
		}
		ImGui::SameLine();
		if (ImGui::Button(("Refresh##refresh_" + Ptr).c_str())) {
			RefreshScriptList();
		}
		ImGui::SameLine();
		if (ImGui::Button(("Reload##reload_" + Ptr).c_str())) {
			if (_selectedIndex >= 0 && _selectedIndex < (int)_scriptList.size()) {
				std::string name = _scriptList[_selectedIndex];
				if (!name.empty()) {
					UnLoadScript();
					LoadScriptByName(name);
				}
			}
		}
		ImGui::EndGroup();

		// Loaded 情報
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text("Loaded");
		ImGui::TableSetColumnIndex(1);
		if (_scriptInstance && !_scriptName.empty()) {
			std::string displayName = _scriptName;
			try {
				fs::path p(_scriptName);
				if (p.has_filename()) displayName = p.stem().string();
			}
			catch (...) {}
			ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", displayName.c_str());
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "None");
		}

		// CallCustom
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text("Call Function");
		ImGui::TableSetColumnIndex(1);
		ImGui::PushItemWidth(-1);
		ImGui::InputText(("##CallName_" + Ptr).c_str(), _callBuf, sizeof(_callBuf));
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button(("Call##call_" + Ptr).c_str())) {
			if (_scriptInstance) {
				std::string fn = _callBuf;
				if (!fn.empty()) {
					_scriptInstance->CallCustom(fn);
				}
			}
		}

		ImGui::EndTable();
	}

	// Build log
	if (_showBuildLog) {
		if (ImGui::CollapsingHeader(("Build Log##log_" + Ptr).c_str())) {
			ImGui::BeginChild(("BuildLogChild_" + Ptr).c_str(), ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);
			ImGui::TextUnformatted(GUI::GetInstance()->ShiftJISToUTF8(_buildLog).c_str());
			ImGui::EndChild();
		}
	}

	ImGui::TextDisabled("Script sources: Script/Src/*.cpp  -> Build -> Script/Bin/*.dll");
	ImGui::TextDisabled("Create Script: generates header+cpp skeleton. Implement logic in generated cpp.");
}