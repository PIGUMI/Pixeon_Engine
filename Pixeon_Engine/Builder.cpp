/*
* ファイル名　Builder
* 説　　　明　エディタのビルドウィンドウ
*/
#include "Builder.h"
#include "GUI.h"
#include "File.h"
#include "SceneManger.h"
#include "SettingManager.h"
#include "ShaderManager.h"
#include <Windows.h>
#include <algorithm>

Builder* Builder::m_cls_Instance = nullptr;

namespace {
	// GUI::ShiftJISToUTF8 の短縮
	std::string U8(const std::string& s) { return GUI::GetInstance()->ShiftJISToUTF8(s); }

	/*
	* 関数名　ExePathGet
	* 引　数　out_dir：exe のディレクトリ　out_name：exe のファイル名
	* 戻り値　なし
	* 説　明　ランタイム（エディタ自身の exe と Pixeon_Engine.dll）の場所を取得する
	*/
	void ExePathGet(std::string& out_dir, std::string& out_name)
	{
		char path[MAX_PATH]{};
		GetModuleFileNameA(nullptr, path, MAX_PATH);
		const std::string full = path;
		const size_t pos = full.find_last_of("\\/");
		out_dir = (pos == std::string::npos) ? "." : full.substr(0, pos + 1);
		out_name = (pos == std::string::npos) ? full : full.substr(pos + 1);
	}
}

/*
* 関数名　:GetInstance
* 引　数　:なし
* 戻り値　:インスタンスポインター
* 概　要　:Builderクラスのインスタンスを取得する
*/
Builder* Builder::GetInstance()
{
	if (m_cls_Instance == nullptr) m_cls_Instance = new Builder();
	return m_cls_Instance;
}

/*
* 関数名　:DeleteInstance
* 引　数　:なし
* 戻り値　:なし
* 概　要　:Builderクラスのインスタンスを削除する（ビルド中なら完了を待つ）
*/
void Builder::DeleteInstance()
{
	delete m_cls_Instance;
	m_cls_Instance = nullptr;
}

Builder::~Builder()
{
	if (m_cls_Thread.joinable()) m_cls_Thread.join();
}

/*
* 関数名　:Init
* 引　数　:なし
* 戻り値　:なし
* 概　要　:ビルド設定の初期値を環境設定から読み込む
*/
void Builder::Init()
{
	const std::string out = SettingManager::GetInstance()->GetBuildOutputPath();
	strncpy_s(m_c_OutputDir, out.c_str(), _TRUNCATE);
}

/*
* 関数名　:SettingsMake
* 引　数　:out_settings：ビルド設定の出力先
* 戻り値　:なし
* 概　要　:ウィンドウの入力と環境設定からビルド設定を組み立てる
*/
void Builder::SettingsMake(BuildSettings& out_settings) const
{
	SettingManager* sm = SettingManager::GetInstance();
	out_settings.projectRoot = ".";
	out_settings.layout.assetsDir = sm->GetAssetsFilePath();
	out_settings.layout.sceneDir = sm->GetSceneFilePath();
	out_settings.layout.hlslDir = sm->GetShaderFilePath();
	out_settings.layout.csoDir = sm->GetCSOFilePath();
	out_settings.layout.spirvDir = sm->GetSPIRVFilePath();
	out_settings.layout.configDir = sm->GetConfigDirectoryPath();
	out_settings.layout.anim2dDir = sm->GetAnimator2DProjectFilePath();
	out_settings.layout.scriptDllDir = sm->GetDLLFilePath();
	out_settings.outputDir = m_c_OutputDir;
	ExePathGet(out_settings.runtimeDir, out_settings.runtimeExeName);

	out_settings.manifest = m_str_Manifest;
	out_settings.manifest.name = m_c_Name;
	out_settings.manifest.version = m_c_Version;
	auto scenes = SceneManger::GetInstance()->GetSceneList();
	if (m_i_StartScene >= 0 && m_i_StartScene < (int)scenes.size()) {
		const std::string& file = scenes[m_i_StartScene];
		out_settings.manifest.startScene = file.substr(0, file.find_last_of('.'));
	}
}

/*
* 関数名　:BuildStart
* 引　数　:なし
* 戻り値　:開始できたら true
* 概　要　:現在のシーンとシェーダーを最新にしてから、別スレッドでビルドを開始する
*/
bool Builder::BuildStart()
{
	if (m_b_Building.load()) return false;
	if (m_cls_Thread.joinable()) m_cls_Thread.join();

	// 編集中の内容をビルドに含めるため、メインスレッドで保存・コンパイルしておく
	SceneManger::GetInstance()->Save();
	ShaderManager::GetInstance()->UpdateAndCompileShaders();
	SettingManager::GetInstance()->SetBuildOutputPath(m_c_OutputDir);

	BuildSettings settings;
	SettingsMake(settings);
	m_b_Building = true;
	m_f_Progress = 0.0f;
	m_cls_Thread = std::thread([this, settings]() {
		GameBuilder builder;
		BuildReport report;
		builder.BuildRun(settings, report, [this](float p, const std::string& step) {
			m_f_Progress = p;
			std::lock_guard<std::mutex> lk(m_Mutex);
			m_Step = step;
			});
		{
			std::lock_guard<std::mutex> lk(m_Mutex);
			m_str_Report = std::move(report);
			m_b_HasReport = true;
		}
		m_b_Building = false;
		});
	return true;
}

/*
* 関数名　:WindowDraw
* 引　数　:p_open：ウィンドウの表示フラグ
* 戻り値　:なし
* 概　要　:ビルド設定・進捗・結果を表示する
*/
void Builder::WindowDraw(bool* p_open)
{
	if (!*p_open) return;
	ImGui::SetNextWindowSize(ImVec2(560, 520), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(U8("ゲームビルド").c_str(), p_open)) { ImGui::End(); return; }

	const bool busy = m_b_Building.load();
	ImGui::BeginDisabled(busy);
	ImGui::InputText(U8("ゲーム名").c_str(), m_c_Name, sizeof(m_c_Name));
	ImGui::InputText(U8("バージョン").c_str(), m_c_Version, sizeof(m_c_Version));

	auto scenes = SceneManger::GetInstance()->GetSceneList();
	m_i_StartScene = std::clamp(m_i_StartScene, 0, std::max(0, (int)scenes.size() - 1));
	const char* preview = scenes.empty() ? "-" : scenes[m_i_StartScene].c_str();
	if (ImGui::BeginCombo(U8("開始シーン").c_str(), preview)) {
		for (int i = 0; i < (int)scenes.size(); ++i)
			if (ImGui::Selectable(scenes[i].c_str(), i == m_i_StartScene)) m_i_StartScene = i;
		ImGui::EndCombo();
	}

	const char* backends[] = { "DirectX 11", "DirectX 12", "Vulkan" };
	int backend = (int)m_str_Manifest.backend;
	if (ImGui::Combo(U8("描画API").c_str(), &backend, backends, IM_ARRAYSIZE(backends)))
		m_str_Manifest.backend = (GraphicsBackend)backend;
	ImGui::InputInt(U8("幅").c_str(), &m_str_Manifest.i_Width);
	ImGui::InputInt(U8("高さ").c_str(), &m_str_Manifest.i_Height);
	ImGui::Checkbox(U8("フルスクリーン").c_str(), &m_str_Manifest.b_Fullscreen);
	ImGui::SameLine();
	ImGui::Checkbox("VSync", &m_str_Manifest.b_VSync);
	ImGui::InputText(U8("出力先").c_str(), m_c_OutputDir, sizeof(m_c_OutputDir));
	ImGui::EndDisabled();

	ImGui::Separator();
	if (busy) {
		std::lock_guard<std::mutex> lk(m_Mutex);
		ImGui::ProgressBar(m_f_Progress.load(), ImVec2(-1, 0), U8(m_Step).c_str());
	}
	else {
		if (ImGui::Button(U8("ビルド").c_str(), ImVec2(120, 0))) BuildStart();
		ImGui::SameLine();
		if (ImGui::Button(U8("出力先を開く").c_str(), ImVec2(120, 0))) File::OpenExplorer(m_c_OutputDir);
	}
	ReportDraw();
	ImGui::End();
}

/*
* 関数名　:ReportDraw
* 引　数　:なし
* 戻り値　:なし
* 概　要　:直近のビルド結果をログ付きで表示する
*/
void Builder::ReportDraw()
{
	std::lock_guard<std::mutex> lk(m_Mutex);
	if (!m_b_HasReport) return;
	const auto& r = m_str_Report;
	ImGui::TextColored(r.b_Success ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
		"%s", U8(r.b_Success ? "ビルド成功" : "ビルド失敗").c_str());
	ImGui::SameLine();
	ImGui::Text(" %.1fs / %zu files / %.1f MB / warn %zu / error %zu", r.f_Seconds, r.i_ArchiveFiles,
		r.i_ArchiveBytes / (1024.0 * 1024.0), r.CountGet(BuildReport::Level::Warning), r.CountGet(BuildReport::Level::Error));

	ImGui::BeginChild("BuildLog", ImVec2(0, 0), true);
	for (auto& m : r.messages) {
		const ImVec4 color = (m.e_Level == BuildReport::Level::Error) ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
			: (m.e_Level == BuildReport::Level::Warning) ? ImVec4(1.0f, 0.85f, 0.3f, 1.0f) : ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::TextWrapped("%s", U8(m.text).c_str());
		ImGui::PopStyleColor();
	}
	ImGui::EndChild();
}
