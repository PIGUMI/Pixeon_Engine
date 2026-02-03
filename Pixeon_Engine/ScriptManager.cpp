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

		// インスタンスのコピーを作成(イテレート中の変更を避けるため)
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
	std::string headerPath = SettingManager::GetInstance()->GetScriptFilePath() + scriptName + ".h";
	std::string dllPath = SettingManager::GetInstance()->GetDLLFilePath() + scriptName + ".dll";

	// ソースファイルの存在確認
	bool hasCpp = fs::exists(srcPath);
	bool hasH = fs::exists(headerPath);
	bool hasDll = fs::exists(dllPath);

	// タイムスタンプの初期化(初回のみ、ソースファイルがある場合)
	if (!entry.initialized) {
		try {
			if (hasCpp) {
				entry.lastCppWriteTime = fs::last_write_time(srcPath);
			}
			if (hasH) {
				entry.lastHWriteTime = fs::last_write_time(headerPath);
			}
			entry.initialized = true;
		}
		catch (...) {
			// タイムスタンプ取得失敗時は初期化済みフラグだけ立てる
			entry.initialized = true;
		}
	}

	// ビルドが必要かチェック
	bool needBuild = false;

	// ケース1: DLLもソースもない → エラー
	if (!hasDll && !hasCpp) {
		std::cerr << "[ScriptManager] Neither DLL nor source files found for: " << scriptName << std::endl;
		return nullptr;
	}

	// ケース2: DLLはないがソースがある → ビルド必須
	if (!hasDll && hasCpp) {
		needBuild = true;
		std::cout << "[ScriptManager] DLL not found but source exists, building: " << scriptName << std::endl;
	}

	// ケース3: DLLがあってソースもある → タイムスタンプチェック
	if (hasDll && (hasCpp || hasH)) {
		// .cppファイルの変更をチェック
		if (hasCpp) {
			try {
				auto currentCppTime = fs::last_write_time(srcPath);
				auto dllTime = fs::last_write_time(dllPath);

				// .cppファイルがDLLより新しい場合
				if (currentCppTime > dllTime) {
					needBuild = true;
					std::cout << "[ScriptManager] .cpp file modified, rebuilding: " << scriptName << std::endl;
				}

				// タイムスタンプを更新
				entry.lastCppWriteTime = currentCppTime;
			}
			catch (...) {
				std::cerr << "[ScriptManager] Failed to check .cpp timestamp for: " << scriptName << std::endl;
			}
		}

		// .hファイルの変更をチェック
		if (hasH) {
			try {
				auto currentHTime = fs::last_write_time(headerPath);
				auto dllTime = fs::last_write_time(dllPath);

				// .hファイルがDLLより新しい場合
				if (currentHTime > dllTime) {
					needBuild = true;
					std::cout << "[ScriptManager] .h file modified, rebuilding: " << scriptName << std::endl;
				}

				// タイムスタンプを更新
				entry.lastHWriteTime = currentHTime;
			}
			catch (...) {
				std::cerr << "[ScriptManager] Failed to check .h timestamp for: " << scriptName << std::endl;
			}
		}
	}

	// ケース4: DLLがあるがソースがない → ビルド不要、DLLをそのまま使用
	if (hasDll && !hasCpp && !hasH) {
		std::cout << "[ScriptManager] Using precompiled DLL (no source files): " << scriptName << std::endl;
		needBuild = false;
	}

	if (needBuild) {
		// ソースファイルがない場合はビルドできない
		if (!hasCpp) {
			std::cerr << "[ScriptManager] Cannot build: source file not found: " << srcPath << std::endl;
			if (hasDll) {
				std::cout << "[ScriptManager] Using existing DLL instead: " << scriptName << std::endl;
			}
			else {
				return nullptr;
			}
		}
		else {
			// ビルド前にDLLをアンロード(重要!)
			if (entry.hDll != nullptr) {
				std::cout << "[ScriptManager] Unloading DLL before rebuild: " << scriptName << std::endl;
				UnloadDllEntry(entry);
			}

			auto result = BuildScriptDll(scriptName);
			if (!result.success) {
				std::cerr << "[ScriptManager] Build failed for " << scriptName << ": " << result.errorMessage << std::endl;

				// Build失敗時: 既存のDLLが存在する場合はそれを使用
				if (hasDll) {
					std::cout << "[ScriptManager] Using existing DLL after build failure: " << scriptName << std::endl;
					// ビルド失敗したが既存DLLを使うので処理を続行
				}
				else {
					// DLLが存在せず、ビルドも失敗した場合はエラー
					std::cerr << "[ScriptManager] No DLL available and build failed for: " << scriptName << std::endl;
					return nullptr;
				}
			}
		}
	}

	// DLLがまだロードされていないならロード
	if (entry.hDll == nullptr) {
		if (!LoadDllForScript(scriptName, entry)) {
			std::cerr << "[ScriptManager] Load DLL failed for " << scriptName << std::endl;
			return nullptr;
		}
	}

	// CreateScriptInstance関数を呼び出してインスタンス作成
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
		// EndPlay 呼び出し(安全のため) → Destroy
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
		std::string headerPath = SettingManager::GetInstance()->GetScriptFilePath() + scriptName + ".h";
		std::string dllPath = SettingManager::GetInstance()->GetDLLFilePath() + scriptName + ".dll";

		// ソースファイルの存在確認
		bool hasCpp = fs::exists(srcPath);
		bool hasH = fs::exists(headerPath);
		bool hasDll = fs::exists(dllPath);

		// ソースファイルがない場合はホットリロード不可（DLLのみ運用）
		if (!hasCpp && !hasH) {
			++it;
			continue;
		}

		bool needBuild = false;

		// .cppファイルの変更をチェック
		if (hasCpp) {
			try {
				auto cppTime = fs::last_write_time(srcPath);
				// タイムスタンプが変わった かつ DLLより新しい
				if (cppTime != entry.lastCppWriteTime) {
					entry.lastCppWriteTime = cppTime;
					if (!hasDll || cppTime > fs::last_write_time(dllPath)) {
						needBuild = true;
						std::cout << "[ScriptManager] .cpp file changed: " << scriptName << std::endl;
					}
				}
			}
			catch (...) {}
		}

		// .hファイルの変更をチェック
		if (hasH) {
			try {
				auto hTime = fs::last_write_time(headerPath);
				// タイムスタンプが変わった かつ DLLより新しい
				if (hTime != entry.lastHWriteTime) {
					entry.lastHWriteTime = hTime;
					if (!hasDll || hTime > fs::last_write_time(dllPath)) {
						needBuild = true;
						std::cout << "[ScriptManager] .h file changed: " << scriptName << std::endl;
					}
				}
			}
			catch (...) {}
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
				std::cout << "[ScriptManager] Build success: " << scriptName << std::endl;

				// reload dll
				if (LoadDllForScript(scriptName, entry)) {
					std::cout << "[ScriptManager] DLL reloaded: " << scriptName << std::endl;

					// recreate instances
					auto createFunc = (CreateScriptInstanceFunc)GetProcAddress(entry.hDll, "CreateScriptInstance");
					if (createFunc) {
						for (auto owner : owners) {
							IScript* newInst = createFunc();
							if (newInst) {
								entry.instances[owner] = newInst;
								entry.refCount++;
								newInst->SetOwnerComponent(owner);

								// CRITICAL: SetParentObject/Sceneを設定しないとBeginPlayで例外が発生
								try {
									// ScripComponentから親オブジェクトを取得
									AbstractObject* parentObj = owner->GetParent();
									if (parentObj) {
										newInst->SetParentObject(static_cast<Object>(parentObj));

										// 親シーンを取得
										AbstractScene* parentScene = parentObj->GetParentScene();
										if (parentScene) {
											newInst->SetParentScene(static_cast<Scene>(parentScene));
										}
									}
								}
								catch (const std::exception& e) {
									std::cerr << "[ScriptManager] Exception while setting parent: " << e.what() << std::endl;
								}

								// BeginPlayを呼び出し
								try {
									newInst->BeginPlay();
								}
								catch (const std::exception& e) {
									std::cerr << "[ScriptManager] Exception in BeginPlay during hot reload: " << e.what() << std::endl;
								}
							}
						}
					}
				}
				else {
					std::cerr << "[ScriptManager] Failed to reload DLL after build: " << scriptName << std::endl;
				}
			}
			else {
				std::cerr << "[ScriptManager] Build failed: " << scriptName << std::endl;
				std::cerr << result.log << std::endl;

				// Build失敗時: 既存のDLLを再ロードして使用
				if (hasDll) {
					std::cout << "[ScriptManager] Attempting to use existing DLL after build failure: " << scriptName << std::endl;

					if (LoadDllForScript(scriptName, entry)) {
						std::cout << "[ScriptManager] Successfully loaded existing DLL: " << scriptName << std::endl;

						// recreate instances with existing DLL
						auto createFunc = (CreateScriptInstanceFunc)GetProcAddress(entry.hDll, "CreateScriptInstance");
						if (createFunc) {
							for (auto owner : owners) {
								IScript* newInst = createFunc();
								if (newInst) {
									entry.instances[owner] = newInst;
									entry.refCount++;
									newInst->SetOwnerComponent(owner);

									// CRITICAL: SetParentObject/Sceneを設定しないとBeginPlayで例外が発生
									try {
										AbstractObject* parentObj = owner->GetParent();
										if (parentObj) {
											newInst->SetParentObject(static_cast<Object>(parentObj));

											AbstractScene* parentScene = parentObj->GetParentScene();
											if (parentScene) {
												newInst->SetParentScene(static_cast<Scene>(parentScene));
											}
										}
									}
									catch (const std::exception& e) {
										std::cerr << "[ScriptManager] Exception while setting parent: " << e.what() << std::endl;
									}

									// BeginPlayを呼び出し
									try {
										newInst->BeginPlay();
									}
									catch (const std::exception& e) {
										std::cerr << "[ScriptManager] Exception in BeginPlay during fallback: " << e.what() << std::endl;
									}
								}
							}
						}
					}
					else {
						std::cerr << "[ScriptManager] Failed to load existing DLL: " << scriptName << std::endl;
					}
				}
				else {
					std::cerr << "[ScriptManager] No existing DLL to fallback to: " << scriptName << std::endl;
				}
			}
		}

		++it;
	}
}

bool ScriptManager::LoadDllForScript(const std::string& scriptName, DllEntry& entry)
{
	std::string dllPath = SettingManager::GetInstance()->GetDLLFilePath() + scriptName + ".dll";
	if (!fs::exists(dllPath)) {
		std::cerr << "[ScriptManager] DLL not found: " << dllPath << std::endl;
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
	std::string dllDirStr = SettingManager::GetInstance()->GetDLLFilePath();

	if (srcDirStr.empty() && dllDirStr.empty()) return;

	// ソースディレクトリから.cppファイルを探す
	if (!srcDirStr.empty()) {
		fs::path srcDir = srcDirStr;
		if (fs::exists(srcDir)) {
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

						// 対応する.hファイルがあればそのタイムスタンプも記録
						std::string hPath = srcDirStr + name + ".h";
						if (fs::exists(hPath)) {
							entry.lastHWriteTime = fs::last_write_time(hPath);
						}

						entry.initialized = true;
					}
					catch (...) {
						// if cannot read time, leave default
					}
					_dllMap.emplace(name, std::move(entry));
				}
				else {
					// 既に存在する場合はタイムスタンプを最新にしておく
					try {
						it->second.lastCppWriteTime = fs::last_write_time(p.path());

						std::string hPath = srcDirStr + name + ".h";
						if (fs::exists(hPath)) {
							it->second.lastHWriteTime = fs::last_write_time(hPath);
						}

						it->second.initialized = true;
					}
					catch (...) {}
				}
			}
		}
	}

	// DLLディレクトリから.dllファイルを探す（ソースがなくても登録）
	if (!dllDirStr.empty()) {
		fs::path dllDir = dllDirStr;
		if (fs::exists(dllDir)) {
			for (auto& p : fs::directory_iterator(dllDir)) {
				if (!p.is_regular_file()) continue;
				auto ext = p.path().extension().string();
				if (ext != ".dll" && ext != ".DLL") continue;

				std::string name = p.path().stem().string();

				// まだ登録されていない場合のみ追加（ソースから登録済みの場合はスキップ）
				auto it = _dllMap.find(name);
				if (it == _dllMap.end()) {
					DllEntry entry;
					entry.initialized = true; // DLLのみの場合は初期化済みとする
					_dllMap.emplace(name, std::move(entry));
					std::cout << "[ScriptManager] Registered precompiled DLL: " << name << std::endl;
				}
			}
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
		result.errorMessage = "vcvars64.bat not found. Visual Studio 2022 may not be installed.";
		result.log = result.errorMessage;
		std::cout << "[ScriptManager] " << result.errorMessage << std::endl;
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

	// 古いDLLのタイムスタンプを記録(ビルド前)
	fs::file_time_type oldDllTime;
	bool hadOldDll = false;
	if (fs::exists(dllPath)) {
		try {
			oldDllTime = fs::last_write_time(dllPath);
			hadOldDll = true;
		}
		catch (...) {}
	}

	// ========================================
	// vcpkg パスの設定
	// ========================================
	std::string vcpkgRoot = SettingManager::GetInstance()->GetVcpkgFilePath();
	std::string vcpkgInclude = vcpkgRoot + "/installed/x64-windows/include";
	std::string vcpkgLib = vcpkgRoot + "/installed/x64-windows/lib";

	// vcpkgが存在するか確認
	bool hasVcpkg = fs::exists(vcpkgInclude);
	if (!hasVcpkg) {
		std::cout << "[ScriptManager] Warning: vcpkg not found at " << vcpkgRoot << std::endl;
	}

	// Build command
	std::string engineLib = includeDir + "\\Pixeon_Engine.lib";
	std::ostringstream cmd;
	cmd << "cmd /C \"call \"" << vcvars << "\" && "
		<< "cl /LD /EHsc /MD "
		<< "\"" << srcPath << "\" "
		<< "\"" + SettingManager::GetInstance()->GetScriptFilePath() + "Include/IScript.cpp\" "
		<< "/Fe:\"" << dllPath << "\" "
		<< "/I\"" << includeDir << "\" ";

	// vcpkgのインクルードパスを追加
	if (hasVcpkg) {
		cmd << "/I\"" << vcpkgInclude << "\" ";
	}

	cmd << "/link /LIBPATH:\"" << includeDir << "\" ";

	// vcpkgのライブラリパスを追加
	if (hasVcpkg) {
		cmd << "/LIBPATH:\"" << vcpkgLib << "\" ";
	}

	cmd << "\"" << engineLib << "\" user32.lib "
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
		result.log = "No build log produced.";
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
		result.log = "Build failed: DLL was not created.\n\n" + result.log;
		result.success = false;
		return result;
	}

	// DLLが実際に更新されたか確認(タイムスタンプチェック)
	if (hadOldDll) {
		try {
			fs::file_time_type newDllTime = fs::last_write_time(dllPath);
			if (newDllTime <= oldDllTime) {
				result.errorMessage = "DLL was not updated after build (compilation likely failed)";
				result.log = "Build failed: DLL timestamp unchanged.\n\n" + result.log;
				result.success = false;
				return result;
			}
		}
		catch (...) {
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
		if (fs::exists(headerPath)) {
			_dllMap[scriptName].lastHWriteTime = fs::last_write_time(headerPath);
		}
	}
	catch (...) {}

	return result;
}