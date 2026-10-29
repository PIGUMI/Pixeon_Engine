/*
* ファイル名　main (BuildExe)
* 説　　　明　コマンドラインからゲームをビルドするツール (PixeonBuild)
*             CI やバッチからエディタを起動せずにビルドできる
*
* 使用方法
*   PixeonBuild --project <SceneRoot を含むディレクトリ> --runtime <exe/dll のディレクトリ>
*               --out <出力先> [--name <ゲーム名>] [--version <版>] [--scene <開始シーン>]
*               [--backend dx11|dx12|vulkan] [--width N] [--height N] [--fullscreen]
*               [--no-vsync] [--no-clean] [--allow-missing-runtime] [--glslang <path>]
*/
#include "../Common/GameBuilder.h"
#include "../Common/FileUtil.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {
	void UsagePrint()
	{
		std::cout <<
			"使用方法:\n"
			"  PixeonBuild --project <dir> --runtime <dir> --out <dir>\n"
			"              [--name <name>] [--version <ver>] [--scene <start scene>]\n"
			"              [--backend dx11|dx12|vulkan] [--width N] [--height N] [--fullscreen]\n"
			"              [--no-vsync] [--no-clean] [--allow-missing-runtime] [--glslang <path>]\n";
	}

	/*
	* 関数名　ArgumentsParse
	* 引　数　argc/argv：コマンドライン　out_settings：出力
	* 戻り値　解釈できたら true
	* 説　明　コマンドラインをビルド設定に変換する
	*/
	bool ArgumentsParse(int argc, char* argv[], BuildSettings& out_settings)
	{
		auto& m = out_settings.manifest;
		for (int i = 1; i < argc; ++i) {
			const std::string a = argv[i];
			auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : ""; };
			if (a == "--project") out_settings.projectRoot = next();
			else if (a == "--runtime") out_settings.runtimeDir = next();
			else if (a == "--out") out_settings.outputDir = next();
			else if (a == "--name") m.name = next();
			else if (a == "--version") m.version = next();
			else if (a == "--scene") m.startScene = next();
			else if (a == "--width") m.i_Width = std::stoi(next());
			else if (a == "--height") m.i_Height = std::stoi(next());
			else if (a == "--fullscreen") m.b_Fullscreen = true;
			else if (a == "--no-vsync") m.b_VSync = false;
			else if (a == "--no-clean") out_settings.b_Clean = false;
			else if (a == "--allow-missing-runtime") out_settings.b_RequireRuntime = false;
			else if (a == "--glslang") out_settings.spirvCompiler = next();
			else if (a == "--backend") {
				const std::string name = next();
				if (!GraphicsBackendUtil::NameParse(name, m.backend)) {
					std::cout << "不明なバックエンド: " << name << "\n";
					return false;
				}
			}
			else if (a == "--help" || a == "-h") return false;
			else {
				std::cout << "不明な引数: " << a << "\n";
				return false;
			}
		}
		return true;
	}

	/*
	* 関数名　StartSceneResolve
	* 引　数　settings：ビルド設定
	* 戻り値　なし
	* 説　明　開始シーン未指定ならプロジェクトの SceneConfig.sceneconfig から取得する
	*/
	void StartSceneResolve(BuildSettings& settings)
	{
		if (!settings.manifest.startScene.empty()) return;
		std::string text;
		const std::string path = settings.projectRoot + "/" + settings.layout.sceneDir + "SceneConfig.sceneconfig";
		settings.manifest.startScene = "SampleScene";
		if (!FileUtil::TextReadAll(path, text)) return;
		const nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
		if (j.is_object()) settings.manifest.startScene = j.value("StartScene", settings.manifest.startScene);
	}
}

int main(int argc, char* argv[])
{
#if defined(_WIN32)
	SetConsoleOutputCP(CP_UTF8);
#endif
	BuildSettings settings;
	settings.manifest.startScene.clear();
	if (!ArgumentsParse(argc, argv, settings)) {
		UsagePrint();
		return 2;
	}
	StartSceneResolve(settings);

	GameBuilder builder;
	BuildReport report;
	builder.BuildRun(settings, report, [](float p, const std::string& step) {
		std::cout << "[" << (int)(p * 100.0f) << "%] " << step << "\n";
		});
	std::cout << report.TextGet();
	std::cout << "警告 " << report.CountGet(BuildReport::Level::Warning) << " 件 / エラー "
		<< report.CountGet(BuildReport::Level::Error) << " 件 (" << report.f_Seconds << " 秒)\n";
	return report.b_Success ? 0 : 1;
}
