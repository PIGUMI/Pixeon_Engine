// EditrGUI.cpp
#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "System.h"
#include "File.h"
#include "EngineManager.h"
#include "StartUp.h"
#include "SettingManager.h"
#include "ShaderManager.h"
#include "AssetManager.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "SoundManager.h"
#include "SceneManger.h"
#include "Scene.h"
#include "Input.h"

#pragma comment(lib, "windowscodecs.lib")

EditrGUI* EditrGUI::instance = nullptr;

EditrGUI* EditrGUI::GetInstance()
{
	if (instance == nullptr) {
		instance = new EditrGUI();
	}
	return instance;
}

void EditrGUI::DestroyInstance() {
	if (instance) {
		//instance->WriteLogBuffer();
		delete instance;
		instance = nullptr;
	}
}

void EditrGUI::Init() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	auto& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	io.IniFilename = nullptr;

	io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/meiryo.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
	ImGui_ImplWin32_Init(EngineManager::GetInstance()->GetWindowHandle());
	ImGui_ImplDX11_Init(DirectX11::GetInstance()->GetDevice(), DirectX11::GetInstance()->GetContext());

	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	// タブを完全フラット＆ボーダーレスに
	ImVec4 flatTabColor = ImVec4(0.18f, 0.20f, 0.23f, 1.00f); // 基本のタブ背景色
	ImVec4 flatTabActive = ImVec4(0.20f, 0.22f, 0.25f, 1.00f); // アクティブタブ（ほんのり違い）
	ImVec4 flatTabHovered = ImVec4(0.23f, 0.25f, 0.28f, 1.00f); // ホバー（やや明るく）

	colors[ImGuiCol_Tab] = flatTabColor;
	colors[ImGuiCol_TabUnfocused] = flatTabColor;
	colors[ImGuiCol_TabUnfocusedActive] = flatTabColor;
	colors[ImGuiCol_TabActive] = flatTabColor; // アクティブ時も同じ色に
	colors[ImGuiCol_TabHovered] = flatTabColor; // ホバー時も同じ色に
	colors[ImGuiCol_TabActive] = flatTabActive;
	colors[ImGuiCol_TabHovered] = flatTabHovered;

	// タブとタブのボーダーを完全に消す
	colors[ImGuiCol_Border] = flatTabColor;
	colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

	// タブの角丸を無しに
	style.TabRounding = 0.0f;
	style.FrameRounding = 0.0f;
	style.WindowRounding = 0.0f;

	style.TabBorderSize = 0.0f;

	// タブの内側余白を抑えめにして高さも下げる
	style.FramePadding = ImVec2(12, 4);
	style.ItemSpacing = ImVec2(6, 2);

	// タブのテキスト色（アクティブのみ白寄り・非アクティブはグレー寄りで差をつける）
	colors[ImGuiCol_Text] = ImVec4(0.88f, 0.90f, 0.94f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.48f, 0.54f, 1.00f);
	LogBuffer = "";

	img = LoadImg(L"SceneRoot/Editor/texture/img.png", DirectX11::GetInstance()->GetDevice());
	Sound = LoadImg(L"SceneRoot/Editor/texture/Sound.png", DirectX11::GetInstance()->GetDevice());
	fbx = LoadImg(L"SceneRoot/Editor/texture/fbx.png", DirectX11::GetInstance()->GetDevice());
	sceneIcon = LoadImg(L"SceneRoot/Editor/texture/Scene.png", DirectX11::GetInstance()->GetDevice());
	folderIcon = LoadImg(L"SceneRoot/Editor/texture/File.png", DirectX11::GetInstance()->GetDevice());
	shaderIcon = LoadImg(L"SceneRoot/Editor/texture/HLSL.png", DirectX11::GetInstance()->GetDevice());
	scriptIcon = LoadImg(L"SceneRoot/Editor/texture/Script.png", DirectX11::GetInstance()->GetDevice());
	JsonIcon = LoadImg(L"SceneRoot/Editor/texture/Json.png", DirectX11::GetInstance()->GetDevice());
	archiveIcon = LoadImg(L"SceneRoot/Editor/texture/Archive.png", DirectX11::GetInstance()->GetDevice());
	ExeIcon = LoadImg(L"SceneRoot/Editor/texture/Exe.png", DirectX11::GetInstance()->GetDevice());
}

void EditrGUI::Update() {
}

void EditrGUI::Draw()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	WindowGUI();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

std::string EditrGUI::ShiftJISToUTF8(const std::string& str)
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

void EditrGUI::WindowGUI()
{
	static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
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
	ImGui::Begin("MainDockspace", nullptr, window_flags);

	// メニューバー
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu(ShiftJISToUTF8("ファイル").c_str()))
		{
			ImGui::MenuItem(ShiftJISToUTF8("新規シーン").c_str());
			ImGui::MenuItem(ShiftJISToUTF8("開く...").c_str());
			ImGui::MenuItem(ShiftJISToUTF8("保存").c_str());
			ImGui::Separator();
			if (ImGui::MenuItem(ShiftJISToUTF8("シーン作成").c_str())) ShowSceneCreate = true;
			ImGui::Separator();
			if (ImGui::MenuItem(ShiftJISToUTF8("シェーダーリスト").c_str())) ShowShaderListWindow = true;
			if (ImGui::MenuItem(ShiftJISToUTF8("アセットマネージャー").c_str())) ShowAssetManagerWindow = true;
			if (ImGui::MenuItem(ShiftJISToUTF8("テクスチャマネージャー").c_str())) ShowTextureManagerWindow = true;
			if (ImGui::MenuItem(ShiftJISToUTF8("モデルマネージャー").c_str())) ShowModelManagerWindow = true;
			if (ImGui::MenuItem(ShiftJISToUTF8("サウンドマネージャー").c_str())) ShowSoundManagerWindow = true;
			ImGui::Separator();
			if (ImGui::MenuItem(ShiftJISToUTF8("環境設定").c_str())) ShowSettingsWindow = true;
			ImGui::Separator();
			if (ImGui::MenuItem(ShiftJISToUTF8("ライセンス表示").c_str())) ShowLicense = true;
			ImGui::Separator();
			if (ImGui::MenuItem(ShiftJISToUTF8("終了").c_str())) SetRun(false);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(ShiftJISToUTF8("編集").c_str()))
		{
			ImGui::MenuItem(ShiftJISToUTF8("取り消し").c_str());
			ImGui::MenuItem(ShiftJISToUTF8("やり直し").c_str());
			ImGui::Separator();
			// レイアウト初期化
			if (ImGui::MenuItem(ShiftJISToUTF8("レイアウトを初期状態に戻す").c_str())) {
				ImGui::GetIO().IniFilename = nullptr;
				ImGui::LoadIniSettingsFromMemory("");
				dockNeedsReset = true;
			}
			if (ImGui::MenuItem(ShiftJISToUTF8("レイアウトを保存").c_str())) {
				ImGui::GetIO().IniFilename = "imgui_layout.ini";
				ImGui::SaveIniSettingsToDisk("imgui_layout.ini");
			}
			ImGui::Separator();
			if (ImGui::MenuItem(ShiftJISToUTF8("入力デバック").c_str())) ShowInPutDebug = true;
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(ShiftJISToUTF8("ツール").c_str()))
		{
			if (ImGui::MenuItem(ShiftJISToUTF8("アーカイブ化").c_str())) ShowArchiveWindow = true;
			if (ImGui::MenuItem(ShiftJISToUTF8("シェーダーエディタ").c_str())) ShowShaderEditorWindow = true;

			if (ImGui::MenuItem(ShiftJISToUTF8("フォルダ").c_str())) {
				std::string Path = File::GetExePath();
				Path = File::RemoveExeFromPath(Path);
				File::OpenExplorer(Path);
			}

			if (ImGui::MenuItem(ShiftJISToUTF8("外部ツール").c_str())) ShowExternalToolsWindow = true;

			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	// 環境設定ウィンドウ
	if (ShowSettingsWindow) SettingWindow();
	// ツールウインドウ
	if (ShowArchiveWindow)
	{
		ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
		if (ImGui::Begin(ShiftJISToUTF8("アーカイブ化").c_str(), &ShowArchiveWindow, flags)) {
			ImGui::Text(ShiftJISToUTF8("ツールパス:").c_str());
			ImGui::SameLine();
			ImGui::Text(ShiftJISToUTF8(SettingManager::GetInstance()->GetPackingToolFilePath()).c_str());
			ImGui::Text(ShiftJISToUTF8("アセットフォルダ:").c_str());
			ImGui::SameLine();
			ImGui::Text(ShiftJISToUTF8(SettingManager::GetInstance()->GetAssetsFilePath()).c_str());
			ImGui::Text(ShiftJISToUTF8("アーカイブ出力先:").c_str());
			ImGui::SameLine();
			ImGui::Text(ShiftJISToUTF8(SettingManager::GetInstance()->GetArchiveFilePath() + "/assets.PixAssets").c_str());
			ImGui::Separator();
			ImGui::Text(ShiftJISToUTF8("説明").c_str());
			ImGui::TextWrapped(ShiftJISToUTF8("アセットフォルダ内のファイルを一つのアーカイブファイルにまとめます。アーカイブ化には時間がかかる場合があります。").c_str());
			if (ImGui::Button(ShiftJISToUTF8("アーカイブ化実行").c_str(), ImVec2(120, 0))) {
				bool OK = false;
				OK = File::RunArchiveTool(SettingManager::GetInstance()->GetPackingToolFilePath(),
					SettingManager::GetInstance()->GetAssetsFilePath(),
					SettingManager::GetInstance()->GetArchiveFilePath() + "/assets.PixAssets");
				if (OK)
					MessageBoxA(NULL, "アーカイブ化に成功しました。", "成功", MB_OK | MB_ICONINFORMATION);
				else
					MessageBoxA(NULL, "アーカイブ化に失敗しました。", "失敗", MB_OK | MB_ICONERROR);
			}
		}
		ImGui::End();
	}

	ExternalToolsWindow();

	// DockSpaceを作成
	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

	// Dock初期化
	static bool dock_init = false;
	if (!dock_init || dockNeedsReset) {
		dock_init = true;
		dockNeedsReset = false;
		ImGui::DockBuilderRemoveNode(dockspace_id); // DockSpaceリセット
		ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

		ImGuiID dock_main_id = dockspace_id;
		ImGuiID dock_id_right;
		ImGuiID dock_id_bottom;
		ImGuiID dock_id_left;

		// 右にInspector
		ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, &dock_id_right, &dock_main_id);
		// 下にContentDrawer
		ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.4f, &dock_id_bottom, &dock_main_id);
		// 左にHierarchy
		ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.18f, &dock_id_left, &dock_main_id);

		ImGui::DockBuilderDockWindow(ShiftJISToUTF8("ゲームビュー").c_str(), dock_main_id);
		ImGui::DockBuilderDockWindow(ShiftJISToUTF8("インスペクター").c_str(), dock_id_right);
		ImGui::DockBuilderDockWindow(ShiftJISToUTF8("コンソール").c_str(), dock_id_bottom);
		ImGui::DockBuilderDockWindow(ShiftJISToUTF8("Prefab").c_str(), dock_id_bottom);
		ImGui::DockBuilderDockWindow(ShiftJISToUTF8("コンテンツドロワー").c_str(), dock_id_bottom);
		ImGui::DockBuilderDockWindow(ShiftJISToUTF8("ヒエラルキー").c_str(), dock_id_left);

		ImGui::DockBuilderFinish(dockspace_id);
	}

	ImGui::End();
	ImGui::PopStyleVar();

	// 各種ウィンドウ表示
	ShowGameView();
	if (EngineManager::GetInstance()->IsShowGUI())return;
	ShowContentDrawer();
	ShowConsole();
	ShowPrefab();
	ShaderEditorWindow();
	ShaderListWindow();
	ShowHierarchy();
	ShowInspector();
	AssetManagerWindow();
	TextureManagerWindow();
	ModelManagerWindow();
	SoundManagerWindow();
	ShowLicenseWindow();
	ShowSceneCreateWindow();
	ShowSceneRenameWindow();
	ShowInputDebug();
}

void EditrGUI::ShowGameView()
{
	ImGui::Begin(ShiftJISToUTF8("ゲームビュー").c_str());

	// --- 上部にコントロールバー ---
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

	bool isGamePlaying = EngineManager::GetInstance()->IsInGame();

	// 再生・停止ボタン
	if (!isGamePlaying) {
		if (ImGui::Button(ShiftJISToUTF8("再生").c_str(), ImVec2(70, 0)))
		{
			EngineManager::GetInstance()->SetInGame(true);
			EngineManager::GetInstance()->SetShowGUI(true);
		}
	}
	else {
		if (ImGui::Button(ShiftJISToUTF8("停止").c_str(), ImVec2(70, 0)))
		{
			EngineManager::GetInstance()->SetInGame(false);
			EngineManager::GetInstance()->SetShowGUI(false);
			SelectedObject = nullptr;
		}
	}

	ImGui::PopStyleVar(2);

	ImGui::Separator();

	// --- ゲーム画面（プレビュー） ---
	ID3D11ShaderResourceView* srv = EngineManager::GetInstance()->GetGameRender();
	ImVec2 size = ImGui::GetContentRegionAvail();
	// アスペクト比16:9に合わせる
	float aspect = 16.0f / 9.0f;
	if (size.x / size.y > aspect) {
		size.x = size.y * aspect;
	}
	else {
		size.y = size.x / aspect;
	}

	ImVec2 pos = ImGui::GetCursorPos();
	pos.x += (ImGui::GetContentRegionAvail().x - size.x) * 0.5f;
	ImGui::SetCursorPosX(pos.x);
	if (srv)
		ImGui::Image((ImTextureID)srv, size);
	else
		ImGui::Text("SRVがNullです");

	bool active = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	ImGui::End();

	if (active)
	{
		Scene* Temp = nullptr;
		Temp = SceneManger::GetInstance()->GetCurrentScene();
		if (Temp)
		{
			CameraComponent* Cam = nullptr;
			Cam = Temp->GetMainCamera();
			if (Cam)
			{
				// 右方向ベクトル
				DirectX::XMFLOAT3 right = Cam->GetRightVector();
				// 前方向ベクトル
				DirectX::XMFLOAT3 forward = Cam->GetForwardVector();

				float scrollSpeed = 2.0f;
				float forwardNotches = MouseWheelForward();
				float backwardNotches = MouseWheelBackward();

				DirectX::XMFLOAT3 pos;
				if (Cam->IsChangeCalculation()) {
					pos = Cam->GetPosition();
				}
				else {
					pos = Cam->GetFixation();
				}
				// 前方向に移動
				if (forwardNotches > 0.0f) {
					pos.x -= forward.x * forwardNotches * scrollSpeed;
					pos.y -= forward.y * forwardNotches * scrollSpeed;
					pos.z -= forward.z * forwardNotches * scrollSpeed;
				}
				// 後方向に移動
				if (backwardNotches > 0.0f) {
					pos.x += forward.x * backwardNotches * scrollSpeed;
					pos.y += forward.y * backwardNotches * scrollSpeed;
					pos.z += forward.z * backwardNotches * scrollSpeed;
				}

				if (IsKeyPress(1))
				{
					float MoveX = (float)MouseMoveX() * SettingManager::GetInstance()->GetMouseSensitivity();
					float MoveY = (float)MouseMoveY() * SettingManager::GetInstance()->GetMouseSensitivity();

					DirectX::XMFLOAT3 up = Cam->GetUpVector();

					// 右方向×MoveX ＋ Up方向×MoveY
					pos.x += right.x * MoveX + up.x * MoveY;
					pos.y += right.y * MoveX + up.y * MoveY;
					pos.z += right.z * MoveX + up.z * MoveY;
				}
				if (IsKeyPress(2))
				{
					float MoveX = (float)MouseMoveX();
					float MoveY = (float)MouseMoveY();
					MoveX = MoveX * SettingManager::GetInstance()->GetMouseSensitivity();
					MoveY = MoveY * SettingManager::GetInstance()->GetMouseSensitivity();
					DirectX::XMFLOAT3 Rot;
					Rot = Cam->GetRotation();
					Rot.x += MoveX;
					Rot.y += MoveY;
					Cam->SetRotation(Rot);
				}
				if (Cam->IsChangeCalculation()) {
					Cam->SetPosition(pos);
				}
				else {
					Cam->SetFixation(pos);
				}
			}
		}
	}
}

void EditrGUI::ShowConsole() {
	ImGui::Begin(ShiftJISToUTF8("コンソール").c_str());
	ImGui::BeginChild("ConsoleChild###EditerGUI___", ImVec2(0, 0), true);
	ImGui::Text(ShiftJISToUTF8(LogBuffer).c_str());
	ImGui::EndChild();
	ImGui::End();
}

void EditrGUI::WriteLog(std::string Log)
{
	std::string Buffer;
	Buffer += Log + "\n";
	LogBuffer += Buffer;
}

void EditrGUI::WriteLogBuffer()
{
	MessageBox(NULL, "エディタのログを保存します。", "ログ保存", MB_OK | MB_ICONINFORMATION);
	// txtファイルに保存
	std::string LogFilePath = SettingManager::GetInstance()->GetAssetsFilePath();
	LogFilePath += "/EditorLog.txt";
	// ファイルに書き込み
	std::ofstream ofs(LogFilePath, std::ios::out | std::ios::trunc);
	if (ofs.is_open()) {
		ofs << LogBuffer;
		ofs.close();
	}
}

void EditrGUI::ShowSceneCreateWindow()
{
	if (!ShowSceneCreate)return;
	ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
	if (ImGui::Begin(ShiftJISToUTF8("シーン作成##SceneManager_CreateScene").c_str(), &ShowSceneCreate, flags)) {
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
		Scene* current = SceneManger::GetInstance()->GetCurrentScene();
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
		for (const auto& scene : sceneList) { ImGui::Text(ShiftJISToUTF8(scene).c_str()); };
		ImGui::EndChild();
	}
	ImGui::End();
}

void EditrGUI::ShaderListWindow() {
	if (!ShowShaderListWindow)return;
	ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("シェーダーリスト").c_str(), &ShowShaderListWindow, flags)) {
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

void EditrGUI::ExternalToolsWindow() {
	if (!ShowExternalToolsWindow)return;
	ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("外部ツール管理").c_str(), &ShowExternalToolsWindow, flags)) {
		ImGui::Text(ShiftJISToUTF8("外部ツール管理ウインドウです").c_str());
		std::string ExternelTool = SettingManager::GetInstance()->GetExternelToolPath();
		ImGui::Text(ShiftJISToUTF8("外部ツールフォルダ:").c_str());
		ImGui::SameLine();
		ImGui::Text(ShiftJISToUTF8(ExternelTool).c_str());
		ImGui::Separator();
		ImGui::Text(ShiftJISToUTF8("説明").c_str());
		ImGui::TextWrapped(ShiftJISToUTF8("外部ツールフォルダにある実行ファイルを一覧表示します。").c_str());
		ImGui::TextWrapped(ShiftJISToUTF8("実行ファイルをクリックすると外部ツールが起動します。").c_str());
		ImGui::Separator();

		ImGui::End();
	}
}

void EditrGUI::SettingWindow()
{
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("環境設定").c_str(), &ShowSettingsWindow, flags)) {
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

		ImGui::Text(ShiftJISToUTF8("マウス感度設定").c_str());
		float mouseSensitivity = SettingManager::GetInstance()->GetMouseSensitivity();
		if (ImGui::SliderFloat(ShiftJISToUTF8("マウス感度:").c_str(), &mouseSensitivity, 0.01f, 1.0f))SettingManager::GetInstance()->SetMouseSensitivity(mouseSensitivity);
	}
	ImGui::End();
}

void EditrGUI::ShaderEditorWindow() {
	if (!ShowShaderEditorWindow)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (
		ImGui::Begin(ShiftJISToUTF8("シェーダー作成").c_str(), &ShowShaderEditorWindow, flags)) {
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

void EditrGUI::AssetManagerWindow() {
	if (!ShowAssetManagerWindow)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("アセットマネージャー").c_str(), &ShowAssetManagerWindow, flags)) {
		AssetManager::Instance()->DrawDebugGUI();
		ImGui::End();
	}
}

void EditrGUI::TextureManagerWindow() {
	if (!ShowTextureManagerWindow)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("テクスチャマネージャー").c_str(), &ShowTextureManagerWindow, flags)) {
		TextureManager::Instance()->DrawDebugGUI();
		ImGui::End();
	}
}

void EditrGUI::ModelManagerWindow() {
	if (!ShowModelManagerWindow)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("モデルマネージャー").c_str(), &ShowModelManagerWindow, flags)) {
		ModelManager::Instance()->DrawDebugGUI();
		ImGui::End();
	}
}

void EditrGUI::SoundManagerWindow() {
	if (!ShowSoundManagerWindow)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("サウンドマネージャー").c_str(), &ShowSoundManagerWindow, flags)) {
		SoundManager::Instance()->DrawDebugGUI();
		ImGui::End();
	}
}

void EditrGUI::ShowSceneRenameWindow()
{
	if (!ShowSceneRename)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("シーンリネーム").c_str(), &ShowSceneRename, flags))
	{
		static char oldSceneName[128] = "";
		static char newSceneName[128] = "";

		// stringをchar配列に変換
		std::string currentSceneName = SceneRenameNewName_;
		strncpy_s(oldSceneName, currentSceneName.c_str(), sizeof(oldSceneName));

		ImGui::InputText(ShiftJISToUTF8("現在のシーン名").c_str(), oldSceneName, sizeof(oldSceneName));
		ImGui::InputText(ShiftJISToUTF8("新しいシーン名").c_str(), newSceneName, sizeof(newSceneName));
		ImGui::Separator();
		ImGui::Text(ShiftJISToUTF8("説明").c_str());
		ImGui::TextWrapped(ShiftJISToUTF8("シーン名を変更します。現在のシーン名と新しいシーン名を入力してリネームボタンを押してください。").c_str());
		ImGui::Separator();
		if (ImGui::Button(ShiftJISToUTF8("リネーム実行").c_str(), ImVec2(120, 0))) {
			bool OK = SceneManger::GetInstance()->RenameFileInDirectory(oldSceneName, newSceneName);
			if (OK) {
				MessageBoxA(NULL, "シーン名の変更に成功しました。", "成功", MB_OK | MB_ICONINFORMATION);
				// 名前の初期化
				strcpy_s(oldSceneName, "");
				strcpy_s(newSceneName, "");
				SceneRenameNewName_ = "";
			}
			else {
				MessageBoxA(NULL, "シーン名の変更に失敗しました。", "失敗", MB_OK | MB_ICONERROR);
			}
		}
		ImGui::End();
	}
}

void EditrGUI::ShowLicenseWindow() {
	if (!ShowLicense)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("ライセンス情報").c_str(), &ShowLicense, flags)) {
		const char* license_text =
			"本ソフトウェアは、以下のオープンソースライブラリを使用しています。\n"
			"1. Dear ImGui (MIT License)\n"
			"2. Bullet Physics Library (zlib License)\n"
			"\n"
			"本ソフトウェアは改変・再配布自由ですが、改変配布時は作成者AC30Wの表記をお願いします。\n"
			"本ソフトウェアの利用によるいかなる損害に対しても、作成者は責任を負いません。\n"
			"\n"
			"Dear ImGui - MIT License\n"
			"Permission is hereby granted, free of charge, to any person obtaining a copy\n"
			"of this software and associated documentation files (the \"Software\"), to deal\n"
			"in the Software without restriction, including without limitation the rights\n"
			"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
			"copies of the Software, and to permit persons to whom the Software is\n"
			"furnished to do so, subject to the following conditions:\n"
			"The above copyright notice and this permission notice shall be included in all\n"
			"copies or substantial portions of the Software.\n"
			"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
			"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
			"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
			"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
			"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
			"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
			"SOFTWARE.\n"
			"\n"
			"Bullet Physics Library - zlib License\n"
			"This software is provided 'as-is', without any express or implied warranty.\n"
			"In no event will the authors be held liable for any damages arising from the use of this software.\n"
			"Permission is granted to anyone to use this software for any purpose,\n"
			"including commercial applications, and to alter it and redistribute it freely,\n"
			"subject to the following restrictions:\n"
			"1. The origin of this software must not be misrepresented;\n"
			"2. Altered source versions must be plainly marked as such;\n"
			"3. This notice may not be removed or altered from any source distribution.\n"
			"\n"
			"Copyright (c) AC30W\n"
			;
		ImGui::TextWrapped(ShiftJISToUTF8(license_text).c_str());

		ImGui::End();
	}
}

void EditrGUI::ShowInputDebug()
{
	if (!ShowInPutDebug)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("入力デバック").c_str(), &ShowInPutDebug, flags))
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

ID3D11ShaderResourceView* EditrGUI::LoadImg(const std::wstring& filename, ID3D11Device* device)
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