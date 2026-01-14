#ifndef _SCRIPT_MANAGER_H_
#define _SCRIPT_MANAGER_H_

#include <string>
#include <map>
#include <filesystem>
#include <Windows.h>
#include <mutex>
#include <vector>

class ScripComponent;
class IScript;

class ScriptManager
{
public:
	static ScriptManager& Instance();
	static void Release();

	IScript* CreateScriptInstance(const std::string& scriptName, ScripComponent* owner);

	void DestroyScriptInstance(const std::string& scriptName, ScripComponent* owner);

	void Update();

	std::string GetVSDevEnvPath() const;

	void RegisterAllScripts();

	struct BuildResult {
		bool success;
		std::string log;
		std::string errorMessage;
	};

	BuildResult BuildScriptDll(const std::string& scriptName);

private:
	ScriptManager() = default;
	~ScriptManager();

	ScriptManager(const ScriptManager&) = delete;
	ScriptManager& operator=(const ScriptManager&) = delete;

	struct DllEntry {
		HMODULE hDll = nullptr;
		int refCount = 0; // 作成されたインスタンス数
		std::map<ScripComponent*, IScript*> instances; // owner -> instance
		std::filesystem::file_time_type lastCppWriteTime;
	};

	bool BuildScriptDll(const std::string& scriptName, std::string& outError);
	bool LoadDllForScript(const std::string& scriptName, DllEntry& entry);
	void UnloadDllEntry(DllEntry& entry);

	std::map<std::string, DllEntry> _dllMap;
	std::mutex _mutex;
};

#endif // _SCRIPT_MANAGER_H_
