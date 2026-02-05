#include "GUI.h"
#include "System.h"
#include "MainFrame.h"
#include "File.h"
#include "Input.h"

#include "AssetManager.h"
#include "ShaderManager.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "SoundManager.h"
#include "SettingManager.h"
#include "EffectManager.h"
#include "IZANAGI.h"

#include "SceneManger.h"
#include "Scene.h"

GUI* GUI::instance = nullptr;

GUI* GUI::GetInstance()
{
	if (instance == nullptr)
	{
		instance = new GUI();
	}
	return instance;
}

void GUI::DestroyInstance()
{
	if (instance != nullptr)
	{
		delete instance;
		instance = nullptr;
	}
}

void GUI::Init()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	auto& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	io.IniFilename = nullptr;

	// 【重要】フォント設定を明示的にUTF-8対応にする
	ImFontConfig config;
	config.OversampleH = 2;
	config.OversampleV = 1;
	config.PixelSnapH = true;

	// 日本語フォントをUTF-8として読み込む
	io.Fonts->AddFontFromFileTTF(
		"C:/Windows/Fonts/meiryo.ttc",
		18.0f,
		&config,
		io.Fonts->GetGlyphRangesJapanese()
	);

	// フォントビルド
	io.Fonts->Build();

	ImGui_ImplWin32_Init(MainFrame::GetInstance()->GetWindowHandle());
	ImGui_ImplDX11_Init(DirectX11::GetInstance()->GetDevice(), DirectX11::GetInstance()->GetContext());

	// 以降のスタイル設定は同じ...
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	ImVec4 flatTabColor = ImVec4(0.18f, 0.20f, 0.23f, 1.00f);
	ImVec4 flatTabActive = ImVec4(0.20f, 0.22f, 0.25f, 1.00f);
	ImVec4 flatTabHovered = ImVec4(0.23f, 0.25f, 0.28f, 1.00f);

	colors[ImGuiCol_Tab] = flatTabColor;
	colors[ImGuiCol_TabUnfocused] = flatTabColor;
	colors[ImGuiCol_TabUnfocusedActive] = flatTabColor;
	colors[ImGuiCol_TabActive] = flatTabColor;
	colors[ImGuiCol_TabHovered] = flatTabColor;
	colors[ImGuiCol_TabActive] = flatTabActive;
	colors[ImGuiCol_TabHovered] = flatTabHovered;

	colors[ImGuiCol_Border] = flatTabColor;
	colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

	style.TabRounding = 0.0f;
	style.FrameRounding = 0.0f;
	style.WindowRounding = 0.0f;
	style.TabBorderSize = 0.0f;
	style.FramePadding = ImVec2(12, 4);
	style.ItemSpacing = ImVec2(6, 2);

	colors[ImGuiCol_Text] = ImVec4(0.88f, 0.90f, 0.94f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.48f, 0.54f, 1.00f);

	bSceneCreateWindow_ = false;
	bShaderListWindow_ = false;
	bAssetsManagerWindow_ = false;
	bTextureManagerWindow_ = false;
	bModelManagerWindow_ = false;
	bSoundManagerWindow_ = false;
	bSettingWindow_ = false;
}

void GUI::BeginDraw()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	MainMenuBar();
	SceneCreateWindow();
	ShaderListWindow();
	AssetsManagerWindow();
	TextureManagerWindow();
	ModelManagerWindow();
	SoundManagerWindow();
	SettingWindow();
	InputDebugWindow();
	ShaderEditorWindow();
	EffectManagerWindow();
	AISettingWindow();
}

void GUI::EndDraw()
{
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void GUI::MainMenuBar()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags window_flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_MenuBar;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::Begin("MainDock", nullptr, window_flags);
	// メニューバー
	if (ImGui::BeginMenuBar()) {
		const char* guiModes[] = { "Editor","Animator2D" };
		int guiModeWidth = 160;

		float curX = ImGui::GetCursorPosX();
		float targetX = curX - (float)guiModeWidth - 10.0f;
		if (targetX > curX) ImGui::SetCursorPosX(targetX);

		int GuiMode = (int)MainFrame::GetInstance()->GetSoftwareMode();

		ImGui::PushItemWidth((float)guiModeWidth);
		if (ImGui::Combo("##GuiModeCombo", &GuiMode, guiModes, IM_ARRAYSIZE(guiModes))) {
			MainFrame::GetInstance()->SetSoftwareMode((SoftWareMode)GuiMode);
		}

		ImGui::PopItemWidth();
		if (ImGui::BeginMenu(ShiftJISToUTF8("ファイル").c_str()))
		{
			if (ImGui::MenuItem(ShiftJISToUTF8("シーン作成").c_str())) bSceneCreateWindow_ = true;
			ImGui::Separator();

			if (ImGui::MenuItem(ShiftJISToUTF8("シェーダーリスト").c_str())) bShaderListWindow_ = true;
			if (ImGui::MenuItem(ShiftJISToUTF8("アセットマネージャー").c_str())) bAssetsManagerWindow_ = true;
			if (ImGui::MenuItem(ShiftJISToUTF8("テクスチャマネージャー").c_str())) bTextureManagerWindow_ = true;
			if (ImGui::MenuItem(ShiftJISToUTF8("モデルマネージャー").c_str())) bModelManagerWindow_ = true;
			if (ImGui::MenuItem(ShiftJISToUTF8("サウンドマネージャー").c_str())) bSoundManagerWindow_ = true;
			if (ImGui::MenuItem(ShiftJISToUTF8("エフェクトマネージャー").c_str())) bEffectManagerWindow_ = true;
			ImGui::Separator();

			if (ImGui::MenuItem(ShiftJISToUTF8("環境設定").c_str())) bSettingWindow_ = true;
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(ShiftJISToUTF8("編集").c_str()))
		{
			if (ImGui::MenuItem(ShiftJISToUTF8("入力デバック").c_str())) bInputDebugWindow_ = true;
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(ShiftJISToUTF8("ツール").c_str()))
		{
			if (ImGui::MenuItem(ShiftJISToUTF8("シェーダーエディタ").c_str())) bShaderEditorWindow_ = true;
			if (ImGui::MenuItem(ShiftJISToUTF8("AI設定ウィンドウ").c_str())) bAISettingWindow_ = true;
			if (ImGui::MenuItem(ShiftJISToUTF8("フォルダ").c_str())) {
				std::string Path = File::GetExePath();
				Path = File::RemoveExeFromPath(Path);
				File::OpenExplorer(Path);
			}
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

void GUI::SceneCreateWindow()
{
	if (!bSceneCreateWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
	if (ImGui::Begin(ShiftJISToUTF8("シーン作成##SceneManager_CreateScene").c_str(), &bSceneCreateWindow_, flags)) {
		static char sceneName[128] = "NewScene";
		ImGui::InputText(ShiftJISToUTF8("シーン名").c_str(), sceneName, sizeof(sceneName));
		ImGui::Separator();
		ImGui::Text(ShiftJISToUTF8("説明").c_str());
		ImGui::TextWrapped(ShiftJISToUTF8("新しいシーンを作成します。シーン名を入力して作成ボタンを押してください。").c_str());
		ImGui::Separator();
		// シーン作成ボタン
		if (ImGui::Button(ShiftJISToUTF8("作成").c_str(), ImVec2(120, 0))) {
			bool OK = SceneManger::GetInstance()->CreateAndRegisterScene(sceneName);
			if (OK) {
				// 作成成功
				MessageBoxA(NULL, "シーンの作成に成功しました。", "成功", MB_OK | MB_ICONINFORMATION);
				// 名前の初期化
				strcpy_s(sceneName, "NewScene");
			}
			else {
				MessageBoxA(NULL, "シーンの作成に失敗しました。", "失敗", MB_OK | MB_ICONERROR);
			}
		}
		// シーンチェンジ
		ImGui::Separator();
		ImGui::Text(ShiftJISToUTF8("現在のシーン:").c_str());
		ImGui::SameLine();
		AbstractScene* current = SceneManger::GetInstance()->GetCurrentScene();
		if (current) {
			std::string currentScene = SceneManger::GetInstance()->GetCurrentScene()->GetName();
			ImGui::Text(ShiftJISToUTF8(("現在のシーン: " + currentScene).c_str()).c_str());
		}

		ImGui::Separator();
		static char changeSceneName[128] = "";
		ImGui::InputText(ShiftJISToUTF8("シーン名##ChangeScene").c_str(), changeSceneName, sizeof(changeSceneName));
		if (ImGui::Button(ShiftJISToUTF8("シーン切り替え").c_str(), ImVec2(120, 0)))SceneManger::GetInstance()->ChangeScene(changeSceneName);

		ImGui::Separator();
		// シーン一覧
		ImGui::Text(ShiftJISToUTF8("シーン一覧").c_str());
		auto sceneList = SceneManger::GetInstance()->GetSceneList();
		ImGui::BeginChild("SceneListChild###EditerGUI___", ImVec2(0, 0), true);
		for (const auto& AbstractScene : sceneList) { ImGui::Text(ShiftJISToUTF8(AbstractScene).c_str()); };
		ImGui::EndChild();
	}
	ImGui::End();
}

void GUI::ShaderListWindow()
{
	if (!bShaderListWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("シェーダーリスト").c_str(), &bShaderListWindow_, flags)) {
		static int shaderListType = 0;
		const char* shaderListTypes[] = { "Vertex Shader", "Pixel Shader" };
		ImGui::Combo(ShiftJISToUTF8("シェーダーリスト").c_str(), &shaderListType, shaderListTypes, IM_ARRAYSIZE(shaderListTypes));
		ImGui::Separator();
		std::vector<std::string> ShaderList;
		ShaderList.clear();
		if (shaderListType == 0)
			ShaderList = ShaderManager::GetInstance()->GetShaderList("VS");
		else
			ShaderList = ShaderManager::GetInstance()->GetShaderList("PS");
		for (const auto& shaderName : ShaderList) {
			ImGui::Text(ShiftJISToUTF8(shaderName).c_str());
		}
		ImGui::End();
	}
}

void GUI::AssetsManagerWindow()
{
	if (!bAssetsManagerWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("アセットマネージャー").c_str(), &bAssetsManagerWindow_, flags)) {
		AssetManager::Instance()->DrawDebugGUI();
		ImGui::End();
	}
}

void GUI::TextureManagerWindow()
{
	if (!bTextureManagerWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("テクスチャマネージャー").c_str(), &bTextureManagerWindow_, flags)) {
		TextureManager::Instance()->DrawDebugGUI();
		ImGui::End();
	}
}

void GUI::ModelManagerWindow()
{
	if (!bModelManagerWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("モデルマネージャー").c_str(), &bModelManagerWindow_, flags)) {
		ModelManager::Instance()->DrawDebugGUI();
		ImGui::End();
	}
}

void GUI::SoundManagerWindow()
{
	if (!bSoundManagerWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("サウンドマネージャー").c_str(), &bSoundManagerWindow_, flags)) {
		SoundManager::Instance()->DrawDebugGUI();
		ImGui::End();
	}
}

void GUI::EffectManagerWindow()
{
	if (!bEffectManagerWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("エフェクトマネージャー").c_str(), &bEffectManagerWindow_, flags)) {
		EffectManager::Instance()->DrawDebugGUI();
		ImGui::End();
	}
}

void GUI::SettingWindow()
{
	if (!bSettingWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("環境設定").c_str(), &bSettingWindow_, flags)) {
		ImGui::Text(ShiftJISToUTF8("パスの設定").c_str());
		ImGui::Separator();
		std::string assetsPath = SettingManager::GetInstance()->GetAssetsFilePath();
		char assetsBuffer[256];
		strncpy_s(assetsBuffer, assetsPath.c_str(), sizeof(assetsBuffer));
		if (ImGui::InputText(ShiftJISToUTF8("アセットフォルダ").c_str(), assetsBuffer, sizeof(assetsBuffer)))SettingManager::GetInstance()->SetAssetsFilePath(assetsBuffer);
		ImGui::Text(ShiftJISToUTF8("* アセットフォルダパスを変えた場合再起動してください").c_str());
		std::string archivePath = SettingManager::GetInstance()->GetArchiveFilePath();
		char archiveBuffer[256];
		strncpy_s(archiveBuffer, archivePath.c_str(), sizeof(archiveBuffer));
		if (ImGui::InputText(ShiftJISToUTF8("アーカイブフォルダ").c_str(), archiveBuffer, sizeof(archiveBuffer)))SettingManager::GetInstance()->SetArchiveFilePath(archiveBuffer);
		std::string scenePath = SettingManager::GetInstance()->GetSceneFilePath();
		char sceneBuffer[256];
		strncpy_s(sceneBuffer, scenePath.c_str(), sizeof(sceneBuffer));
		if (ImGui::InputText(ShiftJISToUTF8("シーンフォルダ").c_str(), sceneBuffer, sizeof(sceneBuffer)))SettingManager::GetInstance()->SetSceneFilePath(sceneBuffer);
		std::string shaderPath = SettingManager::GetInstance()->GetShaderFilePath();
		char shaderBuffer[256];
		strncpy_s(shaderBuffer, shaderPath.c_str(), sizeof(shaderBuffer));
		if (ImGui::InputText(ShiftJISToUTF8("シェーダーフォルダ").c_str(), shaderBuffer, sizeof(shaderBuffer)))SettingManager::GetInstance()->SetShaderFilePath(shaderBuffer);
		std::string csoPath = SettingManager::GetInstance()->GetCSOFilePath();
		char csoBuffer[256];
		strncpy_s(csoBuffer, csoPath.c_str(), sizeof(csoBuffer));
		if (ImGui::InputText(ShiftJISToUTF8("CSOフォルダ").c_str(), csoBuffer, sizeof(csoBuffer)))SettingManager::GetInstance()->SetCSOFilePath(csoBuffer);
		std::string PackingTool = SettingManager::GetInstance()->GetPackingToolFilePath();
		char PackingToolBuffer[256];
		strncpy_s(PackingToolBuffer, PackingTool.c_str(), sizeof(PackingToolBuffer));
		if (ImGui::InputText(ShiftJISToUTF8("PackingTool").c_str(), PackingToolBuffer, sizeof(PackingToolBuffer)))SettingManager::GetInstance()->SetPackingToolFilePath(PackingToolBuffer);

		std::string VcpkgPath = SettingManager::GetInstance()->GetVcpkgFilePath();
		char VcpkgBuffer[256];
		strncpy_s(VcpkgBuffer, VcpkgPath.c_str(), sizeof(VcpkgBuffer));
		if (ImGui::InputText(ShiftJISToUTF8("Vcpkgフォルダ").c_str(), VcpkgBuffer, sizeof(VcpkgBuffer)))SettingManager::GetInstance()->SetVcpkgFilePath(VcpkgBuffer);

		char ExternelToolBuffer[256];
		std::string ExternelTool = SettingManager::GetInstance()->GetExternelToolPath();
		strncpy_s(ExternelToolBuffer, ExternelTool.c_str(), sizeof(ExternelToolBuffer));
		if (ImGui::InputText(ShiftJISToUTF8("外部ツールフォルダ").c_str(), ExternelToolBuffer, sizeof(ExternelToolBuffer)))SettingManager::GetInstance()->SetExternelToolPath(ExternelToolBuffer);

		char ScriptDllBuffer[256];
		std::string ScriptDllPath = SettingManager::GetInstance()->GetDLLFilePath();
		strncpy_s(ScriptDllBuffer, ScriptDllPath.c_str(), sizeof(ScriptDllBuffer));
		if (ImGui::InputText(ShiftJISToUTF8("スクリプトDLLフォルダ").c_str(), ScriptDllBuffer, sizeof(ScriptDllBuffer)))SettingManager::GetInstance()->SetDLLFilePath(ScriptDllBuffer);

		char ScriptSourceBuffer[256];
		std::string ScriptSourcePath = SettingManager::GetInstance()->GetScriptFilePath();
		strncpy_s(ScriptSourceBuffer, ScriptSourcePath.c_str(), sizeof(ScriptSourceBuffer));
		if (ImGui::InputText(ShiftJISToUTF8("スクリプトソースフォルダ").c_str(), ScriptSourceBuffer, sizeof(ScriptSourceBuffer)))SettingManager::GetInstance()->SetScriptFilePath(ScriptSourceBuffer);

		ImGui::Text(ShiftJISToUTF8("レンダリング設定").c_str());
		ImGui::Separator();
		bool bZBuffer = SettingManager::GetInstance()->GetZBuffer();
		if (ImGui::Checkbox(ShiftJISToUTF8("Zバッファを有効にする").c_str(), &bZBuffer))SettingManager::GetInstance()->SetZBuffer(bZBuffer);

		int autoSaveInterval = SettingManager::GetInstance()->GetAutoSaveInterval();
		if (ImGui::InputInt(ShiftJISToUTF8("自動保存間隔（分）").c_str(), &autoSaveInterval)) {
			if (autoSaveInterval < 1) autoSaveInterval = 1;
			SettingManager::GetInstance()->SetAutoSaveInterval(autoSaveInterval);
		}

		ImGui::Text(ShiftJISToUTF8("バックグラウンドカラーを変更").c_str());
		DirectX::XMFLOAT4 Color;
		Color = SettingManager::GetInstance()->GetBackgroundColor();
		float color[4] = { Color.x, Color.y, Color.z, Color.w };
		if (ImGui::ColorEdit4(ShiftJISToUTF8("背景色").c_str(), color)) {
			SettingManager::GetInstance()->SetBackgroundColor(DirectX::XMFLOAT4(color[0], color[1], color[2], color[3]));
		}

		ImGui::Text(ShiftJISToUTF8("ピクセルポストエフェクト設定").c_str());
		ImGui::Separator();
		bool bPixelPostEffect = MainFrame::GetInstance()->isPixelated();
		ImGui::Checkbox(ShiftJISToUTF8("ピクセルポストエフェクトを有効にする").c_str(), &bPixelPostEffect);
		MainFrame::GetInstance()->setPixelated(bPixelPostEffect);

		ImGui::Text(ShiftJISToUTF8("マウス感度設定").c_str());
		float mouseSensitivity = SettingManager::GetInstance()->GetMouseSensitivity();
		if (ImGui::SliderFloat(ShiftJISToUTF8("マウス感度:").c_str(), &mouseSensitivity, 0.01f, 1.0f))SettingManager::GetInstance()->SetMouseSensitivity(mouseSensitivity);
	}
	ImGui::End();
}

void GUI::InputDebugWindow()
{
	if (!bInputDebugWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("入力デバック").c_str(), &bInputDebugWindow_, flags))
	{
		ImGui::Text(ShiftJISToUTF8("マウスの移動量").c_str());
		ImGui::Text(ShiftJISToUTF8("X:").c_str());
		ImGui::SameLine();
		int X;
		// 座標を取得
		X = MouseMoveX();
		// 文字列に変換して表示
		ImGui::Text(std::to_string(X).c_str());
		ImGui::Text(ShiftJISToUTF8("Y:").c_str());
		ImGui::SameLine();
		int Y;
		// 座標を取得
		Y = MouseMoveY();
		// 文字列に変換して表示
		ImGui::Text(std::to_string(Y).c_str());

		// マウスのホイール量
		int Wheel;
		Wheel = (int)MouseWheel();
		ImGui::Text(ShiftJISToUTF8("ホイール:").c_str());
		ImGui::SameLine();
		ImGui::Text(std::to_string(Wheel).c_str());

		int A;
		A = (int)MouseWheelForward();
		ImGui::Text(ShiftJISToUTF8("ホイール前方向ノッチ数:").c_str());
		ImGui::SameLine();
		ImGui::Text(std::to_string(A).c_str());

		int B;
		B = (int)MouseWheelBackward();
		ImGui::Text(ShiftJISToUTF8("ホイール後方向ノッチ数:").c_str());
		ImGui::SameLine();
		ImGui::Text(std::to_string(B).c_str());

		ImGui::Separator();
		ImGui::Text(ShiftJISToUTF8("キーボードの入力状態").c_str());
		for (int i = 0; i < 256; i++) {
			if (IsKeyPress(i)) {
				ImGui::Text(ShiftJISToUTF8(("キーコード " + std::to_string(i) + " が押されています").c_str()).c_str());
			}
		}
		ImGui::End();
	}
}

void GUI::ShaderEditorWindow()
{
	if (!bShaderEditorWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("シェーダー作成").c_str(), &bShaderEditorWindow_, flags)) {
		ImGui::Text(ShiftJISToUTF8("シェーダーエディターウインドウです").c_str());
		ImGui::Text(ShiftJISToUTF8("シェーダーファイルフォルダ:").c_str());
		ImGui::SameLine();
		ImGui::Text(ShiftJISToUTF8(SettingManager::GetInstance()->GetShaderFilePath()).c_str());
		ImGui::Separator();
		ImGui::Text(ShiftJISToUTF8("説明").c_str());
		ImGui::TextWrapped(ShiftJISToUTF8("シェーダーファイルを編集した後、保存すると自動的にコンパイルされます。").c_str());
		ImGui::TextWrapped(ShiftJISToUTF8("コンパイルエラーが発生した場合、メッセージボックスで通知されます。").c_str());
		static char shaderName[128] = "";
		ImGui::InputText(ShiftJISToUTF8("シェーダー名").c_str(), shaderName, sizeof(shaderName));
		static int shaderType = 0;
		const char* shaderTypes[] = { "Vertex Shader", "Pixel Shader" };
		ImGui::Combo(ShiftJISToUTF8("シェーダータイプ").c_str(), &shaderType, shaderTypes, IM_ARRAYSIZE(shaderTypes));
		if (ImGui::Button(ShiftJISToUTF8("新規シェーダーファイル作成").c_str(), ImVec2(180, 0))) {
			if (strlen(shaderName) == 0) {
				MessageBoxA(NULL, "シェーダー名を入力してください。", "エラー", MB_OK | MB_ICONERROR);
			}
			else {
				std::string name = shaderName;
				if (shaderType == 0) {
					name = "VS_" + name;
					if (ShaderManager::GetInstance()->CreateHLSLTemplate(name, "VS")) {
						MessageBoxA(NULL, "頂点シェーダーのテンプレートを作成しました。", "成功", MB_OK | MB_ICONINFORMATION);
					}
					else {
						MessageBoxA(NULL, "シェーダーファイルの作成に失敗しました。", "エラー", MB_OK | MB_ICONERROR);
					}
				}
				else {
					name = "PS_" + name;
					if (ShaderManager::GetInstance()->CreateHLSLTemplate(name, "PS")) {
						MessageBoxA(NULL, "ピクセルシェーダーのテンプレートを作成しました。", "成功", MB_OK | MB_ICONINFORMATION);
					}
					else {
						MessageBoxA(NULL, "シェーダーファイルの作成に失敗しました。", "エラー", MB_OK | MB_ICONERROR);
					}
				}
			}
		}
		ImGui::End();
	}
}

void GUI::AISettingWindow()
{
	if (!bAISettingWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("AI設定").c_str(), &bAISettingWindow_, flags))
	{
		static char persona[4096] = "";
		ImGui::Text(ShiftJISToUTF8("性格の設定").c_str());
		ImGui::InputTextMultiline(ShiftJISToUTF8("##PersonaInput").c_str(), (char*)persona, sizeof(persona), ImVec2(-1.0f, -1.0f));
		if(ImGui::Button(ShiftJISToUTF8("適用").c_str(), ImVec2(120, 0)))
		{
			IZANAGI::GetInstance()->SetPersona(std::string(persona));
		}
		ImGui::End();
	}
}

std::string GUI::ShiftJISToUTF8(const std::string& str)
{
	if (str.empty()) return {};

	// Shift-JIS → UTF-16
	int lenW = MultiByteToWideChar(932, 0, str.data(), (int)str.size(), nullptr, 0);
	if (lenW <= 0) return {};

	std::wstring wstr(lenW, 0);
	MultiByteToWideChar(932, 0, str.data(), (int)str.size(), &wstr[0], lenW);

	// UTF-16 → UTF-8
	int lenU8 = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), lenW, nullptr, 0, nullptr, nullptr);
	if (lenU8 <= 0) return {};

	std::string u8str(lenU8, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.data(), lenW, &u8str[0], lenU8, nullptr, nullptr);

	return u8str;
}

ID3D11ShaderResourceView* GUI::LoadImg(const std::wstring& filename, ID3D11Device* device)
{
	IWICImagingFactory* factory = nullptr;
	IWICBitmapDecoder* decoder = nullptr;
	IWICBitmapFrameDecode* frame = nullptr;
	IWICFormatConverter* converter = nullptr;
	ID3D11ShaderResourceView* srv = nullptr;

	// COM は Main::Init() で既に初期化されているため、ここでは初期化不要
	HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
	if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromFilename(filename.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
	if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
	if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
	if (SUCCEEDED(hr)) hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

	UINT width = 0, height = 0;
	if (SUCCEEDED(hr)) hr = frame->GetSize(&width, &height);

	std::vector<BYTE> buffer;
	if (SUCCEEDED(hr)) {
		buffer.resize(width * height * 4);
		hr = converter->CopyPixels(nullptr, width * 4, (UINT)buffer.size(), buffer.data());
	}
	if (SUCCEEDED(hr)) {
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		D3D11_SUBRESOURCE_DATA initData = { buffer.data(), width * 4, 0 };
		ID3D11Texture2D* tex = nullptr;
		hr = device->CreateTexture2D(&desc, &initData, &tex);
		if (SUCCEEDED(hr)) hr = device->CreateShaderResourceView(tex, nullptr, &srv);
		if (tex) tex->Release();
	}

	if (converter) converter->Release();
	if (frame) frame->Release();
	if (decoder) decoder->Release();
	if (factory) factory->Release();
	// COM は Main::UnInit() で終了処理されるため、ここでは終了処理不要

	return srv;
}