/*
* ファイル名　GameBuilder
* 説　　　明　プロジェクト (SceneRoot) から配布可能なゲームを組み立てる
*             エディタのビルドウィンドウと、コマンドラインの BuildExe の両方から使う
*
* 出力構成
*   <出力先>/
*     <ゲーム名>.exe            … ランタイム (Exe プロジェクト) をリネームしたもの
*     Pixeon_Engine.dll         … エンジン本体
*     *.dll                     … assimp など依存 DLL
*     game.json                 … 起動設定（開始シーン・描画バックエンド・解像度）
*     data.pak                  … アセット・シーン・シェーダー・設定のアーカイブ
*     SceneRoot/Script/DLL/     … スクリプト DLL（LoadLibrary で読むためアーカイブ外）
*
* 手順
*   1. 検証       開始シーンの存在、ランタイムファイルの存在
*   2. シェーダー  DXBC (.cso) の存在・鮮度確認、Vulkan 用に SPIR-V を生成
*   3. アーカイブ  assets/ scene/ shader/ config/ anim2d/ を data.pak へ
*   4. ランタイム  exe / dll / スクリプト DLL をコピー
*   5. 起動設定    game.json を書き出す
*/
#ifndef GAME_BUILDER_H
#define GAME_BUILDER_H

#include "GameManifest.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// プロジェクト内の各ディレクトリ（エディタ設定と同じ既定値）
struct ProjectLayout {
	std::string assetsDir = "SceneRoot/Assets/";
	std::string sceneDir = "SceneRoot/Scene/";
	std::string hlslDir = "SceneRoot/Shader/hlsl/";
	std::string csoDir = "SceneRoot/Shader/cso/";
	std::string spirvDir = "SceneRoot/Shader/spv/";
	std::string configDir = "SceneRoot/config/";
	std::string anim2dDir = "SceneRoot/Editor/Project/";
	std::string scriptDllDir = "SceneRoot/Script/DLL/";
};

struct BuildSettings {
	std::string projectRoot = ".";          // SceneRoot を含むディレクトリ
	ProjectLayout layout;
	std::string outputDir = "Build/";
	std::string runtimeDir = ".";           // ランタイム exe / エンジン dll があるディレクトリ
	std::string runtimeExeName = "Exe.exe";
	std::string engineDllName = "Pixeon_Engine.dll";
	GameManifest manifest;
	bool b_Clean = true;                    // 出力先を空にしてから組み立てる
	bool b_RequireRuntime = true;           // ランタイムが無ければ失敗にする（false なら警告）
	std::string spirvCompiler;              // 空なら自動検出
};

struct BuildReport {
	enum class Level { Info, Warning, Error };
	struct Message {
		Level e_Level = Level::Info;
		std::string text;
	};
	bool b_Success = false;
	std::vector<Message> messages;
	size_t i_ArchiveFiles = 0;
	uint64_t i_ArchiveBytes = 0;
	double f_Seconds = 0.0;

	size_t CountGet(Level level) const;
	std::string TextGet() const;
};

class GameBuilder {
public:
	// 進捗通知（0.0 - 1.0、現在の工程名）
	using ProgressCallback = std::function<void(float progress, const std::string& step)>;

	bool BuildRun(const BuildSettings& settings, BuildReport& out_report, const ProgressCallback& progress = nullptr);

private:
	bool SettingsValidate();
	bool ShadersPrepare();
	bool ArchiveCreate();
	bool RuntimeCopy();
	bool ManifestWrite();

	std::string ProjectPath(const std::string& relative) const;
	void Log(BuildReport::Level level, const std::string& text);
	void Progress(float value, const std::string& step);

	BuildSettings m_str_Settings;
	BuildReport* m_str_Report = nullptr;
	ProgressCallback m_Progress;
	std::vector<std::string> m_SpirvFiles;   // 生成した SPIR-V（アーカイブへ追加）
};

#endif // !GAME_BUILDER_H
