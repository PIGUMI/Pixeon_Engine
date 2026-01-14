#include "ScriptManager.h"
#include "ScripComponent.h"
#include "SettingManager.h"
#include "IScript.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

typedef IScript* (*CreateScriptInstanceFunc)();
typedef void (*DestroyScriptInstanceFunc)(IScript*);

ScriptManager* g_instance = nullptr;

ScriptManager& ScriptManager::Instance()
{
	if (!g_instance) g_instance = new ScriptManager();
	return *g_instance;
}

void ScriptManager::Release()
{
	if (g_instance) {
		delete g_instance;
		g_instance = nullptr;
	}
}

ScriptManager::~ScriptManager()
{
	std::lock_guard<std::mutex> lk(_mutex);
	// 全てのインスタンスを破棄してDLLをアンロード
	for (auto& kv : _dllMap) {
		UnloadDllEntry(kv.second);
	}
	_dllMap.clear();
}

bool ScriptManager::IsScriptLoaded(const std::string& scriptName)
{
	std::lock_guard<std::mutex> lk(_mutex);
	auto it = _dllMap.find(scriptName);
	if (it == _dllMap.end()) return false;
	return (it->second.hDll != nullptr && it->second.refCount > 0);
}

void ScriptManager::UnloadAllInstancesOfScript(const std::string& scriptName)
{
	std::lock_guard<std::mutex> lk(_mutex);
	auto it = _dllMap.find(scriptName);
	if (it == _dllMap.end()) return;

	auto& entry = it->second;

	// すべてのインスタンスを破棄
	if (entry.hDll) {
		auto destroyFunc = (DestroyScriptInstanceFunc)GetProcAddress(entry.hDll, "DestroyScriptInstance");

		// インスタンスのコピーを作成（イテレート中の変更を避けるため）
		std::vector<std::pair<ScripComponent*, IScript*>> instancesCopy;
		for (auto& kv : entry.instances) {
			instancesCopy.push_back(kv);
		}

		// すべてのインスタンスを破棄
		for (auto& kv : instancesCopy) {
			IScript* inst = kv.second;
			if (inst) {
				inst->EndPlay();
				if (destroyFunc) destroyFunc(inst);
				else delete inst;
			}
		}
		entry.instances.clear();

		// DLLをアンロード
		FreeLibrary(entry.hDll);
		entry.hDll = nullptr;
		entry.refCount = 0;
	}
}

IScript* ScriptManager::CreateScriptInstance(const std::string& scriptName, ScripComponent* owner)
{
	std::lock_guard<std::mutex> lk(_mutex);

	// エントリ作成または取得
	auto& entry = _dllMap[scriptName];

	// ソース/バイナリパス
	std::string srcPath = SettingManager::GetInstance()->GetScriptFilePath() + scriptName + ".cpp";
	std::string dllPath = SettingManager::GetInstance()->GetDLLFilePath() + scriptName + ".dll";

	// 初回か、ソースが新しいかチェックしてビルドが必要ならビルド
	bool needBuild = false;
	if (!fs::exists(dllPath)) {
		needBuild = true;
	}
	else if (fs::exists(srcPath)) {
		auto cppTime = fs::last_write_time(srcPath);
		if (entry.lastCppWriteTime != cppTime) {
			// タイムスタンプが変わっている(初回なら default なので build)
			if (cppTime > fs::last_write_time(dllPath)) {
				needBuild = true;
			}
			entry.lastCppWriteTime = cppTime;
		}
	}

	if (needBuild) {
		// ビルド前にDLLをアンロード（重要！）
		if (entry.hDll != nullptr) {
			std::cout << "[ScriptManager] Unloading DLL before rebuild:  " << scriptName << std::endl;
			UnloadDllEntry(entry);
		}

		auto result = BuildScriptDll(scriptName);
		if (!result.success) {
			std::cerr << "[ScriptManager] Build failed for " << scriptName << ": " << result.errorMessage << std::endl;
			// ビルド失敗時は古いDLLも使用しない
			return nullptr;
		}
	}

	// DLLがまだロードでないならロード
	if (entry.hDll == nullptr) {
		if (!LoadDllForScript(scriptName, entry)) {
			std::cerr << "[ScriptManager] Load DLL failed for " << scriptName << std::endl;
			return nullptr;
		}
	}

	// CreateScriptInstance 関数を呼び出してインスタンス作成
	auto createFunc = (CreateScriptInstanceFunc)GetProcAddress(entry.hDll, "CreateScriptInstance");
	if (!createFunc) {
		std::cerr << "[ScriptManager] CreateScriptInstance not found in " << scriptName << std::endl;
		return nullptr;
	}

	IScript* instance = createFunc();
	if (!instance) {
		std::cerr << "[ScriptManager] CreateScriptInstance returned nullptr for " << scriptName << std::endl;
		return nullptr;
	}

	// 登録
	entry.instances[owner] = instance;
	entry.refCount++;

	return instance;
}

void ScriptManager::DestroyScriptInstance(const std::string& scriptName, ScripComponent* owner)
{
	std::lock_guard<std::mutex> lk(_mutex);
	auto it = _dllMap.find(scriptName);
	if (it == _dllMap.end()) return;
	auto& entry = it->second;
	auto itInst = entry.instances.find(owner);
	if (itInst == entry.instances.end()) return;

	IScript* instance = itInst->second;
	if (instance) {
		// EndPlay 呼び出し(安全のため)→ Destroy
		instance->EndPlay();

		// DestroyScriptInstance を DLL から呼ぶ(あれば)
		auto destroyFunc = (DestroyScriptInstanceFunc)GetProcAddress(entry.hDll, "DestroyScriptInstance");
		if (destroyFunc) {
			destroyFunc(instance);
		}
		else {
			// エクスポートされていないなら delete
			delete instance;
		}
	}

	entry.instances.erase(itInst);
	entry.refCount--;
	if (entry.refCount <= 0) {
		// アンロード
		UnloadDllEntry(entry);
		_dllMap.erase(it);
	}
}

void ScriptManager::Update()
{
	std::lock_guard<std::mutex> lk(_mutex);

	// 各登録スクリプトについてソースの更新をチェック
	for (auto it = _dllMap.begin(); it != _dllMap.end(); /*inc inside*/) {
		const std::string scriptName = it->first;
		DllEntry& entry = it->second;

		std::string srcPath = SettingManager::GetInstance()->GetScriptFilePath() + scriptName + ".cpp";
		std::string dllPath = SettingManager::GetInstance()->GetDLLFilePath() + scriptName + ".dll";

		bool needBuild = false;
		if (fs::exists(srcPath)) {
			auto cppTime = fs::last_write_time(srcPath);
			if (entry.lastCppWriteTime != cppTime) {
				// ソースタイムスタンプ変化
				entry.lastCppWriteTime = cppTime;
				// 比較:  cpp > dll
				if (!fs::exists(dllPath) || cppTime > fs::last_write_time(dllPath)) {
					needBuild = true;
				}
			}
		}

		if (needBuild) {
			std::cout << "[ScriptManager] Detected change in " << scriptName << ", building..." << std::endl;

			// 保存しておくデータ
			auto oldH = entry.hDll;
			auto owners = std::vector<ScripComponent*>{};
			for (auto& kv : entry.instances) owners.push_back(kv.first);

			// Destroy old instances and unload DLL before build
			for (auto owner : owners) {
				auto itInst = entry.instances.find(owner);
				if (itInst == entry.instances.end()) continue;
				IScript* instance = itInst->second;
				if (instance) {
					instance->EndPlay();
					auto destroyFunc = (DestroyScriptInstanceFunc)GetProcAddress(oldH, "DestroyScriptInstance");
					if (destroyFunc) destroyFunc(instance);
					else delete instance;
				}
				entry.instances.erase(itInst);
			}

			// Free old dll BEFORE building
			if (oldH) {
				FreeLibrary(oldH);
				entry.hDll = nullptr;
			}

			auto result = BuildScriptDll(scriptName);
			if (result.success) {
				std::cout << "[ScriptManager] Build succeeded for " << scriptName << ", reloading..." << std::endl;

				// Load new dll
				if (!LoadDllForScript(scriptName, entry)) {
					std::cerr << "[ScriptManager] Failed to load rebuilt DLL for " << scriptName << std::endl;
					++it;
					continue;
				}

				// Recreate instances and call BeginPlay
				auto createFunc = (CreateScriptInstanceFunc)GetProcAddress(entry.hDll, "CreateScriptInstance");
				if (!createFunc) {
					std::cerr << "[ScriptManager] CreateScriptInstance not found after reload for " << scriptName << std::endl;
					++it;
					continue;
				}

				for (auto owner : owners) {
					IScript* newInst = createFunc();
					if (newInst) {
						entry.instances[owner] = newInst;
						// BeginPlay はここで呼ぶ(必要なら)
						newInst->BeginPlay();
					}
				}

				entry.refCount = (int)entry.instances.size();
				std::cout << "[ScriptManager] Reload completed for " << scriptName << std::endl;
			}
			else {
				std::cerr << "[ScriptManager] Build failed for " << scriptName << ": " << result.errorMessage << std::endl;
			}
		}

		++it;
	}
}

bool ScriptManager::LoadDllForScript(const std::string& scriptName, DllEntry& entry)
{
	std::string dllPath = SettingManager::GetInstance()->GetDLLFilePath() + scriptName + ".dll";
	if (!fs::exists(dllPath)) {
		std::cerr << "[ScriptManager] DLL not found:  " << dllPath << std::endl;
		return false;
	}
	entry.hDll = LoadLibraryA(dllPath.c_str());
	if (!entry.hDll) {
		DWORD err = GetLastError();
		std::cerr << "[ScriptManager] LoadLibrary failed (" << err << "): " << dllPath << std::endl;
		return false;
	}
	return true;
}

void ScriptManager::UnloadDllEntry(DllEntry& entry)
{
	// 全てのインスタンスを破棄(DLLの DestroyScriptInstance を使う)
	if (entry.hDll) {
		auto destroyFunc = (DestroyScriptInstanceFunc)GetProcAddress(entry.hDll, "DestroyScriptInstance");
		for (auto& kv : entry.instances) {
			IScript* inst = kv.second;
			if (inst) {
				inst->EndPlay();
				if (destroyFunc) destroyFunc(inst);
				else delete inst;
			}
		}
		entry.instances.clear();

		FreeLibrary(entry.hDll);
		entry.hDll = nullptr;
		entry.refCount = 0;
	}
}

std::string ScriptManager::GetVSDevEnvPath() const
{
	const std::vector<std::string> vsPaths = {
		"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat",
		"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat",
		"C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Auxiliary\\Build\\vcvars64.bat"
	};
	for (const auto& p : vsPaths) {
		if (fs::exists(p)) return p;
	}
	return "";
}

void ScriptManager::RegisterAllScripts()
{
	std::lock_guard<std::mutex> lk(_mutex);

	std::string srcDirStr = SettingManager::GetInstance()->GetScriptFilePath();
	if (srcDirStr.empty()) return;

	fs::path srcDir = srcDirStr;
	if (!fs::exists(srcDir)) return;

	for (auto& p : fs::directory_iterator(srcDir)) {
		if (!p.is_regular_file()) continue;
		auto ext = p.path().extension().string();
		if (ext != ".cpp" && ext != ".CPP" && ext != ".cxx") continue;

		std::string name = p.path().stem().string();
		auto it = _dllMap.find(name);
		if (it == _dllMap.end()) {
			DllEntry entry;
			try {
				entry.lastCppWriteTime = fs::last_write_time(p.path());
			}
			catch (...) {
				// if cannot read time, leave default
			}
			// hDll==nullptr, instances empty, refCount==0 の仮エントリを作るだけ
			_dllMap.emplace(name, std::move(entry));
		}
		else {
			// 既に存在する場合はタイムスタンプを最新にしておく(Update の処理判断の基準になる)
			try {
				it->second.lastCppWriteTime = fs::last_write_time(p.path());
			}
			catch (...) {}
		}
	}
}

ScriptManager::BuildResult ScriptManager::BuildScriptDll(const std::string& scriptName)
{
	BuildResult result;
	result.success = false;

	// Paths
	std::string srcPath = SettingManager::GetInstance()->GetScriptFilePath() + scriptName + ".cpp";
	std::string headerPath = SettingManager::GetInstance()->GetScriptFilePath() + scriptName + ".h";
	std::string includeDir = SettingManager::GetInstance()->GetScriptFilePath() + "Include";
	std::string binDir = SettingManager::GetInstance()->GetDLLFilePath();
	std::string dllPath = binDir + "/" + scriptName + ".dll";
	std::string libPath = binDir + "/" + scriptName + ".lib";
	std::string pdbPath = binDir + "/" + scriptName + ".pdb";

	// ログファイルパス
	std::string logFile = SettingManager::GetInstance()->GetScriptLogFilePath() + "build_" + scriptName + ".log";

	// check vcvars
	std::string vcvars = GetVSDevEnvPath();
	if (vcvars.empty()) {
		result.errorMessage = "vcvars64.bat not found.  Please install Visual Studio 2022.";
		result.log = result.errorMessage;
		return result;
	}

	// ソースファイルが存在するか確認
	if (!fs::exists(srcPath)) {
		result.errorMessage = "Source file not found: " + srcPath;
		result.log = result.errorMessage;
		return result;
	}

	// ensure directories
	try {
		fs::create_directories(binDir);
		fs::path logDir = fs::path(logFile).parent_path();
		fs::create_directories(logDir);
	}
	catch (...) {}

	// 古いDLLのタイムスタンプを記録（ビルド前）
	fs::file_time_type oldDllTime;
	bool hadOldDll = false;
	if (fs::exists(dllPath)) {
		try {
			oldDllTime = fs::last_write_time(dllPath);
			hadOldDll = true;
		}
		catch (...) {}
	}

	// Build command
	std::string engineLib = includeDir + "\\Pixeon_Engine.lib";
	std::ostringstream cmd;
	cmd << "cmd /C \"call \"" << vcvars << "\" && "
		<< "cl /LD /EHsc /MD "
		<< "\"" << srcPath << "\" "
		<< "\"" + SettingManager::GetInstance()->GetScriptFilePath() + "Include/IScript.cpp\" "
		<< "/Fe:\"" << dllPath << "\" "
		<< "/I\"" << includeDir << "\" "
		<< "/link /LIBPATH:\"" << includeDir << "\" \"" << engineLib << "\" user32.lib "
		<< "/IMPLIB:\"" << libPath << "\" "
		<< "/PDB:\"" << pdbPath << "\""
		<< " > \"" << logFile << "\" 2>&1\"";

	std::string command = cmd.str();

	std::cout << "[ScriptManager] Running build command: " << command << std::endl;
	int exitCode = std::system(command.c_str());

	// ログファイルを読み込む
	if (fs::exists(logFile)) {
		std::ifstream ifs(logFile);
		std::ostringstream ss;
		ss << ifs.rdbuf();
		result.log = ss.str();
	}
	else {
		result.log = "No build log produced. ";
	}

	// ビルド結果を判定
	if (exitCode != 0) {
		result.errorMessage = "cl.exe returned error code " + std::to_string(exitCode);
		result.log = "Build failed (Exit code: " + std::to_string(exitCode) + ")\n\n" + result.log;
		result.success = false;
		return result;
	}

	// DLLが存在するか確認
	if (!fs::exists(dllPath)) {
		result.errorMessage = "DLL not found after build: " + dllPath;
		result.log = "Build failed:  DLL was not created.\n\n" + result.log;
		result.success = false;
		return result;
	}

	// DLLが実際に更新されたか確認（タイムスタンプチェック）
	if (hadOldDll) {
		try {
			fs::file_time_type newDllTime = fs::last_write_time(dllPath);
			if (newDllTime <= oldDllTime) {
				// DLLが更新されていない = ビルド失敗
				result.errorMessage = "DLL was not updated after build (compilation likely failed)";
				result.log = "Build failed: DLL timestamp unchanged.\n\n" + result.log;
				result.success = false;
				return result;
			}
		}
		catch (...) {
			// タイムスタンプ取得失敗時は警告を追加
			result.log = "Warning: Could not verify DLL update time.\n\n" + result.log;
		}
	}

	// 成功
	result.success = true;
	result.log = "Build succeeded.\n\n" + result.log;

	// update last write time
	try {
		if (fs::exists(srcPath)) {
			_dllMap[scriptName].lastCppWriteTime = fs::last_write_time(srcPath);
		}
	}
	catch (...) {}

	return result;
}