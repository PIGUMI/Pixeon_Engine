/*
* ファイル名　GameBuilder
* 説　　　明　ゲームビルド処理の実装
*/
#include "GameBuilder.h"
#include "AssetType.h"
#include "FileUtil.h"
#include "PixArchive.h"
#include "ShaderCompiler.h"
#include <algorithm>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

namespace {
	constexpr const char* MANIFEST_FILE_NAME = "game.json";

	/*
	* 関数名　FileNameSanitize
	* 引　数　name：ゲーム名
	* 戻り値　ファイル名に使える文字列
	* 説　明　Windows のファイル名に使えない文字を '_' に置き換える
	*/
	std::string FileNameSanitize(const std::string& name)
	{
		std::string out;
		for (char c : name) out.push_back(std::string("\\/:*?\"<>|").find(c) != std::string::npos ? '_' : c);
		while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
		return out.empty() ? "Game" : out;
	}

	/*
	* 関数名　FilesCollect
	* 引　数　dir：ディレクトリ　recursive：再帰するか
	* 戻り値　通常ファイルの一覧（パス昇順）
	* 説　明　ディレクトリ内のファイルを列挙する（存在しなければ空）
	*/
	std::vector<fs::path> FilesCollect(const fs::path& dir, bool recursive)
	{
		std::vector<fs::path> out;
		std::error_code ec;
		if (!fs::is_directory(dir, ec)) return out;
		if (recursive) {
			for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
				if (it->is_regular_file()) out.push_back(it->path());
		}
		else {
			for (auto it = fs::directory_iterator(dir, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
				if (it->is_regular_file()) out.push_back(it->path());
		}
		std::sort(out.begin(), out.end());
		return out;
	}

	std::string ExtensionLower(const fs::path& p)
	{
		return AssetTypeUtil::ExtensionGet(FileUtil::PathToUtf8(p.filename()));
	}
}

// ================================================================
// BuildReport
// ================================================================

size_t BuildReport::CountGet(Level level) const
{
	return (size_t)std::count_if(messages.begin(), messages.end(), [level](const Message& m) { return m.e_Level == level; });
}

/*
* 関数名　TextGet
* 引　数　なし
* 戻り値　ログ全文
* 説　明　ビルドログをテキストにまとめる（build_log.txt 用）
*/
std::string BuildReport::TextGet() const
{
	std::string text;
	for (auto& m : messages) {
		text += (m.e_Level == Level::Error) ? "[ERROR] " : (m.e_Level == Level::Warning) ? "[WARN ] " : "[INFO ] ";
		text += m.text + "\n";
	}
	return text;
}

// ================================================================
// GameBuilder
// ================================================================

/*
* 関数名　BuildRun
* 引　数　settings：ビルド設定　out_report：結果　progress：進捗通知
* 戻り値　成功なら true
* 説　明　検証 → シェーダー → アーカイブ → ランタイム → 起動設定 の順に実行する
*/
bool GameBuilder::BuildRun(const BuildSettings& settings, BuildReport& out_report, const ProgressCallback& progress)
{
	const auto t0 = std::chrono::steady_clock::now();
	m_str_Settings = settings;
	m_str_Report = &out_report;
	m_Progress = progress;
	m_SpirvFiles.clear();
	out_report = BuildReport{};

	Log(BuildReport::Level::Info, "ビルド開始: " + settings.manifest.name + " (" +
		GraphicsBackendUtil::NameGet(settings.manifest.backend) + ")");
	bool ok = SettingsValidate();
	if (ok) { Progress(0.10f, "シェーダー"); ok = ShadersPrepare(); }
	if (ok) { Progress(0.40f, "アーカイブ"); ok = ArchiveCreate(); }
	if (ok) { Progress(0.80f, "ランタイム"); ok = RuntimeCopy(); }
	if (ok) { Progress(0.95f, "起動設定"); ok = ManifestWrite(); }

	out_report.b_Success = ok && out_report.CountGet(BuildReport::Level::Error) == 0;
	out_report.f_Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	Log(out_report.b_Success ? BuildReport::Level::Info : BuildReport::Level::Error,
		out_report.b_Success ? "ビルド成功" : "ビルド失敗");

	const std::string log_text = out_report.TextGet();
	FileUtil::FileWriteAll(FileUtil::PathToUtf8(FileUtil::PathToNative(settings.outputDir) / "build_log.txt"), log_text.data(), log_text.size());
	Progress(1.0f, out_report.b_Success ? "完了" : "失敗");
	m_str_Report = nullptr;
	return out_report.b_Success;
}

std::string GameBuilder::ProjectPath(const std::string& relative) const
{
	const fs::path p = FileUtil::PathToNative(relative);
	if (p.is_absolute()) return relative;
	return FileUtil::PathToUtf8(FileUtil::PathToNative(m_str_Settings.projectRoot) / p);
}

void GameBuilder::Log(BuildReport::Level level, const std::string& text)
{
	if (m_str_Report) m_str_Report->messages.push_back({ level, text });
}

void GameBuilder::Progress(float value, const std::string& step)
{
	if (m_Progress) m_Progress(value, step);
}

/*
* 関数名　SettingsValidate
* 引　数　なし
* 戻り値　続行できるなら true
* 説　明　開始シーン・ランタイムの存在を確認し、出力先を準備する
*         出力先の削除は「空」または「以前のビルド結果 (game.json がある)」の場合のみ行う
*/
bool GameBuilder::SettingsValidate()
{
	const auto& s = m_str_Settings;
	std::error_code ec;
	const fs::path scene = FileUtil::PathToNative(ProjectPath(s.layout.sceneDir)) / (s.manifest.startScene + ".scene");
	if (s.manifest.startScene.empty() || !fs::is_regular_file(scene, ec)) {
		Log(BuildReport::Level::Error, "開始シーンが見つかりません: " + FileUtil::PathToUtf8(scene));
		return false;
	}

	for (const std::string& name : { s.runtimeExeName, s.engineDllName }) {
		const fs::path p = FileUtil::PathToNative(s.runtimeDir) / name;
		if (!fs::is_regular_file(p, ec)) {
			Log(s.b_RequireRuntime ? BuildReport::Level::Error : BuildReport::Level::Warning,
				"ランタイムファイルが見つかりません: " + FileUtil::PathToUtf8(p) + "（Release|x64 でソリューションをビルドしてください）");
			if (s.b_RequireRuntime) return false;
		}
	}

	const fs::path out = FileUtil::PathToNative(s.outputDir);
	if (s.b_Clean && fs::exists(out, ec)) {
		const bool empty = fs::is_directory(out, ec) && fs::directory_iterator(out, ec) == fs::directory_iterator();
		const bool previous_build = fs::exists(out / MANIFEST_FILE_NAME, ec) || fs::exists(out / "build_log.txt", ec);
		if (!empty && !previous_build) {
			Log(BuildReport::Level::Error, "出力先が空ではなく、以前のビルド結果でもないため削除しません: " + s.outputDir);
			return false;
		}
		fs::remove_all(out, ec);
	}
	fs::create_directories(out, ec);
	if (!fs::is_directory(out, ec)) {
		Log(BuildReport::Level::Error, "出力先を作成できません: " + s.outputDir);
		return false;
	}
	return true;
}

/*
* 関数名　ShadersPrepare
* 引　数　なし
* 戻り値　続行できるなら true
* 説　明　DXBC は存在と鮮度を確認し、SPIR-V は glslangValidator で生成する
*         Vulkan を選んだのにコンパイラが無い場合は失敗、それ以外は警告に留める
*/
bool GameBuilder::ShadersPrepare()
{
	const auto& s = m_str_Settings;
	const bool need_dx = s.manifest.backend != GraphicsBackend::Vulkan;
	const bool need_vk = s.manifest.backend == GraphicsBackend::Vulkan;
	const fs::path hlsl_dir = FileUtil::PathToNative(ProjectPath(s.layout.hlslDir));
	const fs::path cso_dir = FileUtil::PathToNative(ProjectPath(s.layout.csoDir));
	const fs::path spv_dir = FileUtil::PathToNative(ProjectPath(s.layout.spirvDir));

	std::string compiler = s.spirvCompiler.empty() ? ShaderCompiler::SpirvCompilerFind() : s.spirvCompiler;
	if (compiler.empty()) {
		Log(need_vk ? BuildReport::Level::Error : BuildReport::Level::Warning,
			"glslangValidator が見つからないため Vulkan 用シェーダーを生成できません（Vulkan SDK をインストールしてください）");
		if (need_vk) return false;
	}

	std::error_code ec;
	size_t compiled = 0;
	std::vector<std::string> stale;
	for (auto& hlsl : FilesCollect(hlsl_dir, false)) {
		ShaderBindings::Stage stage{};
		const std::string name = FileUtil::PathToUtf8(hlsl.stem());
		if (ExtensionLower(hlsl) != "hlsl" || !ShaderCompiler::StageFromName(name, stage)) continue;

		const fs::path cso = cso_dir / (hlsl.stem().string() + ".cso");
		if (!fs::exists(cso, ec)) {
			Log(need_dx ? BuildReport::Level::Error : BuildReport::Level::Warning, "DXBC がありません（エディタを起動すると生成されます）: " + name);
		}
		else if (fs::last_write_time(cso, ec) < fs::last_write_time(hlsl, ec)) {
			stale.push_back(name);
		}

		if (compiler.empty()) continue;
		const fs::path spv = spv_dir / (hlsl.stem().string() + ".spv");
		std::string log;
		if (!ShaderCompiler::SpirvCompile(compiler, FileUtil::PathToUtf8(hlsl), stage, FileUtil::PathToUtf8(spv), log)) {
			Log(need_vk ? BuildReport::Level::Error : BuildReport::Level::Warning, "SPIR-V コンパイル失敗: " + name + "\n" + log);
			continue;
		}
		++compiled;
	}
	if (!stale.empty() && need_dx) {
		std::string names;
		for (auto& n : stale) names += (names.empty() ? "" : ", ") + n;
		Log(BuildReport::Level::Warning, "HLSL より古い DXBC が " + std::to_string(stale.size()) +
			" 件あります。変更を反映するにはエディタで再コンパイルしてください: " + names);
	}
	if (!compiler.empty()) Log(BuildReport::Level::Info, "SPIR-V を生成: " + std::to_string(compiled) + " 件");
	return m_str_Report->CountGet(BuildReport::Level::Error) == 0;
}

/*
* 関数名　ArchiveCreate
* 引　数　なし
* 戻り値　成功なら true
* 説　明　実行時に必要なファイルだけを仮想パスへ割り当ててアーカイブ化する
*/
bool GameBuilder::ArchiveCreate()
{
	const auto& s = m_str_Settings;
	PixArchive::Writer writer;
	writer.SetAlignment(PixArchive::ALIGNMENT_DEFAULT);

	struct Mount {
		const char* prefix;
		std::string dir;
		bool recursive;
		std::vector<std::string> exts;   // 空なら IsRuntimeAsset で判定
	};
	const std::vector<Mount> mounts = {
		{ "assets", s.layout.assetsDir, true, {} },
		{ "scene", s.layout.sceneDir, false, { "scene", "sceneconfig", "meta" } },
		{ "shader/cso", s.layout.csoDir, false, { "cso" } },
		{ "shader/spv", s.layout.spirvDir, false, { "spv" } },
		{ "config", s.layout.configDir, false, { "json" } },
		{ "anim2d", s.layout.anim2dDir, false, { "anim2d" } },
	};

	for (auto& m : mounts) {
		const fs::path root = FileUtil::PathToNative(ProjectPath(m.dir));
		size_t count = 0;
		for (auto& file : FilesCollect(root, m.recursive)) {
			const std::string rel = FileUtil::PathToUtf8(fs::relative(file, root));
			const AssetType type = AssetTypeUtil::TypeFromPath(rel);
			const bool take = m.exts.empty() ? AssetTypeUtil::IsRuntimeAsset(type)
				: std::find(m.exts.begin(), m.exts.end(), ExtensionLower(file)) != m.exts.end();
			if (!take) continue;
			writer.FileAdd(std::string(m.prefix) + "/" + rel, FileUtil::PathToUtf8(file), (uint8_t)type);
			++count;
		}
		Log(BuildReport::Level::Info, std::string(m.prefix) + "/ : " + std::to_string(count) + " files");
	}

	const fs::path pak = FileUtil::PathToNative(s.outputDir) / FileUtil::PathToNative(s.manifest.archive);
	const size_t total = writer.GetFileCount();
	const bool ok = writer.ArchiveWrite(FileUtil::PathToUtf8(pak), [&](size_t done, size_t all, const std::string&) {
		Progress(0.40f + 0.40f * (float)done / (float)std::max<size_t>(all, 1), "アーカイブ");
		});
	if (!ok) {
		Log(BuildReport::Level::Error, "アーカイブ作成失敗: " + writer.GetLastError());
		return false;
	}
	std::error_code ec;
	m_str_Report->i_ArchiveFiles = total;
	m_str_Report->i_ArchiveBytes = (uint64_t)fs::file_size(pak, ec);
	Log(BuildReport::Level::Info, "アーカイブ: " + FileUtil::PathToUtf8(pak) + " (" + std::to_string(total) + " files, "
		+ std::to_string(m_str_Report->i_ArchiveBytes / 1024) + " KB)");
	return true;
}

/*
* 関数名　RuntimeCopy
* 引　数　なし
* 戻り値　成功なら true
* 説　明　ランタイム exe をゲーム名にリネームしてコピーし、DLL 群とスクリプト DLL を配置する
*/
bool GameBuilder::RuntimeCopy()
{
	const auto& s = m_str_Settings;
	const fs::path runtime = FileUtil::PathToNative(s.runtimeDir);
	const fs::path out = FileUtil::PathToNative(s.outputDir);
	std::error_code ec;

	const fs::path exe = runtime / s.runtimeExeName;
	if (fs::exists(exe, ec)) {
		const fs::path dst = out / FileUtil::PathToNative(FileNameSanitize(s.manifest.name) + fs::path(s.runtimeExeName).extension().string());
		fs::copy_file(exe, dst, fs::copy_options::overwrite_existing, ec);
		if (ec) { Log(BuildReport::Level::Error, "exe のコピーに失敗: " + ec.message()); return false; }
	}

	size_t dll_count = 0;
	for (auto& file : FilesCollect(runtime, false)) {
		if (ExtensionLower(file) != "dll") continue;
		fs::copy_file(file, out / file.filename(), fs::copy_options::overwrite_existing, ec);
		if (ec) { Log(BuildReport::Level::Error, "DLL のコピーに失敗: " + FileUtil::PathToUtf8(file)); return false; }
		++dll_count;
	}
	Log(BuildReport::Level::Info, "ランタイム DLL: " + std::to_string(dll_count) + " 件");

	const fs::path script_src = FileUtil::PathToNative(ProjectPath(s.layout.scriptDllDir));
	const fs::path script_dst = out / FileUtil::PathToNative(s.layout.scriptDllDir);
	size_t script_count = 0;
	for (auto& file : FilesCollect(script_src, false)) {
		if (ExtensionLower(file) != "dll") continue;
		fs::create_directories(script_dst, ec);
		fs::copy_file(file, script_dst / file.filename(), fs::copy_options::overwrite_existing, ec);
		if (ec) { Log(BuildReport::Level::Error, "スクリプト DLL のコピーに失敗: " + FileUtil::PathToUtf8(file)); return false; }
		++script_count;
	}
	Log(BuildReport::Level::Info, "スクリプト DLL: " + std::to_string(script_count) + " 件");
	return true;
}

/*
* 関数名　ManifestWrite
* 引　数　なし
* 戻り値　成功なら true
* 説　明　game.json を出力先に書き出す
*/
bool GameBuilder::ManifestWrite()
{
	const fs::path path = FileUtil::PathToNative(m_str_Settings.outputDir) / MANIFEST_FILE_NAME;
	if (!m_str_Settings.manifest.FileSave(FileUtil::PathToUtf8(path))) {
		Log(BuildReport::Level::Error, "game.json の書き込みに失敗しました");
		return false;
	}
	return true;
}
