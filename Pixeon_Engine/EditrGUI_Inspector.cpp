#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "System.h"
#include "StartUp.h"
#include "EngineManager.h"
#include "SettingManager.h"
#include "SceneManger.h"
#include "Object.h"
#include "Component.h"
#include "ComponentManager.h"
#include "File.h"

void EditrGUI::ShowInspector()
{
	ImGui::Begin(ShiftJISToUTF8("インスペクター").c_str());
	if (SelectedObject) {
		ImGui::Text(ShiftJISToUTF8("オブジェクト名:").c_str());
		ImGui::SameLine();
		char buf[256];
		strcpy_s(buf, SelectedObject->GetObjectName().c_str());
		if (ImGui::InputText(ShiftJISToUTF8("##オブジェクト名").c_str(), buf, sizeof(buf))) {
			SelectedObject->SetObjectName(buf);
		}
		ImGui::Separator();
		ImGui::BeginChild("InspectorChild", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
		if (ImGui::CollapsingHeader(ShiftJISToUTF8("Transform").c_str())) {
			Transform TempTransform = SelectedObject->GetTransform();
			if (ImGui::DragFloat3(ShiftJISToUTF8("位置").c_str(), &TempTransform.position.x, 0.1f)) {
				SelectedObject->SetPosition(TempTransform.position.x, TempTransform.position.y, TempTransform.position.z);
			}

			DirectX::XMFLOAT3 rot;
			rot.x = DirectX::XMConvertToDegrees(TempTransform.rotation.x);
			rot.y = DirectX::XMConvertToDegrees(TempTransform.rotation.y);
			rot.z = DirectX::XMConvertToDegrees(TempTransform.rotation.z);

			if (ImGui::DragFloat3(ShiftJISToUTF8("回転").c_str(), &rot.x, 0.1f)) {
				TempTransform.rotation.x = DirectX::XMConvertToRadians(rot.x);
				TempTransform.rotation.y = DirectX::XMConvertToRadians(rot.y);
				TempTransform.rotation.z = DirectX::XMConvertToRadians(rot.z);

				SelectedObject->SetRotation(TempTransform.rotation.x, TempTransform.rotation.y, TempTransform.rotation.z);
			}
			if (ImGui::DragFloat3(ShiftJISToUTF8("スケール").c_str(), &TempTransform.scale.x, 0.1f)) {
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
					ImGui::Text(ShiftJISToUTF8("コンポーネント:").c_str());
					ImGui::SameLine();
					ImGui::Text(ShiftJISToUTF8(comp->GetComponentName()).c_str());
					ImGui::Separator();
					if (ImGui::Button(ShiftJISToUTF8("削除").c_str())) {
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
		ImGui::Combo(ShiftJISToUTF8("##Component追加").c_str(), &CurrentComponent, [](void* data, int idx, const char** out_text) {
			std::string* items = (std::string*)data;
			if (out_text) { *out_text = items[idx].c_str(); }
			return true;
			}, ComponentList, IM_ARRAYSIZE(ComponentList), IM_ARRAYSIZE(ComponentList));
		ImGui::SameLine();
		if (ImGui::Button(ShiftJISToUTF8("追加").c_str())) ComponentManager::GetInstance()->AddComponent(SelectedObject, (ComponentManager::COMPONENT_TYPE)CurrentComponent);

		ImGui::EndChild();
	}
	ImGui::End();
}