#include "Animator2DComponent.h"
#include "Animator2D.h"
#include "SettingManager.h"
#include "Animator2DManager.h"
#include "ImageRender.h"
#include "GUI.h"
#include <string>

void Animator2DComponent::Init(Object* Prt)
{
	_Parent = Prt;
	_ComponentName = "Animator2DComponent";
	_Type = ComponentManager::COMPONENT_TYPE::ANIMATOR2D;
	_animators.clear();
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
	std::string label = GUI::GetInstance()->ShiftJISToUTF8(_ComponentName);
	std::string Ptr = std::to_string((uintptr_t)this);
	label += "###" + Ptr;
	if (!ImGui::CollapsingHeader(GUI::GetInstance()->ShiftJISToUTF8(label).c_str())) return;
	if (ImGui::BeginTable(("Animator2D" + Ptr).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		if(ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("Animator2D追加").c_str()))
		{
			ImGui::OpenPopup("AddAnimator2D");
		}
		DrawAnimator2DPopup();
		ImGui::TableSetColumnIndex(1);


		for (auto& animator : _animators) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("プロジェクト名").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::Text(animator->GetProjectName().c_str());
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("総再生時間").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::Text(std::to_string(animator->GetTotalTime()).c_str());
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("ループ設定").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::Text(animator->GetLoop() ? GUI::GetInstance()->ShiftJISToUTF8("有効").c_str() : GUI::GetInstance()->ShiftJISToUTF8("無効").c_str());
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("ビュー設定").c_str());
			ImGui::TableSetColumnIndex(1);
			std::string viewModeStr = (animator->GetViewMode() == Animator2D::ViewMode::Billboard) ? "Billboard" : "Fixed";
			ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8(viewModeStr).c_str());
		}

		ImGui::EndTable();
	}

}

void Animator2DComponent::DrawAnimator2DPopup()
{
	if (ImGui::BeginPopupModal("AddAnimator2D", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };
		ImGui::Text(SJ("Animator2D追加します").c_str());

		if (projectFiles_.empty()) {
			const std::string projectDir = SettingManager::GetInstance()->GetAnimator2DProjectFilePath();
			projectFiles_.clear();
			for (const auto& entry : std::filesystem::directory_iterator(projectDir)) {
				if (entry.is_regular_file()) {
					if (entry.path().extension() == ".anim2d") {
						projectFiles_.push_back(entry.path().filename().string());
					}
				}
			}
		}

		for (int i = 0; i < projectFiles_.size(); ++i) {
			bool selected = (i == selectedProjectIndex_);
			if (ImGui::Selectable(projectFiles_[i].c_str(), selected)) {
				selectedProjectIndex_ = i;
				if (selectedProjectIndex_ < 0 && selectedProjectIndex_ >= projectFiles_.size())return;
				Animator2D* newAnimator = Animator2DManager::GetInstance()->GetAnimator2D(projectFiles_[selectedProjectIndex_]);
				_animators.push_back(newAnimator);
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::Separator();
		if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("キャンセル").c_str())) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}