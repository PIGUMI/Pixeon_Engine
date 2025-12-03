#include "Animator2DComponent.h"
#include "EditrGUI.h"
#include <string>

void Animator2DComponent::Init(Object* Prt)
{
	_Parent = Prt;
	_ComponentName = "Animator2DComponent";
	_Type = ComponentManager::COMPONENT_TYPE::ANIMATOR2D;
	_animators.clear();
	Animator2D* Test = new Animator2D;
	_animators.push_back(Test);
}

void Animator2DComponent::InGameUpdate()
{
	for (auto& animator : _animators) {
		animator->Update();
	}
}

void Animator2DComponent::Draw()
{
	for (auto& animator : _animators) {
		animator->Draw();
	}
}

void Animator2DComponent::DrawInspector()
{
	std::string label = EditrGUI::GetInstance()->ShiftJISToUTF8(_ComponentName);
	std::string Ptr = std::to_string((uintptr_t)this);
	label += "###" + Ptr;
	if (!ImGui::CollapsingHeader(EditrGUI::GetInstance()->ShiftJISToUTF8(label).c_str())) return;
	if (ImGui::BeginTable(("Animator2D" + Ptr).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)){
		for (auto animator : _animators)
		{
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			std::string name;
			name = animator->GetProjectName();
			ImGui::Text(EditrGUI::GetInstance()->ShiftJISToUTF8(name).c_str());

			ImGui::TableSetColumnIndex(1);


		}
		ImGui::EndTable();
	}
	

}
