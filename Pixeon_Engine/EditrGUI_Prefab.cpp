
#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"

void EditrGUI::ShowPrefab()
{
	ImGui::Begin(ShiftJISToUTF8("Prefab").c_str());
	// プレハブ編集用のGUIをここに実装
	ImGui::Text(ShiftJISToUTF8("プレハブ編集機能は現在開発中です。").c_str());
	ImGui::End();
}
