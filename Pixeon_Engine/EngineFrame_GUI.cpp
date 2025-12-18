#include "EngineFrame.h"
#include "MainFrame.h"
#include "GUI.h"
#include "Input.h"
#include "SceneManger.h"
#include "Scene.h"
#include "Object.h"
#include "SettingManager.h"
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

	if (ext == ".scene") {
		if (ImGui::MenuItem(GUI::GetInstance()->ShiftJISToUTF8("名前変更").c_str())) {
			SceneRenameNewName_ = path.stem().string();
			ShowSceneRename = true;
		}
	}
}

void OpenWithVisualStudio(const std::string& filepath) {
	// 1) ファイルの存在チェック（相対→絶対に変換）
	char fullPathBuf[MAX_PATH];
	if (GetFullPathNameA(filepath.c_str(), MAX_PATH, fullPathBuf, nullptr) == 0) {
		// 失敗時はそのまま渡すが、ログ等を出すことを推奨
		strncpy_s(fullPathBuf, filepath.c_str(), MAX_PATH - 1);
	}
	// 存在しない場合はメッセージ表示（任意）
	if (GetFileAttributesA(fullPathBuf) == INVALID_FILE_ATTRIBUTES) {
		MessageBoxA(NULL, "指定されたファイルが存在しません。", "Visual Studio", MB_OK | MB_ICONERROR);
		return;
	}

	// 2) /Edit で既存インスタンスにエディタとして開かせる
	//    パスは必ず二重引用符で囲む
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

ImTextureID EngineFrame::GetAssetIcon(const std::string& name)
{
	size_t dot = name.find_last_of('.');
	std::string ext = (dot != std::string::npos) ? name.substr(dot) : "";
	if (ext == ".png" || ext == ".jpg") return (ImTextureID)ImgIcon_;
	if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") return (ImTextureID)SoundIcon_;
	if (ext == ".fbx" || ext == ".obj") return (ImTextureID)FbxIcon_;
	if (ext == ".scene") return (ImTextureID)SceneIcon_;
	if (ext == ".hlsl" || ext == ".fx") return (ImTextureID)ShaderIcon_;
	if (ext == ".cpp" || ext == ".h" || ext == ".cs") return (ImTextureID)ScriptIcon_;
	if (ext == ".json") return (ImTextureID)JsonIcon_;
	if (ext == ".PixAssets") return (ImTextureID)ArchiveIcon_;
	if (ext == ".exe") return (ImTextureID)ExeIcon_;
	return (ImTextureID)nullptr;
}

std::string AbbreviateName(const std::string& name, size_t maxBaseLen)
{
	// 拡張子とベース名を分離
	size_t dot = name.find_last_of('.');
	std::string ext = (dot != std::string::npos) ? name.substr(dot) : "";
	std::string base = (dot != std::string::npos) ? name.substr(0, dot) : name;

	// ベース名が十分短ければそのまま
	if (base.size() <= maxBaseLen) {
		return base + ext; // 元の名前と同じ
	}

	// 長い場合はベース名だけを "xxx..." にする
	//   maxBaseLen 文字以内に収める前提で「...」分を確保
	const size_t dotsLen = 3;
	if (maxBaseLen <= dotsLen) {
		// かなり小さい指定のときは保険で全部 "..." にする
		return std::string("...") + ext;
	}

	size_t remain = maxBaseLen - dotsLen;              // 先頭から残す文字数
	if (remain > base.size()) remain = base.size();

	std::string shortBase = base.substr(0, remain) + "...";
	return shortBase + ext;                            // ★ 最後に拡張子をそのまま付ける
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
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGuiID DockSpace = ImGui::GetID("EngineFrameDockSpace");
	ImGui::DockSpace(DockSpace, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

	static bool EngineFrame_dock_init = false;
	if (!EngineFrame_dock_init) {
		EngineFrame_dock_init = true;
		ImGui::DockBuilderRemoveNode(DockSpace); // DockSpaceリセット
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
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("コンソール").c_str(), dock_id_bottom);
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("Prefab").c_str(), dock_id_bottom);
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("コンテンツドロワー").c_str(), dock_id_bottom);
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("ヒエラルキー").c_str(), dock_id_left);

		ImGui::DockBuilderFinish(DockSpace);
	}

	ImGui::End();
	ImGui::PopStyleVar();

	GameViewWindow();
	if (!bShowGUI_)return;
	HierarchyWindow();
	InspectorWindow();
	ContentWindow();
	SceneRenameWindow();
}

void EngineFrame::GameViewWindow()
{
	ImGui::Begin(GUI::GetInstance()->ShiftJISToUTF8("ゲームビュー").c_str());
	// --- 上部にコントロールバー ---
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

	// 再生・停止ボタン
	if (!EngineFrame::GetInstance()->IsInGame()) {
		if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("再生").c_str(), ImVec2(70, 0)))
		{
			EngineFrame::GetInstance()->SetInGame(true);
			EngineFrame::GetInstance()->SetShowGUI(false);
		}
	}
	else {
		if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("停止").c_str(), ImVec2(70, 0)))
		{
			EngineFrame::GetInstance()->SetInGame(false);
			EngineFrame::GetInstance()->SetShowGUI(true);
			SelectedObject = nullptr;
		}
	}

	ImGui::PopStyleVar(2);

	ImGui::Separator();

	// --- ゲーム画面（プレビュー） ---
	ID3D11ShaderResourceView* srv = MainFrame::GetInstance()->GetFinalRenderTargetSRV();
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
		if (!bShowGUI_)return;
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

void  EngineFrame::HierarchyWindow()
{
	ImGui::Begin(GUI::GetInstance()->ShiftJISToUTF8("ヒエラルキー").c_str());
	// シーン内のオブジェクトをリスト表示
	Scene* currentScene = SceneManger::GetInstance()->GetCurrentScene();
	ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("シーン: ").c_str());
	ImGui::SameLine();
	ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8(currentScene ? currentScene->GetName() : "No Scene").c_str());
	ImGui::Separator();
	// 右クリックでコンテキストメニュー表示
	if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight))
	{
		if (ImGui::MenuItem(GUI::GetInstance()->ShiftJISToUTF8("オブジェクトの追加").c_str())) {
			AbstractObject* newObj = new AbstractObject();
			// 名前を比較、同じ名前付けられないようにする
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
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (obj == SelectedObject) {
				flags |= ImGuiTreeNodeFlags_Selected;
			}
			bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)obj, flags, GUI::GetInstance()->ShiftJISToUTF8(obj->GetObjectName()).c_str());
			if (ImGui::IsItemClicked()) {
				SelectedObject = obj;
			}

			// オブジェクトごとにユニークなラベルを作成
			std::string popupLabel = "ObjectContextMenu_" + std::to_string((intptr_t)obj);

			// 右クリックでコンテキストメニュー表示
			if (ImGui::BeginPopupContextItem(popupLabel.c_str(), ImGuiPopupFlags_MouseButtonRight))
			{
				if (ImGui::MenuItem(GUI::GetInstance()->ShiftJISToUTF8("Prefabとして保存").c_str())) {
					EngineFrame::GetInstance()->AddPrefab(obj);
				}
				if (ImGui::MenuItem(GUI::GetInstance()->ShiftJISToUTF8("削除").c_str())) {
					SceneManger::GetInstance()->GetCurrentScene()->RemoveObject(obj);
					SelectedObject = nullptr;
				}
				ImGui::EndPopup();
			}
			if (nodeOpen) {
				ImGui::TreePop();
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
		if (removeComponentIndex >= 0) {
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

void  EngineFrame::ContentWindow()
{
	// 初期パス設定（Assetsフォルダ）
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
	static const char* filterExts[] = { "", ".png", ".jpg", ".obj", ".txt", ".fbx", ".wav", ".mp3", ".ogg", ".hlsl", ".scene", ".cpp", ".h", ".cs" };
	static int filterIndex = 0;
	ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("フィルター:").c_str());
	ImGui::SameLine();
	if (ImGui::Combo("##ExtFilter", &filterIndex, filterExts, IM_ARRAYSIZE(filterExts))) {
		selectedExt = filterExts[filterIndex];
	}

	// フォルダ・ファイル一覧
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
		};

		ImGui::EndPopup();
	}

	float iconSize = 48.0f;
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

		std::string fileName = entry.path().filename().string(); // "PS_SSAO.hlsl"
		bool isDir = entry.is_directory();
		ImTextureID icon = isDir ? (ImTextureID)FolderIcon_ : EngineFrame::GetInstance()->GetAssetIcon(fileName);

		// 表示文字列と ID を分離
		std::string displayName = AbbreviateName(fileName, 12);        // 画面に表示する略称
		std::string idName = fileName + "##" + std::to_string(index++); // ImGui ID

		float groupX = ImGui::GetCursorPosX();

		// アイコンを横方向センタリング
		float cursorX = groupX + (itemWidth - iconSize) * 0.5f;
		ImGui::SetCursorPosX(cursorX);

		bool isSelected = (entry.path() == selectedEntryPath);

		// ボタン色
		if (!isSelected) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
		}
		else {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.6f, 1.0f, 0.5f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.7f, 1.0f, 0.7f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.5f, 1.0f, 1.0f));
		}

		bool iconClicked = false;
		if (icon) {
			iconClicked = ImGui::ImageButton(
				(std::string("icon_") + idName).c_str(),   // ID は idName を使う
				icon,
				ImVec2(iconSize, iconSize)
			);
		}
		else {
			iconClicked = ImGui::Button(
				(std::string("btn_") + idName).c_str(),
				ImVec2(iconSize, iconSize)
			);
		}

		ImGui::PopStyleColor(3);

		// テキストを横方向センタリング（表示文字列で幅計算）
		std::string textUTF8 = GUI::GetInstance()->ShiftJISToUTF8(displayName);
		float textWidth = ImGui::CalcTextSize(textUTF8.c_str()).x;
		ImGui::SetCursorPosX(groupX + (itemWidth - textWidth) * 0.5f);

		ImGui::PushID(idName.c_str());
		bool nameClicked = ImGui::Selectable(
			textUTF8.c_str(),
			isSelected, 0, ImVec2(itemWidth, 0)
		);

		// 右クリック（コンテキストメニュー）：BeginPopupContextItem を利用
		if (ImGui::BeginPopupContextItem("context")) {
			// エントリに対する右クリックメニューを表示
			if (!isDir) {
				HandleAssetContextMenu(entry.path());
			}
			else {
				// ディレクトリに対するメニュー（例）
				if (ImGui::MenuItem("Open")) {
					currentDir = entry.path();
				}
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();

		// クリック判定
		if (iconClicked || nameClicked) {
			selectedEntryPath = entry.path();
			if (isDir) {
				currentDir = entry.path();
			}
			else
			{
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