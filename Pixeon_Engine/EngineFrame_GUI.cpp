#include "EngineFrame.h"
#include "MainFrame.h"
#include "GUI.h"
#include "Input.h"
#include "SceneManger.h"
#include "Scene.h"
#include "Object.h"

#include "SettingManager.h"

Object* SelectedObject = nullptr;

void GameViewWindow();


void EngineFrame::DrawGUI()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGuiID DockSpace = ImGui::GetID("EngineFrameDockSpace");
	ImGui::DockSpace(DockSpace, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

	static bool EngineFrame_dock_init = false;
	if (!EngineFrame_dock_init){
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

}

void GameViewWindow()
{
	ImGui::Begin(GUI::GetInstance()->ShiftJISToUTF8("ゲームビュー").c_str());
	// --- 上部にコントロールバー ---
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

	// 再生・停止ボタン
	if (EngineFrame::GetInstance()->IsInGame()) {
		if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("再生").c_str(), ImVec2(70, 0)))
		{
			EngineFrame::GetInstance()->SetInGame(true);
			EngineFrame::GetInstance()->SetShowGUI(true);
		}
	}
	else {
		if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("停止").c_str(), ImVec2(70, 0)))
		{
			EngineFrame::GetInstance()->SetInGame(false);
			EngineFrame::GetInstance()->SetShowGUI(false);
			SelectedObject = nullptr;
		}
	}

	ImGui::PopStyleVar(2);

	ImGui::Separator();

	// --- ゲーム画面（プレビュー） ---
	ID3D11ShaderResourceView* srv = MainFrame::GetInstance()->GetGameRenderTargetSRV();
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

