#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "EngineManager.h"
#include "Object.h"

#include <vector>


void EditrGUI::ShowPrefab()
{
	ImGui::Begin(ShiftJISToUTF8("Prefab").c_str());
	/* PrefabƒŠƒXƒg‚ÌŽæ“¾ */
	std::vector<Object*> prefabList = EngineManager::GetInstance()->GetPrefabs();


	




	ImGui::End();
}