#include "ScripComponent.h"
#include "ScriptManager.h"
#include "ComponentManager.h"
#include "GUI.h"
#include "IMGUI/imgui.h"
#include "SettingManager.h"
#include "MainFrame.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

void ScripComponent::Init(AbstractObject* owner) {
	_Parent = owner;
	_ComponentName = "ScripComponent";
	_Type = ComponentManager::COMPONENT_TYPE::SCRIPT;
	RefreshScriptList();
}

void ScripComponent::BeginPlay() {
	if (_scriptInstance)
	{
		Object parentObj = static_cast<Object>(_Parent);
		_scriptInstance->SetParentObject(parentObj);
		Scene parentScene = static_cast<Scene>(_Parent->GetParentScene());
		_scriptInstance->SetParentScene(parentScene);
		try
		{
			_scriptInstance->BeginPlay();
		}
		catch (const std::exception& e)
		{
			_StopOnError = true;
			std::cerr << "[ScripComponent] Exception in BeginPlay of script " << _scriptName << ": " << e.what() << std::endl;
		}
	}
}

void ScripComponent::InGameUpdate() {
	try
	{
		if (_scriptInstance && !_StopOnError)
		{
			_scriptInstance->Update(MainFrame::GetInstance()->GetDeltaTime());
			_InGamePlay = true;
		}
	}
	catch (const std::exception&)
	{
		_StopOnError = true;
		std::cerr << "[ScripComponent] Exception in Update of script " << _scriptName << std::endl;
	}
}

void ScripComponent::EditUpdate()
{
	try
	{
		if (_InGamePlay)
		{
			if (_scriptInstance)
				_scriptInstance->EndPlay();
			_InGamePlay = false;
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "[ScripComponent] Exception in EditUpdate: "
			<< e.what() << std::endl;
		_StopOnError = true;
	}
}

void ScripComponent::UInit() {
	if (_scriptInstance)_scriptInstance->ClearParent();
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
		const fs::path srcDir = SettingManager::GetInstance()->GetScriptFilePath();
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

void ScripComponent::CallFunction(const std::string& functionName)
{
	if (_scriptInstance && !_StopOnError)
	{
		try
		{
			_scriptInstance->CallCustom(functionName);
		}
		catch (const std::exception&)
		{
			_StopOnError = true;
			std::cerr << "[ScripComponent] Exception in CallCustom of script " << _scriptName << ": " << functionName << std::endl;
		}
	}
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
			"    void Update(float DeltaTime) override;\n"
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
		cpp << "void " << className << "::BeginPlay() {\n    IScript::BeginPlay(); // BeginPlay\n}\n\n";
		cpp << "void " << className << ":: Update(float DeltaTime) {\n    IScript::Update(DeltaTime);// Update\n}\n\n";
		cpp << "void " << className << "::EndPlay() {\n    IScript::EndPlay();// EndPlay\n}\n";

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

bool ScripComponent::LoadScriptByName(const std::string& scriptName) {
	std::string dllPath = SettingManager::GetInstance()->GetDLLFilePath() + scriptName + ".dll";

	// DLLが存在しない場合、ScriptManagerを通じてビルド
	if (!fs::exists(dllPath)) {
		auto result = ScriptManager::Instance().BuildScriptDll(scriptName);
		_buildLog = result.log;
		_buildSuccess = result.success;
		_showBuildLog = true;

		if (!result.success) {
			std::cerr << "[ScripComponent] Build failed for " << scriptName << std::endl;
			return false;
		}
	}
	else {
		// DLLが存在する場合でも、ソースの方が新しい場合はリビルド
		std::string srcPath = SettingManager::GetInstance()->GetScriptFilePath() + scriptName + ".cpp";
		if (fs::exists(srcPath)) {
			try {
				auto cppTime = fs::last_write_time(srcPath);
				auto dllTime = fs::last_write_time(dllPath);

				if (cppTime > dllTime) {
					// ソースの方が新しいので、すべてのインスタンスをアンロードしてからリビルド
					_buildLog = "Source file is newer than DLL. Rebuilding...\n\n";
					_showBuildLog = true;

					// ScriptManagerを通じてこのスクリプトの全インスタンスをアンロード
					ScriptManager::Instance().UnloadAllInstancesOfScript(scriptName);

					auto result = ScriptManager::Instance().BuildScriptDll(scriptName);
					_buildLog += result.log;
					_buildSuccess = result.success;

					if (!result.success) {
						std::cerr << "[ScripComponent] Rebuild failed for " << scriptName << std::endl;
						return false;
					}
				}
			}
			catch (...) {
				// タイムスタンプ比較失敗時は既存のDLLを使用
			}
		}
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
		std::cerr << "[ScripComponent] Failed to create script instance:  " << scriptName << std::endl;
		_scriptInstance = nullptr;
		return false;
	}
	_scriptInstance = inst;
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

// --- ImGui Inspector 表示 ---
void ScripComponent::DrawInspector() {
	std::string label = GUI::GetInstance()->ShiftJISToUTF8(_ComponentName);
	std::string Ptr = std::to_string((uintptr_t)this);
	label += "###" + Ptr;

	if (!ImGui::CollapsingHeader(GUI::GetInstance()->ShiftJISToUTF8(label).c_str())) return;

	if (ImGui::BeginTable(("ScriptTable_" + Ptr).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
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
			}
			ImGui::PopItemWidth();
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text("New Script Name");
		ImGui::TableSetColumnIndex(1);
		ImGui::PushItemWidth(-1);
		ImGui::InputText(("##NewScriptName_" + Ptr).c_str(), _newNameBuf, sizeof(_newNameBuf));
		ImGui::PopItemWidth();

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

					_buildLog = "Script files created successfully:\n";
					_buildLog += "- " + name + ".h\n";
					_buildLog += "- " + name + ".cpp\n";
					_buildSuccess = true;
					_showBuildLog = true;
				}
				else {
					_buildLog = "Failed to create script files for:  " + name;
					_buildSuccess = false;
					_showBuildLog = true;
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::Button(("Build##build_" + Ptr).c_str())) {
			std::string name;
			if (_selectedIndex >= 0 && _selectedIndex < (int)_scriptList.size()) {
				name = _scriptList[_selectedIndex];
			}
			if (name.empty()) name = std::string(_newNameBuf);
			if (!name.empty()) {
				// ビルド前にこのスクリプトの全インスタンスをアンロード
				_buildLog = "Unloading all instances of script: " + name + "\n\n";
				_showBuildLog = true;
				ScriptManager::Instance().UnloadAllInstancesOfScript(name);

				// ScriptManagerのビルド機能を使用
				auto result = ScriptManager::Instance().BuildScriptDll(name);
				_buildLog += result.log;
				_buildSuccess = result.success;
				_showBuildLog = true;

				if (result.success) {
					std::cout << "[ScripComponent] Build succeeded for " << name << std::endl;
				}
				else {
					std::cerr << "[ScripComponent] Build failed for " << name << ": " << result.errorMessage << std::endl;
				}

				RefreshScriptList();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button(("Load##load_" + Ptr).c_str())) {
			std::string name;
			if (_selectedIndex >= 0 && _selectedIndex < (int)_scriptList.size()) name = _scriptList[_selectedIndex];
			if (!name.empty()) {
				bool success = LoadScriptByName(name);
				if (success && _scriptInstance) {
					_buildLog = "Script loaded successfully:  " + name;
					_buildSuccess = true;
					_showBuildLog = true;
				}
				else if (!_showBuildLog) {
					_buildLog = "Failed to load script: " + name;
					_buildSuccess = false;
					_showBuildLog = true;
				}
			}
		}
		if (ImGui::Button(("Unload##unload_" + Ptr).c_str())) {
			if (!_scriptName.empty()) {
				std::string unloadedName = _scriptName;
				UnLoadScript();
				_buildLog = "Script unloaded:  " + unloadedName;
				_buildSuccess = true;
				_showBuildLog = true;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button(("Refresh##refresh_" + Ptr).c_str())) {
			RefreshScriptList();
			_buildLog = "Script list refreshed.  Found " + std::to_string(_scriptList.size()) + " script(s).";
			_buildSuccess = true;
			_showBuildLog = true;
		}
		ImGui::SameLine();
		if (ImGui::Button(("Reload##reload_" + Ptr).c_str())) {
			if (_selectedIndex >= 0 && _selectedIndex < (int)_scriptList.size()) {
				std::string name = _scriptList[_selectedIndex];
				if (!name.empty()) {
					_buildLog = "Reloading script: " + name + "\n\n";
					_showBuildLog = true;

					// ScriptManagerを通じてこのスクリプトの全インスタンスをアンロード
					ScriptManager::Instance().UnloadAllInstancesOfScript(name);

					// リビルド
					auto result = ScriptManager::Instance().BuildScriptDll(name);
					_buildLog += result.log;
					_buildSuccess = result.success;

					if (result.success) {
						// ビルド成功したらロード
						bool loadSuccess = LoadScriptByName(name);
						if (loadSuccess) {
							_buildLog += "\nScript reloaded successfully:  " + name;
						}
						else {
							_buildLog += "\nFailed to load script after rebuild:  " + name;
							_buildSuccess = false;
						}
					}
					else {
						_buildLog += "\nReload failed due to build errors. ";
					}
				}
			}
		}
		ImGui::EndGroup();

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
					_buildLog = "Called function: " + fn;
					_buildSuccess = true;
					_showBuildLog = true;
				}
			}
			else {
				_buildLog = "Cannot call function:  No script loaded";
				_buildSuccess = false;
				_showBuildLog = true;
			}
		}

		ImGui::EndTable();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::BeginGroup();

	if (_showBuildLog) {
		if (_buildSuccess) {
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "[SUCCESS]");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "[FAILED]");
		}
		ImGui::SameLine();
	}

	ImGui::Text("Build / Action Log");

	ImGui::SameLine();
	if (ImGui::SmallButton(("Clear##clearlog_" + Ptr).c_str())) {
		_buildLog.clear();
		_showBuildLog = false;
	}

	if (_scriptInstance && !_scriptName.empty()) {
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::Text("Script Properties");
		ImGui::Separator();

		auto properties = _scriptInstance->GetProperties();

		if (!properties.empty()) {
			if (ImGui::BeginTable(("ScriptProps_" + Ptr).c_str(), 2,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {

				ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableHeadersRow();

				for (auto& prop : properties) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%s", prop.name.c_str());

					ImGui::TableSetColumnIndex(1);
					ImGui::PushItemWidth(-1);

					std::string id = "##" + prop.name + "_" + Ptr;
					bool changed = false;

					switch (prop.type) {
					case PropertyType::FLOAT: {
						float* value = static_cast<float*>(prop.dataPtr);
						if (prop.hasRange) {
							changed = ImGui::SliderFloat(id.c_str(), value, prop.minValue, prop.maxValue);
						}
						else {
							changed = ImGui::DragFloat(id.c_str(), value, 0.1f);
						}
						break;
					}
					case PropertyType::INT: {
						int* value = static_cast<int*>(prop.dataPtr);
						if (prop.hasRange) {
							changed = ImGui::SliderInt(id.c_str(), value, (int)prop.minValue, (int)prop.maxValue);
						}
						else {
							changed = ImGui::DragInt(id.c_str(), value);
						}
						break;
					}
					case PropertyType::BOOL: {
						bool* value = static_cast<bool*>(prop.dataPtr);
						changed = ImGui::Checkbox(id.c_str(), value);
						break;
					}
					case PropertyType::STRING: {
						std::string* value = static_cast<std::string*>(prop.dataPtr);
						char buffer[256];
						strncpy_s(buffer, value->c_str(), sizeof(buffer) - 1);
						buffer[sizeof(buffer) - 1] = '\0';
						if (ImGui::InputText(id.c_str(), buffer, sizeof(buffer))) {
							*value = buffer;
							changed = true;
						}
						break;
					}
					}

					// 値が変更されたら、必要に応じて処理
					if (changed) {
						// デバッグ出力やコールバック呼び出しなど
					}

					ImGui::PopItemWidth();
				}

				ImGui::EndTable();
			}
		}
		else {
			ImGui::TextDisabled("No properties defined in this script.");
		}
	}

	ImGui::BeginChild(("BuildLogDisplay_" + Ptr).c_str(),
		ImVec2(0, 250),
		true,
		ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);

	if (_showBuildLog && !_buildLog.empty()) {
		std::string convertedLog = GUI::GetInstance()->ShiftJISToUTF8(_buildLog);

		std::istringstream logStream(convertedLog);
		std::string line;
		while (std::getline(logStream, line)) {
			if (line.find("error") != std::string::npos ||
				line.find("Error") != std::string::npos ||
				line.find("ERROR") != std::string::npos ||
				line.find("failed") != std::string::npos ||
				line.find("Failed") != std::string::npos ||
				line.find("cannot open") != std::string::npos) {
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", line.c_str());
			}
			else if (line.find("warning") != std::string::npos ||
				line.find("Warning") != std::string::npos ||
				line.find("WARNING") != std::string::npos) {
				ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "%s", line.c_str());
			}
			else if (line.find("succeeded") != std::string::npos ||
				line.find("success") != std::string::npos ||
				line.find("Success") != std::string::npos ||
				line.find("completed") != std::string::npos) {
				ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", line.c_str());
			}
			else {
				ImGui::TextUnformatted(line.c_str());
			}
		}

		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
			ImGui::SetScrollHereY(1.0f);
		}
	}
	else {
		ImGui::TextDisabled("No log messages.  Perform an action to see output.");
	}

	ImGui::EndChild();
	ImGui::EndGroup();

	ImGui::Spacing();


	ImGui::TextDisabled("Script sources:  Script/Src/*. cpp -> Build -> Script/Bin/*.dll");
	ImGui::TextDisabled("Create Script:  generates header+cpp skeleton.  Implement logic in generated cpp.");
}