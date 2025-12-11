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

void Animator2DComponent::EditUpdate()
{
	for (auto& animator : _animators) {
		animator->EditorUpdate();
		animator->PreviewUpdate();
	}
}

void Animator2DComponent::Draw()
{
	int count = 0;
	for (auto& animator : _animators) {
		animator->SetEditorMode(false);
		animator->SetViewMode(animatorViewModes_[count]);
		animator->Draw();
		count++;
	}
}

void Animator2DComponent::UInit()
{
	for (auto& animator : _animators) {
		delete animator;
		animator = nullptr;
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
		if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("Animator2D追加").c_str()))
		{
			ImGui::OpenPopup("AddAnimator2D");
		}
		DrawAnimator2DPopup();
		ImGui::TableSetColumnIndex(1);

		int count = 0;
		std::string msg;
		for (auto& animator : _animators) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("プロジェクト名").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::Text(animator->GetProjectName().c_str());
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("再生時間").c_str());
			ImGui::TableSetColumnIndex(1);
			msg = std::to_string(animator->fNowTime_);
			msg += " / ";
			msg += std::to_string(animator->GetTotalTime());
			ImGui::Text(msg.c_str());
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("ループ設定").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::Checkbox(("##LoopSetting" + animator->GetProjectName() + Ptr).c_str(), &animator->bLoop_);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			if (animator->GetViewMode() == ViewMode::Billboard)
			{
				ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("ビュー設定:ビルボード").c_str());
			}
			else
			{
				ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("ビュー設定:UI").c_str());
			}
			ImGui::TableSetColumnIndex(1);
			if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("ビュー切替##" + animator->GetProjectName() + Ptr).c_str()))
			{
				if (animator->GetViewMode() == ViewMode::Billboard)
				{
					animator->SetViewMode(ViewMode::UI);
					animatorViewModes_[count] = ViewMode::UI;
				}
				else
				{
					animator->SetViewMode(ViewMode::Billboard);
					animatorViewModes_[count] = ViewMode::Billboard;
				}
			}
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("削除").c_str());
			ImGui::TableSetColumnIndex(1);
			std::string msg = "削除##" + animator->GetProjectName() + Ptr;
			if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8(msg).c_str())) {
				_animators.erase(std::remove(_animators.begin(), _animators.end(), animator), _animators.end());
				animatorNames_.erase(animatorNames_.begin() + count);
				animatorViewModes_.erase(animatorViewModes_.begin() + count);
				delete animator;
				animator = nullptr;
				break;
			}
			for (auto kf : animator->GetKeyFrames())
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("NowPos").c_str());
				ImGui::TableSetColumnIndex(1);
				msg = "X:" + std::to_string(kf.NowTransform.Position.x);
				msg += "Y:" + std::to_string(kf.NowTransform.Position.y);
				ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8(msg).c_str());

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("NowSize").c_str());
				ImGui::TableSetColumnIndex(1);
				msg = "X:" + std::to_string(kf.NowTransform.Scale.x);
				msg += "Y:" + std::to_string(kf.NowTransform.Scale.y);
				ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8(msg).c_str());
			}
			count++;
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
				newAnimator->SetOwner(_Parent);
				_animators.push_back(newAnimator);
				animatorNames_.push_back(projectFiles_[selectedProjectIndex_]);
				animatorViewModes_.push_back(ViewMode::UI);
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

void Animator2DComponent::SaveToFile(std::ostream& out)
{
	int animatorCount = static_cast<int>(_animators.size());
	out.write(reinterpret_cast<const char*>(&animatorCount), sizeof(int));
	for (const auto& animator : _animators) {
		std::string projectName = animator->GetProjectName();
		int nameLength = static_cast<int>(projectName.size());
		out.write(reinterpret_cast<const char*>(&nameLength), sizeof(int));
		out.write(projectName.c_str(), nameLength);
	}
	int viewModeCount = static_cast<int>(animatorViewModes_.size());
	out.write(reinterpret_cast<const char*>(&viewModeCount), sizeof(int));
	for (const auto& viewMode : animatorViewModes_) {
		int mode = static_cast<int>(viewMode);
		out.write(reinterpret_cast<const char*>(&mode), sizeof(int));
	}
}

void Animator2DComponent::LoadFromFile(std::istream& in)
{
	int animatorCount = 0;
	in.read(reinterpret_cast<char*>(&animatorCount), sizeof(int));
	for (int i = 0; i < animatorCount; ++i) {
		int nameLength = 0;
		in.read(reinterpret_cast<char*>(&nameLength), sizeof(int));
		std::string projectName(nameLength, ' ');
		in.read(&projectName[0], nameLength);
		Animator2D* animator = Animator2DManager::GetInstance()->GetAnimator2D(projectName + ".anim2d");
		animator->SetOwner(_Parent);
		_animators.push_back(animator);
		animatorNames_.push_back(projectName);
	}
	int viewModeCount = 0;
	in.read(reinterpret_cast<char*>(&viewModeCount), sizeof(int));
	for (int i = 0; i < viewModeCount; ++i) {
		int mode = 0;
		in.read(reinterpret_cast<char*>(&mode), sizeof(int));
		animatorViewModes_.push_back(static_cast<ViewMode>(mode));
	}
}