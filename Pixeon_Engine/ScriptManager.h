#ifndef _SCRIPT_MANAGER_H_
#define _SCRIPT_MANAGER_H_

#include <string>
#include <map>
#include <Windows.h>

// Scriptを管理するマネージャークラス
// DLLの読み込み・解放を行う

class ScriptManager
{
public:
	static ScriptManager& Instance();
	static void Release();

public:

	HMODULE LoadScriptDll(const std::string& dllPath);
	void ReleaseScriptDll(const std::string& dllPath);

private:
	struct DllEntry {
		HMODULE hDll;
		int refCount;
	};
	std::map<std::string, DllEntry> _dllCache;
private:
	static ScriptManager* instance;

	ScriptManager()									= default;
	~ScriptManager()								= default;
	ScriptManager(const ScriptManager&)				= delete;
	ScriptManager& operator=(const ScriptManager&)	= delete;
};

#endif // _SCRIPT_MANAGER_H_

