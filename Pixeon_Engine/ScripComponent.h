#ifndef SCRIPT_COMPONENT_H
#define SCRIPT_COMPONENT_H

#include "Component.h"
#include <Windows.h>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include "IScript.h"

class ScripComponent : public AbstractComponent
{
public:
	void Init(AbstractObject* owner) override;
	void BeginPlay() override;
	void InGameUpdate() override;
	void EditUpdate() override;
	void UInit() override;
	void DrawInspector() override;

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

	// Script load/unload
	bool LoadScript(const std::string& scriptName);
	void UnLoadScript();

	// Utility (for Inspector)
	bool LoadScriptByName(const std::string& scriptName);
	bool CreateScriptFiles(const std::string& scriptName);
	void RefreshScriptList();

	void CallFunction(const std::string& functionName);

	IScript* GetScriptInstance() const { return _scriptInstance; }
	std::string GetScriptName() const { return _scriptName; }

	// Notification from ScriptManager when instance becomes invalid
	void InvalidateScriptInstance();

private:
	IScript* _scriptInstance = nullptr;
	std::string _scriptName;

	// Property value storage (property name -> serialized value)
	std::map<std::string, std::string> _savedProperties;

	// Inspector state
	std::vector<std::string> _scriptList;
	int _selectedIndex = -1;
	char _newNameBuf[128] = {};
	char _callBuf[128] = {};
	std::string _buildLog;
	bool _showBuildLog = false;
	bool _buildSuccess = false;
	bool _StopOnError = false;
	bool _InGamePlay = false;

	// Property save/restore helper functions
	void SaveProperties();
	void RestoreProperties();
};

#endif // !SCRIPT_COMPONENT_H