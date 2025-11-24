#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "System.h"
#include "StartUp.h"
#include "SettingManager.h"
#include "SceneManger.h"
#include "Object.h"
#include "Scene.h"
#include "File.h"

void EditrGUI::ShowHierarchy()
{
	ImGui::Begin(ShiftJISToUTF8("ヒエラルキー").c_str());
	// シーン内のオブジェクトをリスト表示
	Scene* currentScene = SceneManger::GetInstance()->GetCurrentScene();
	ImGui::Text(ShiftJISToUTF8("シーン: ").c_str());
	ImGui::SameLine();
	ImGui::Text(ShiftJISToUTF8(currentScene ? currentScene->GetName() : "No Scene").c_str());
	ImGui::Separator();
	// 右クリックでコンテキストメニュー表示
	if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight))
	{
		if (ImGui::MenuItem(ShiftJISToUTF8("オブジェクトの追加").c_str())) {
			Object* newObj = new Object();
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
		std::vector<Object*> objects = currentScene->GetObjects();
		for (size_t i = 0; i < objects.size(); ++i) {
			Object* obj = objects[i];
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (obj == SelectedObject) {
				flags |= ImGuiTreeNodeFlags_Selected;
			}
			bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)obj, flags, ShiftJISToUTF8(obj->GetObjectName()).c_str());
			if (ImGui::IsItemClicked()) {
				SelectedObject = obj;
			}

			// オブジェクトごとにユニークなラベルを作成
			std::string popupLabel = "ObjectContextMenu_" + std::to_string((intptr_t)obj);

			// 右クリックでコンテキストメニュー表示
			if (ImGui::BeginPopupContextItem(popupLabel.c_str(), ImGuiPopupFlags_MouseButtonRight))
			{
				if (ImGui::MenuItem(ShiftJISToUTF8("削除").c_str())) {
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