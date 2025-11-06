#ifndef _SCRIPT_MANAGER_H_
#define _SCRIPT_MANAGER_H_

#include <string>
#include <map>
#include <filesystem>
#include <Windows.h>
#include <mutex>
#include <vector>
/*
 Scriptを管理するマネージャークラス
 DLLの読み込み・解放を行う
*/

// 前方宣言
class ScripComponent;
class IScript;

class ScriptManager
{
public:
    static ScriptManager& Instance();
    static void Release();

    // コンポーネント要求に応じてスクリプトインスタンスを作成し返す
    // 成功なら IScript* を返す。失敗なら nullptr。
    IScript* CreateScriptInstance(const std::string& scriptName, ScripComponent* owner);

    // コンポーネントが自分のスクリプトインスタンスを破棄するとき呼ぶ
    void DestroyScriptInstance(const std::string& scriptName, ScripComponent* owner);

    // 毎フレーム呼ぶ：ファイル更新チェック → 必要ならビルド → ホットリロード
    void Update();

    // VS 開発者コマンドプロンプトのパスを返す（vcvars64.bat）
    std::string GetVSDevEnvPath() const;

    void RegisterAllScripts();

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

    // 内部API
    bool BuildScriptDll(const std::string& scriptName, std::string& outError);
    bool LoadDllForScript(const std::string& scriptName, DllEntry& entry);
    void UnloadDllEntry(DllEntry& entry);

    std::map<std::string, DllEntry> _dllMap; // key = scriptName
    std::mutex _mutex;
};

#endif // _SCRIPT_MANAGER_H_

