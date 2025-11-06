#include "ScriptManager.h"
#include "ScripComponent.h"
#include "IScript.h"

#include <iostream>
#include <sstream>
#include <cstdlib> // system
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

// Create/Destroy function types (DLL側でエクスポートされていること)
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

IScript* ScriptManager::CreateScriptInstance(const std::string& scriptName, ScripComponent* owner)
{
    std::lock_guard<std::mutex> lk(_mutex);

    // エントリ作成または取得
    auto& entry = _dllMap[scriptName];

    // ソース/バイナリパス
    std::string srcPath = "Script/Src/" + scriptName + ".cpp";
    std::string dllPath = "Script/Bin/" + scriptName + ".dll";

    // 初回か、ソースが新しいかをチェックしビルドが必要ならビルド
    bool needBuild = false;
    if (!fs::exists(dllPath)) {
        needBuild = true;
    }
    else if (fs::exists(srcPath)) {
        auto cppTime = fs::last_write_time(srcPath);
        if (entry.lastCppWriteTime != cppTime) {
            // 時刻が変わっている（初期時は default なので build）
            if (cppTime > fs::last_write_time(dllPath)) {
                needBuild = true;
            }
            entry.lastCppWriteTime = cppTime;
        }
    }

    if (needBuild) {
        std::string err;
        if (!BuildScriptDll(scriptName, err)) {
            std::cerr << "[ScriptManager] Build failed for " << scriptName << ": " << err << std::endl;
            return nullptr;
        }
    }

    // DLLが未ロードならロード
    if (entry.hDll == nullptr) {
        if (!LoadDllForScript(scriptName, entry)) {
            std::cerr << "[ScriptManager] Load DLL failed for " << scriptName << std::endl;
            return nullptr;
        }
    }

    // CreateScriptInstance 関数を取り出してインスタンス生成
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
        // EndPlay 呼び出し（安全のため）してから Destroy
        instance->EndPlay();

        // DestroyScriptInstance を DLL から呼ぶ（あれば）
        auto destroyFunc = (DestroyScriptInstanceFunc)GetProcAddress(entry.hDll, "DestroyScriptInstance");
        if (destroyFunc) {
            destroyFunc(instance);
        }
        else {
            // 万一エクスポートがなければ delete
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

        std::string srcPath = "Script/Src/" + scriptName + ".cpp";
        std::string dllPath = "Script/Bin/" + scriptName + ".dll";

        bool needBuild = false;
        if (fs::exists(srcPath)) {
            auto cppTime = fs::last_write_time(srcPath);
            if (entry.lastCppWriteTime != cppTime) {
                // ソースタイムが変化
                entry.lastCppWriteTime = cppTime;
                // 比較：cpp > dll
                if (!fs::exists(dllPath) || cppTime > fs::last_write_time(dllPath)) {
                    needBuild = true;
                }
            }
        }

        if (needBuild) {
            std::cout << "[ScriptManager] Detected change in " << scriptName << ", building..." << std::endl;
            std::string err;
            if (BuildScriptDll(scriptName, err)) {
                std::cout << "[ScriptManager] Build succeeded for " << scriptName << ", reloading..." << std::endl;

                // ホットリロード処理：
                // 1) 古いインスタンスを DestroyScriptInstance で削除
                // 2) FreeLibrary
                // 3) LoadLibrary (新DLL)
                // 4) 各 owner に対して CreateScriptInstance を呼んで新インスタンスを再作成し BeginPlay を呼ぶ

                // 保存しておくデータ
                auto oldH = entry.hDll;
                auto owners = std::vector<ScripComponent*>{};
                for (auto& kv : entry.instances) owners.push_back(kv.first);

                // Destroy old instances
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
                    // erase mapping now
                    entry.instances.erase(itInst);
                }

                // Free old dll
                if (oldH) {
                    FreeLibrary(oldH);
                    entry.hDll = nullptr;
                }

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
                        // BeginPlay はここで呼ぶ（必要なら）
                        newInst->BeginPlay();
                    }
                }

                entry.refCount = (int)entry.instances.size();
                std::cout << "[ScriptManager] Reload completed for " << scriptName << std::endl;
            }
            else {
                std::cerr << "[ScriptManager] Build failed for " << scriptName << ": " << err << std::endl;
            }
        }

        ++it;
    }
}

bool ScriptManager::LoadDllForScript(const std::string& scriptName, DllEntry& entry)
{
    std::string dllPath = "Script/Bin/" + scriptName + ".dll";
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
    // すべてのインスタンスを破棄（DLLの DestroyScriptInstance を使う）
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

bool ScriptManager::BuildScriptDll(const std::string& scriptName, std::string& outError)
{
    // Paths
    std::string srcPath = "Script/Src/" + scriptName + ".cpp";
    std::string headerPath = "Script/Src/" + scriptName + ".h";
    std::string includeDir = "Script/Include";
    std::string binDir = "Script/Bin";
    std::string dllPath = binDir + "/" + scriptName + ".dll";
    std::string libPath = binDir + "/" + scriptName + ".lib";
    std::string pdbPath = binDir + "/" + scriptName + ".pdb";

    // check vcvars
    std::string vcvars = GetVSDevEnvPath();
    if (vcvars.empty()) {
        outError = "vcvars64.bat not found";
        return false;
    }

    // ensure directories
    try {
        fs::create_directories(binDir);
    }
    catch (...) {}

    // Build command (simple synchronous)
    // Note: adjust Engine lib name if needed
    std::string engineLib = includeDir + "\\Pixeon.lib"; // adjust as needed
    std::ostringstream cmd;
    // Wrap with call to vcvars and cl compile & link
    cmd << "cmd /C \"call \"" << vcvars << "\" && "
        << "cl /LD /EHsc /MD "
        << "\"" << srcPath << "\" "
        << "\"Script/Include/IScript.cpp\" "
        << "/Fe:\"" << dllPath << "\" "
        << "/I\"" << includeDir << "\" "
        << "/link /LIBPATH:\"" << includeDir << "\" \"" << engineLib << "\" user32.lib "
        << "/IMPLIB:\"" << libPath << "\" "
        << "/PDB:\"" << pdbPath << "\"\"";

    std::string command = cmd.str();

    std::cout << "[ScriptManager] Running build command: " << command << std::endl;
    int result = std::system(command.c_str()); // blocking
    if (result != 0) {
        outError = "cl returned error code " + std::to_string(result);
        return false;
    }

    // verify build produced dll
    if (!fs::exists(dllPath)) {
        outError = "dll not found after build: " + dllPath;
        return false;
    }

    // update last write time
    try {
        if (fs::exists(srcPath)) {
            _dllMap[scriptName].lastCppWriteTime = fs::last_write_time(srcPath);
        }
    }
    catch (...) {}

    return true;
}