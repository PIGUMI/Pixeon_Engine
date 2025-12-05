#include "EngineFrame.h"
#include "GUI.h"

void EngineFrame::DrawGUI()
{
	ImGuiID DockSpace = ImGui::GetID("EngineFrameDockSpace");
	ImGuiViewport* viewport = ImGui::GetMainViewport();
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


}