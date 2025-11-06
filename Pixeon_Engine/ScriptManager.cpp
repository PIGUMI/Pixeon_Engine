#include "ScriptManager.h"
#include <vector>
#include <fstream>
#include <filesystem>

ScriptManager* ScriptManager::instance = nullptr;

ScriptManager& ScriptManager::Instance()
{
	if (instance == nullptr) {
		instance = new ScriptManager();
	}
	return *instance;
}

void ScriptManager::Release()
{
	if (instance != nullptr) {
		delete instance;
		instance = nullptr;
	}
}

HMODULE ScriptManager::LoadScriptDll(const std::string& dllPath)
{
	// Šù‚É“Ç‚Ýž‚Ü‚ê‚Ä‚¢‚é‚©Šm”F
	auto it = _dllCache.find(dllPath);
	if(it != _dllCache.end()) {
		it->second.refCount++;
		return it->second.hDll;
	}
	// DLL‚ð“Ç‚Ýž‚Þ
	HMODULE hDll = LoadLibraryA(dllPath.c_str());
	if (hDll) {
		_dllCache[dllPath] = { hDll, 1 };
	}
	return hDll;
}

void ScriptManager::ReleaseScriptDll(const std::string& dllPath)
{
	auto it = _dllCache.find(dllPath);
	if (it != _dllCache.end()) {
		it->second.refCount--;
		if (it->second.refCount <= 0) {
			FreeLibrary(it->second.hDll);
			_dllCache.erase(it);
		}
	}
}

std::string ScriptManager::GetVSDevEnvPath()
{
	const std::vector<std::string> vsPaths = {
		"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat",
		"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat",
		"C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Auxiliary\\Build\\vcvars64.bat"
	};
	for (const auto& path : vsPaths) {
		if (std::filesystem::exists(path)) return path;
	}
	return "";
}
