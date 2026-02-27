#include "EngineFrame.h"
#include "MainFrame.h"
#include "GUI.h"
#include "Input.h"
#include "SceneManger.h"
#include "Scene.h"
#include "Object.h"
#include "SettingManager.h"
#include "ChatWindow.h"
#include "IconsFontAwesome5.h"
#include <filesystem>
#include <vector>
#include <string>
#include <Windows.h>
#include <comdef.h>
#include <oleauto.h>

void EngineFrame::HandleAssetContextMenu(const std::filesystem::path& path)
{
	std::string ext = path.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
	std::string fullPath = path.string();

	if (ext == ".AbstractScene") {
		if (ImGui::MenuItem(GUI::GetInstance()->ShiftJISToUTF8("名前変更").c_str())) {
			SceneRenameNewName_ = path.stem().string();
			ShowSceneRename = true;
			selectedLayer_ = nullptr;
		}
	}
}

void OpenWithVisualStudio(const std::string& filepath) {
	char fullPathBuf[MAX_PATH];
	if (GetFullPathNameA(filepath.c_str(), MAX_PATH, fullPathBuf, nullptr) == 0) {
		strncpy_s(fullPathBuf, filepath.c_str(), MAX_PATH - 1);
	}
	if (GetFileAttributesA(fullPathBuf) == INVALID_FILE_ATTRIBUTES) {
		MessageBoxA(NULL, "指定されたファイルが存在しません。", "Visual Studio", MB_OK | MB_ICONERROR);
		return;
	}

	std::string args = "/Edit \"" + std::string(fullPathBuf) + "\"";

	ShellExecuteA(
		NULL,
		"open",
		"devenv.exe",
		args.c_str(),
		NULL,
		SW_SHOWNORMAL
	);
}

const char* EngineFrame::GetAssetIconLabel(const std::string& name)
{
	size_t dot = name.find_last_of('.');
	std::string ext = (dot != std::string::npos) ? name.substr(dot) : "";

	// 拡張子ごとにFontAwesomeアイコンを割り当て
	if (ext == ".png" || ext == ".jpg")      return ICON_FA_IMAGE;
	if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") return ICON_FA_MUSIC;
	if (ext == ".fbx" || ext == ".obj")      return ICON_FA_CUBE;
	if (ext == ".scene")                     return ICON_FA_CLONE;
	if (ext == ".hlsl" || ext == ".fx")      return ICON_FA_CODE;
	if (ext == ".cpp" || ext == ".h" || ext == ".cs") return ICON_FA_FILE_CODE;
	if (ext == ".json")                      return ICON_FA_FILE_ALT;
	if (ext == ".PixAssets")                 return ICON_FA_ARCHIVE;
	if (ext == ".exe")                       return ICON_FA_COG;
	if (ext == ".txt")                       return ICON_FA_FILE_ALT;
	if (ext == ".folder" || ext == "")       return ICON_FA_FOLDER;
	// その他はファイルアイコン
	return ICON_FA_FILE;
}

std::string AbbreviateName(const std::string& name, size_t maxBaseLen)
{
	size_t dot = name.find_last_of('.');
	std::string ext = (dot != std::string::npos) ? name.substr(dot) : "";
	std::string base = (dot != std::string::npos) ? name.substr(0, dot) : name;

	if (base.size() <= maxBaseLen) {
		return base + ext;
	}

	const size_t dotsLen = 3;
	if (maxBaseLen <= dotsLen) {
		return std::string("...") + ext;
	}

	size_t remain = maxBaseLen - dotsLen;
	if (remain > base.size()) remain = base.size();

	std::string shortBase = base.substr(0, remain) + "...";
	return shortBase + ext;
}

std::wstring ToWideACP(const std::string& s) {
	int len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0);
	if (len <= 0) return L"";
	std::wstring w(len - 1, 0);
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &w[0], len);
	return w;
}

void  EngineFrame::HandleAssetClick(const std::filesystem::path& path)
{
	std::string ext = path.extension().string();
	// 大文字小文字を区別しないように小文字化
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
	std::string fullPath = path.string();

	if (ext == ".cpp" || ext == ".h" || ext == ".cs" || ext == ".hlsl" || ext == ".fx" || ext == ".json") {
		// ソースコードやシェーダは Visual Studio で開く（devenv がインストールされている場合）
		OpenWithVisualStudio(fullPath);
	}

	// シーンファイルを開く
	if (ext == ".scene")
	{
		// 拡張子を除いた名前を取得
		std::string sceneName = path.stem().string();
		std::vector<std::string> sceneList = SceneManger::GetInstance()->GetSceneList();
		// シーンリストに存在する場合のみ切り替え
		SceneManger::GetInstance()->ChangeScene(sceneName);
		SelectedObject = nullptr;
	}
}

void EngineFrame::DrawGUI()
{
	StatusBarWindow();


	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGuiID DockSpace = ImGui::GetID("EngineFrameDockSpace");
	ImGui::DockSpace(DockSpace, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

	static bool EngineFrame_dock_init = false;
	if (!EngineFrame_dock_init) {
		EngineFrame_dock_init = true;
		ImGui::DockBuilderRemoveNode(DockSpace);
		ImGui::DockBuilderAddNode(DockSpace, ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(DockSpace, viewport->Size);

		ImGuiID dock_main_id = DockSpace;
		ImGuiID dock_id_right;
		ImGuiID dock_id_bottom;
		ImGuiID dock_id_left;

		// 右にInspector
		ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, &dock_id_right, &dock_main_id);
		// 下にContentDrawer
		ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.4f, &dock_id_bottom, &dock_main_id);
		// 左にHierarchy
		ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.18f, &dock_id_left, &dock_main_id);

		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("ゲームビュー").c_str(), dock_main_id);
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("インスペクター").c_str(), dock_id_right);
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("AIチャット[IZANAGI]").c_str(), dock_id_right);
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("コンソール").c_str(), dock_id_bottom);
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("Prefab").c_str(), dock_id_bottom);
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("コンテンツドロワー").c_str(), dock_id_bottom);
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("コンテンツドロワー").c_str(), dock_id_bottom);
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("ヒエラルキー").c_str(), dock_id_left);
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("レイヤー").c_str(), dock_id_left);
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("レイヤー設定").c_str(), dock_id_right);

		ImGui::DockBuilderFinish(DockSpace);
	}

	ImGui::End();
	ImGui::PopStyleVar();

	GameViewWindow();
	if (!bShowGUI_)return;
	HierarchyWindow();
	InspectorWindow();
	ContentWindow();
	PrefabWindow();
	SceneRenameWindow();
	LayerWindow();
	LayerInspectorWindow();
	ChatWindow::GetInstance()->Draw();
}

void EngineFrame::GameViewWindow()
{
	ImGui::Begin(GUI::GetInstance()->ShiftJISToUTF8("ゲームビュー").c_str());

	ImGui::PushStyleColor(ImGuiCol_Button, bInGame_ ? ImVec4(0.2f, 0.5f, 0.2f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
	if (ImGui::Button(ICON_FA_PLAY "##Play", ImVec2(50, 0))) {
		SetInGame(!bInGame_);
		SetShowGUI(bInGame_ ? false : true);
	}
	ImGui::PopStyleColor();

	ID3D11ShaderResourceView* srv = MainFrame::GetInstance()->GetFinalRenderTargetSRV();
	ImVec2 size = ImGui::GetContentRegionAvail();
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
		if (!bShowGUI_)return;
		AbstractScene* Temp = nullptr;
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

				float scrollSpeed = SettingManager::GetInstance()->GetMouseSensitivity();
				scrollSpeed *= 10.0f;
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
					MoveX *= 0.1f;
					MoveY *= 0.1f;

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
					MoveX *= 0.1f;
					MoveY *= 0.1f;
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

void EngineFrame::HierarchyWindow()
{
	ImGui::Begin(GUI::GetInstance()->ShiftJISToUTF8("ヒエラルキー").c_str());

	AbstractScene* currentScene = SceneManger::GetInstance()->GetCurrentScene();
	ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("シーン:  ").c_str());
	ImGui::SameLine();
	ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8(currentScene ? currentScene->GetName() : "No AbstractScene").c_str());
	ImGui::Separator();

	// 右クリックでコンテキストメニュー表示
	if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight))
	{
		if (ImGui::MenuItem(GUI::GetInstance()->ShiftJISToUTF8("オブジェクトの追加").c_str())) {
			AbstractObject* newObj = new AbstractObject();
			int suffix = 1;
			std::string baseName = "NewObject";
			std::string newName = baseName;
			bool nameExists = true;
			while (nameExists) {
				nameExists = false;
				for (const auto& obj : currentScene->GetObjects()) {
					if (obj->GetObjectName() == newName) {
						nameExists = true;
						break;
					}
				}
				if (nameExists) {
					newName = baseName + std::to_string(suffix);
					suffix++;
				}
			}
			newObj->SetObjectName(newName);
			SceneManger::GetInstance()->GetCurrentScene()->AddObjectLocal(newObj);
		}
		ImGui::EndPopup();
	}

	if (currentScene) {
		std::vector<AbstractObject*> objects = currentScene->GetObjects();

		for (size_t i = 0; i < objects.size(); ++i) {
			AbstractObject* obj = objects[i];
			if (obj->GetParent() == nullptr) {
				DrawObjectNode(obj);
			}
		}
	}
	ImGui::End();
}

void  EngineFrame::InspectorWindow()
{
	ImGui::Begin(GUI::GetInstance()->ShiftJISToUTF8("インスペクター").c_str());
	if (SelectedObject) {
		ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("オブジェクト名:").c_str());
		ImGui::SameLine();
		char buf[256];
		strcpy_s(buf, SelectedObject->GetObjectName().c_str());
		if (ImGui::InputText(GUI::GetInstance()->ShiftJISToUTF8("##オブジェクト名").c_str(), buf, sizeof(buf))) {
			SelectedObject->SetObjectName(buf);
		}
		ImGui::Separator();
		ImGui::BeginChild("InspectorChild", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
		if (ImGui::CollapsingHeader(GUI::GetInstance()->ShiftJISToUTF8("Transform").c_str())) {
			Transform TempTransform = SelectedObject->GetTransform();
			if (ImGui::DragFloat3(GUI::GetInstance()->ShiftJISToUTF8("位置").c_str(), &TempTransform.position.x, 0.1f)) {
				SelectedObject->SetPosition(TempTransform.position.x, TempTransform.position.y, TempTransform.position.z);
			}

			DirectX::XMFLOAT3 rot;
			rot.x = DirectX::XMConvertToDegrees(TempTransform.rotation.x);
			rot.y = DirectX::XMConvertToDegrees(TempTransform.rotation.y);
			rot.z = DirectX::XMConvertToDegrees(TempTransform.rotation.z);

			if (ImGui::DragFloat3(GUI::GetInstance()->ShiftJISToUTF8("回転").c_str(), &rot.x, 0.1f)) {
				TempTransform.rotation.x = DirectX::XMConvertToRadians(rot.x);
				TempTransform.rotation.y = DirectX::XMConvertToRadians(rot.y);
				TempTransform.rotation.z = DirectX::XMConvertToRadians(rot.z);

				SelectedObject->SetRotation(TempTransform.rotation.x, TempTransform.rotation.y, TempTransform.rotation.z);
			}
			if (ImGui::DragFloat3(GUI::GetInstance()->ShiftJISToUTF8("スケール").c_str(), &TempTransform.scale.x, 0.1f)) {
				SelectedObject->SetScale(TempTransform.scale.x, TempTransform.scale.y, TempTransform.scale.z);
			}
		}
		ImGui::Separator();

		int removeComponentIndex = -1;

		auto components = SelectedObject->GetComponents();
		for (int i = 0; i < components.size(); ++i) {
			auto& comp = components[i];
			if (comp)
			{
				comp->DrawInspector();

				// コンポーネントごとに右クリックポップアップを割り当て
				if (ImGui::BeginPopupContextItem(comp->GetComponentName().c_str())) {
					ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("コンポーネント:").c_str());
					ImGui::SameLine();
					ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8(comp->GetComponentName()).c_str());
					ImGui::Separator();
					if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("複製").c_str())) {
						AbstractComponent* newComp = ComponentManager::GetInstance()->AddComponent(SelectedObject, comp->GetComponentType());
						std::stringstream ss;
						comp->SaveToFile(ss);
						newComp->LoadFromFile(ss);
						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();
					if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("削除").c_str())) {
						SelectedObject->RemoveComponent(comp);
						removeComponentIndex = i;
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				}
			}
		}

		// 削除処理
		if (removeComponentIndex >= 0 && removeComponentIndex < (int)components.size()) {
			SelectedObject->RemoveComponent(components[removeComponentIndex]);
		}

		//　コンポーネント追加UI
		std::string ComponentList[(int)ComponentManager::COMPONENT_TYPE::MAX];
		for (int i = 0; i < (int)ComponentManager::COMPONENT_TYPE::MAX; i++) {
			ComponentList[i] = ComponentManager::GetInstance()->GetComponentName((ComponentManager::COMPONENT_TYPE)i);
		}
		static int CurrentComponent = 0;
		ImGui::Combo(GUI::GetInstance()->ShiftJISToUTF8("##Component追加").c_str(), &CurrentComponent, [](void* data, int idx, const char** out_text) {
			std::string* items = (std::string*)data;
			if (out_text) { *out_text = items[idx].c_str(); }
			return true;
			}, ComponentList, IM_ARRAYSIZE(ComponentList), IM_ARRAYSIZE(ComponentList));
		ImGui::SameLine();
		if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("追加").c_str())) ComponentManager::GetInstance()->AddComponent(SelectedObject, (ComponentManager::COMPONENT_TYPE)CurrentComponent);

		ImGui::EndChild();
	}
	ImGui::End();
}

void EngineFrame::ContentWindow()
{
	if (currentDir.empty()) {
		std::string assetsPath = "SceneRoot/";
		currentDir = assetsPath;
	}

	ImGui::Begin(GUI::GetInstance()->ShiftJISToUTF8("コンテンツドロワー").c_str());

	// フォルダ階層表示（戻るボタン）
	if (currentDir.has_parent_path()) {
		if (ImGui::Button("..")) {
			currentDir = currentDir.parent_path();
		}
		ImGui::SameLine();
		ImGui::Text("%s", currentDir.string().c_str());
	}

	// 拡張子フィルター
	static const char* filterExts[] = { "", ".png", ".jpg", ".obj", ".txt", ".fbx", ".wav", ".mp3", ".ogg", ".hlsl", ".AbstractScene", ".cpp", ".h", ".cs" };
	static int filterIndex = 0;
	ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("フィルター:").c_str());
	ImGui::SameLine();
	if (ImGui::Combo("##ExtFilter", &filterIndex, filterExts, IM_ARRAYSIZE(filterExts))) {
		selectedExt = filterExts[filterIndex];
	}

	// フォルダ・ファイル一覧を取得
	std::vector<std::filesystem::directory_entry> entries;
	for (auto& entry : std::filesystem::directory_iterator(currentDir)) {
		if (entry.is_directory() || selectedExt.empty() || entry.path().extension() == selectedExt) {
			entries.push_back(entry);
		}
	}

	ImGui::BeginChild("assets_grid", ImVec2(0, 0), true);

	// 右クリック処理
	if (ImGui::BeginPopupContextWindow("assets_context", ImGuiMouseButton_Right)) {
		if (ImGui::MenuItem(GUI::GetInstance()->ShiftJISToUTF8("新しいフォルダを作成").c_str())) {
			std::filesystem::path newFolderPath = currentDir / "NewFolder";
			int suffix = 1;
			while (std::filesystem::exists(newFolderPath)) {
				newFolderPath = currentDir / ("NewFolder" + std::to_string(suffix));
				suffix++;
			}
			std::filesystem::create_directory(newFolderPath);
		}
		if (ImGui::MenuItem(GUI::GetInstance()->ShiftJISToUTF8("シーンの作成").c_str())) {
			GUI::GetInstance()->bSceneCreateWindow_ = true;
		}
		ImGui::EndPopup();
	}

	float iconFontSize = 32.0f; // FontAwesomeアイコン用のサイズ
	float itemWidth = 96.0f;
	float itemHeight = 80.0f;
	float availWidth = ImGui::GetContentRegionAvail().x;
	int columns = static_cast<int>(availWidth / itemWidth);
	if (columns < 1) columns = 1;
	ImGui::Columns(columns, nullptr, false);

	static std::filesystem::path selectedEntryPath; // 選択中のパス

	int index = 0;

	for (const auto& entry : entries) {
		ImGui::BeginGroup();

		std::string fileName = entry.path().filename().string();
		bool isDir = entry.is_directory();

		// FontAwesomeアイコンを取得
		std::string iconLabel;
		if (isDir) {
			iconLabel = ICON_FA_FOLDER;
		}
		else {
			iconLabel = EngineFrame::GetInstance()->GetAssetIconLabel(fileName);
		}

		std::string displayName = AbbreviateName(fileName, 12);
		std::string idName = fileName + "##" + std::to_string(index++);

		float groupX = ImGui::GetCursorPosX();

		// アイコンをセンタリング
		ImGui::SetCursorPosX(groupX + (itemWidth - iconFontSize) * 0.5f);
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
		ImGui::TextColored(ImVec4(0.86f, 0.86f, 0.86f, 1.0f), "%s", iconLabel.c_str());
		ImGui::PopFont();

		// テキストもセンタリング
		std::string textUTF8 = GUI::GetInstance()->ShiftJISToUTF8(displayName);
		float textWidth = ImGui::CalcTextSize(textUTF8.c_str()).x;
		ImGui::SetCursorPosX(groupX + (itemWidth - textWidth) * 0.5f);

		ImGui::PushID(idName.c_str());
		bool nameClicked = ImGui::Selectable(
			textUTF8.c_str(),
			entry.path() == selectedEntryPath, 0, ImVec2(itemWidth, 0)
		);

		// 右クリック（コンテキストメニュー）
		if (ImGui::BeginPopupContextItem("context")) {
			if (!isDir) {
				HandleAssetContextMenu(entry.path());
			}
			else {
				if (ImGui::MenuItem("Open")) {
					currentDir = entry.path();
				}
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();

		// クリック判定
		if (nameClicked) {
			selectedEntryPath = entry.path();
			if (isDir) {
				currentDir = entry.path();
			}
			else {
				HandleAssetClick(entry.path());
			}
		}

		ImGui::EndGroup();
		ImGui::NextColumn();
	}

	ImGui::Columns(1);
	ImGui::EndChild();
	ImGui::End();
}

void EngineFrame::PrefabWindow() {
	ImGui::Begin(GUI::GetInstance()->ShiftJISToUTF8("Prefab").c_str());

	std::vector<AbstractObject*> prefabs = EngineFrame::GetInstance()->GetPrefabs();

	if (prefabs.empty()) {
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
			GUI::GetInstance()->ShiftJISToUTF8("Prefabがありません").c_str());
		ImGui::TextWrapped(
			GUI::GetInstance()->ShiftJISToUTF8(
				"ヒエラルキーでオブジェクトを右クリックして「Prefabとして保存」を選択してください。"
			).c_str());
		ImGui::End();
		return;
	}

	if (ImGui::BeginPopupContextWindow("prefab_context", ImGuiMouseButton_Right)) {
		if (ImGui::MenuItem(GUI::GetInstance()->ShiftJISToUTF8("すべてクリア").c_str())) {
			for (auto prefab : prefabs) {
				EngineFrame::GetInstance()->RemovePrefab(prefab);
			}
		}
		ImGui::EndPopup();
	}

	ImGui::BeginChild("prefab_grid", ImVec2(0, 0), true);

	float iconFontSize = 32.0f; // FontAwesome用
	float itemWidth = 96.0f;
	float availWidth = ImGui::GetContentRegionAvail().x;
	int columns = static_cast<int>(availWidth / itemWidth);
	if (columns < 1) columns = 1;
	ImGui::Columns(columns, nullptr, false);

	static AbstractObject* selectedPrefab = nullptr;
	int index = 0;

	for (auto prefab : prefabs) {
		if (!prefab) continue;

		ImGui::BeginGroup();

		std::string prefabName = prefab->GetObjectName();
		std::string displayName = AbbreviateName(prefabName, 12);
		std::string idName = prefabName + "##prefab_" + std::to_string(index++);

		float groupX = ImGui::GetCursorPosX();

		// Prefab（立方体アイコンに決定！）
		ImGui::SetCursorPosX(groupX + (itemWidth - iconFontSize) * 0.5f);
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
		ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), ICON_FA_CUBE);
		ImGui::PopFont();

		// テキストもセンタリング
		std::string textUTF8 = GUI::GetInstance()->ShiftJISToUTF8(displayName);
		float textWidth = ImGui::CalcTextSize(textUTF8.c_str()).x;
		ImGui::SetCursorPosX(groupX + (itemWidth - textWidth) * 0.5f);

		ImGui::PushID(idName.c_str());
		bool nameClicked = ImGui::Selectable(
			textUTF8.c_str(),
			prefab == selectedPrefab, 0, ImVec2(itemWidth, 0)
		);

		if (ImGui::BeginPopupContextItem("prefab_item_context")) {
			ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("Prefab:  %s").c_str(), prefabName.c_str());
			ImGui::Separator();

			if (ImGui::MenuItem(GUI::GetInstance()->ShiftJISToUTF8("シーンに追加").c_str())) {
				AbstractScene* currentScene = SceneManger::GetInstance()->GetCurrentScene();
				if (currentScene) {
					AbstractObject* newObj = prefab->Clone();

					std::string baseName = newObj->GetObjectName();
					int count = 1;
					std::string newName = baseName;
					bool nameExists = true;
					while (nameExists) {
						nameExists = false;
						for (const auto& obj : currentScene->GetObjects()) {
							if (obj->GetObjectName() == newName) {
								nameExists = true;
								break;
							}
						}
						if (nameExists) {
							newName = baseName + std::to_string(count);
							count++;
						}
					}
					newObj->SetObjectName(newName);

					currentScene->AddObjectLocal(newObj);

					std::function<void(AbstractObject*)> addAllChildren = [&](AbstractObject* parent) {
						for (auto child : parent->GetChildren()) {
							if (child) {
								currentScene->AddObjectLocal(child);
								addAllChildren(child);
							}
						}
						};
					addAllChildren(newObj);
				}
			}

			if (ImGui::MenuItem(GUI::GetInstance()->ShiftJISToUTF8("削除").c_str())) {
				EngineFrame::GetInstance()->RemovePrefab(prefab);
				if (selectedPrefab == prefab) {
					selectedPrefab = nullptr;
				}
			}

			ImGui::EndPopup();
		}
		ImGui::PopID();

		if (nameClicked) {
			selectedPrefab = prefab;

			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				AbstractScene* currentScene = SceneManger::GetInstance()->GetCurrentScene();
				if (currentScene) {
					AbstractObject* newObj = prefab->Clone();

					std::string baseName = newObj->GetObjectName();
					int count = 1;
					std::string newName = baseName;
					bool nameExists = true;
					while (nameExists) {
						nameExists = false;
						for (const auto& obj : currentScene->GetObjects()) {
							if (obj->GetObjectName() == newName) {
								nameExists = true;
								break;
							}
						}
						if (nameExists) {
							newName = baseName + std::to_string(count);
							count++;
						}
					}
					newObj->SetObjectName(newName);

					currentScene->AddObjectLocal(newObj);

					std::function<void(AbstractObject*)> addAllChildren = [&](AbstractObject* parent) {
						for (auto child : parent->GetChildren()) {
							if (child) {
								currentScene->AddObjectLocal(child);
								addAllChildren(child);
							}
						}
						};
					addAllChildren(newObj);
				}
			}
		}

		ImGui::EndGroup();
		ImGui::NextColumn();
	}

	ImGui::Columns(1);
	ImGui::EndChild();
	ImGui::End();
}

void EngineFrame::LayerWindow()
{
	ImGui::Begin(GUI::GetInstance()->ShiftJISToUTF8("レイヤー").c_str());

	for (int i = 0; i < MAX_LAYER_COUNT; i++) {
		if (SceneManger::GetInstance() == nullptr) return;
		if (SceneManger::GetInstance()->GetCurrentScene() == nullptr) return;
		auto layer = SceneManger::GetInstance()->GetCurrentScene()->GetLayer(i);

		ImGui::PushID(i);

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth;
		if (layer == selectedLayer_) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		std::string label = layer->name + " [" + std::to_string(i) + "]";
		bool nodeOpen = ImGui::TreeNodeEx(&layer, flags,
			GUI::GetInstance()->ShiftJISToUTF8(label).c_str());

		if (ImGui::IsItemClicked()) {
			selectedLayer_ = layer;
		}

		ImGui::SameLine();
		ImGui::Checkbox(("##visible" + std::to_string(i)).c_str(), &layer->visible);

		if (nodeOpen) {
			for (size_t j = 0; j < layer->postEffects.size(); j++) {
				auto& effect = layer->postEffects[j];
				ImGui::BulletText("%s %s",
					effect->GetName().c_str(),
					effect->enabled ? "" : "(Disabled)");
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	ImGui::End();
}

void EngineFrame::LayerInspectorWindow()
{
	ImGui::Begin(GUI::GetInstance()->ShiftJISToUTF8("レイヤー設定").c_str());

	if (selectedLayer_) {
		// 基本設定
		char nameBuf[128];
		strcpy_s(nameBuf, selectedLayer_->name.c_str());
		if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
			selectedLayer_->name = nameBuf;
		}

		ImGui::DragFloat("Opacity", &selectedLayer_->opacity, 0.01f, 0.0f, 1.0f);
		ImGui::Checkbox("Visible", &selectedLayer_->visible);

		ImGui::Separator();
		ImGui::Text("Post Effects:");

		// ポストエフェクト一覧
		for (size_t i = 0; i < selectedLayer_->postEffects.size(); i++) {
			auto& effect = selectedLayer_->postEffects[i];

			std::string header = effect->GetName() + "##" + std::to_string(i);
			if (ImGui::CollapsingHeader(header.c_str())) {
				effect->DrawInspector();

				if (ImGui::Button(("Remove##" + std::to_string(i)).c_str())) {
					selectedLayer_->postEffects.erase(
						selectedLayer_->postEffects.begin() + i);
					break;
				}
			}
		}

		ImGui::Separator();

		// エフェクト追加UI
		static int currentEffect = 0;
		const char* effectNames[] = {
			"Bloom", "Blur", "Pixelate", "Color Grading",
			"Vignette", "Chromatic Aberration"
		};

		ImGui::Combo("##EffectType", &currentEffect, effectNames,
			IM_ARRAYSIZE(effectNames));
		ImGui::SameLine();
		if (ImGui::Button("Add Effect")) {
			selectedLayer_->AddPostEffect((PostEffectType)(currentEffect + 1));
		}
	}
	else {
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
			GUI::GetInstance()->ShiftJISToUTF8("レイヤーを選択してください").c_str());
	}

	ImGui::End();
}

void EngineFrame::SceneRenameWindow()
{
	if (!ShowSceneRename)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(GUI::GetInstance()->ShiftJISToUTF8("シーンリネーム").c_str(), &ShowSceneRename, flags))
	{
		static char oldSceneName[128] = "";
		static char newSceneName[128] = "";

		// stringをchar配列に変換
		std::string currentSceneName = SceneRenameNewName_;
		strncpy_s(oldSceneName, currentSceneName.c_str(), sizeof(oldSceneName));

		ImGui::InputText(GUI::GetInstance()->ShiftJISToUTF8("現在のシーン名").c_str(), oldSceneName, sizeof(oldSceneName));
		ImGui::InputText(GUI::GetInstance()->ShiftJISToUTF8("新しいシーン名").c_str(), newSceneName, sizeof(newSceneName));
		ImGui::Separator();
		ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("説明").c_str());
		ImGui::TextWrapped(GUI::GetInstance()->ShiftJISToUTF8("シーン名を変更します。現在のシーン名と新しいシーン名を入力してリネームボタンを押してください。").c_str());
		ImGui::Separator();
		if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("リネーム実行").c_str(), ImVec2(120, 0))) {
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

void EngineFrame::DrawObjectNode(AbstractObject* obj)
{
	if (!obj) return;

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

	// 子オブジェクトがない場合は葉ノード
	if (obj->GetChildren().empty()) {
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	if (obj == SelectedObject) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	bool nodeOpen = ImGui::TreeNodeEx(
		(void*)(intptr_t)obj,
		flags,
		ICON_FA_CUBE " %s", GUI::GetInstance()->ShiftJISToUTF8(obj->GetObjectName()).c_str()
	);

	// クリックで選択
	if (ImGui::IsItemClicked()) {
		SelectedObject = obj;
	}

	// ドラッグ&ドロップソース
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
		ImGui::SetDragDropPayload("HIERARCHY_OBJECT", &obj, sizeof(AbstractObject*));
		ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8(obj->GetObjectName()).c_str());
		ImGui::EndDragDropSource();
	}

	// ドラッグ&ドロップターゲット
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_OBJECT")) {
			AbstractObject* draggedObj = *(AbstractObject**)payload->Data;
			if (draggedObj && draggedObj != obj) {
				// 循環参照チェック
				bool isCircular = false;
				AbstractObject* parent = obj;
				while (parent) {
					if (parent == draggedObj) {
						isCircular = true;
						break;
					}
					parent = parent->GetParent();
				}

				if (!isCircular) {
					draggedObj->SetParent(obj);
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	// 右クリックでコンテキストメニュー表示
	std::string popupLabel = "ObjectContextMenu_" + std::to_string((intptr_t)obj);
	if (ImGui::BeginPopupContextItem(popupLabel.c_str(), ImGuiPopupFlags_MouseButtonRight))
	{
		if (ImGui::MenuItem(GUI::GetInstance()->ShiftJISToUTF8("複製").c_str())) {
			AbstractObject* newObj = obj->Clone();
			int suffix = 1;
			std::string baseName = obj->GetObjectName();
			std::string newName = baseName;
			// 名前の重複チェック
			AbstractScene* AbstractScene = SceneManger::GetInstance()->GetCurrentScene();
			bool nameExists = true;
			while (nameExists) {
				nameExists = false;
				for (const auto& sceneObj : AbstractScene->GetObjects()) {
					if (sceneObj->GetObjectName() == newName) {
						nameExists = true;
						break;
					}
				}
				if (nameExists) {
					newName = baseName + std::to_string(suffix);
					suffix++;
				}
			}
			newObj->SetParent(obj->GetParent());
			if (obj->GetParent())
			{
				obj->GetParent()->AddChild(newObj);
			}
			newObj->SetObjectName(newName);
			AbstractScene->AddObjectLocal(newObj);
		}
		if (ImGui::MenuItem(GUI::GetInstance()->ShiftJISToUTF8("子オブジェクトを作成").c_str())) {
			AbstractObject* newChild = new AbstractObject();
			int suffix = 1;
			std::string baseName = "ChildObject";
			std::string newName = baseName;

			// 名前の重複チェック
			AbstractScene* AbstractScene = SceneManger::GetInstance()->GetCurrentScene();
			bool nameExists = true;
			while (nameExists) {
				nameExists = false;
				for (const auto& sceneObj : AbstractScene->GetObjects()) {
					if (sceneObj->GetObjectName() == newName) {
						nameExists = true;
						break;
					}
				}
				if (nameExists) {
					newName = baseName + std::to_string(suffix);
					suffix++;
				}
			}

			newChild->SetObjectName(newName);
			newChild->SetParent(obj);
			AbstractScene->AddObjectLocal(newChild);
		}

		if (obj->GetParent() && ImGui::MenuItem(GUI::GetInstance()->ShiftJISToUTF8("親から切り離す").c_str())) {
			obj->RemoveParent();
		}

		if (ImGui::MenuItem(GUI::GetInstance()->ShiftJISToUTF8("Prefabとして保存").c_str())) {
			EngineFrame::GetInstance()->AddPrefab(obj);
		}

		if (ImGui::MenuItem(GUI::GetInstance()->ShiftJISToUTF8("削除").c_str())) {
			// 子オブジェクトも含めて削除
			SceneManger::GetInstance()->GetCurrentScene()->RemoveObject(obj);
			SelectedObject = nullptr;
		}
		ImGui::EndPopup();
	}

	// ツリーノードが開いている場合、子オブジェクトを表示
	if (nodeOpen && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
		for (auto child : obj->GetChildren()) {
			DrawObjectNode(child);
		}
		ImGui::TreePop();
	}
}

void EngineFrame::StatusBarWindow()
{
	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_MenuBar;

	float height = ImGui::GetFrameHeight();
	if (ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(),
		ImGuiDir_Down, height, flags))
	{
		if (ImGui::BeginMenuBar())
		{
			// FPS表示
			ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
			ImGui::Separator();

			int CameraNumber = 0;
			if (SceneManger::GetInstance() && SceneManger::GetInstance()->GetCurrentScene() && SceneManger::GetInstance()->GetCurrentScene()->GetMainCamera()) {
				CameraNumber = SceneManger::GetInstance()->GetCurrentScene()->GetMainCamera()->GetCameraNumber();
			}
			ImGui::Text(ICON_FA_CAMERA " Camera: %d", CameraNumber);

			ImGui::EndMenuBar();
		}
		ImGui::End();
	}
}