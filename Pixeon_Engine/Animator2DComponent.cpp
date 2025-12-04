#include "Animator2DComponent.h"
#include "Animator2D.h"
#include "ImageRender.h"
#include "EditrGUI.h"
#include <string>

void Animator2DComponent::Init(Object* Prt)
{
	_Parent = Prt;
	_ComponentName = "Animator2DComponent";
	_Type = ComponentManager::COMPONENT_TYPE::ANIMATOR2D;
	_animators.clear();

	Animator2D* Test = new Animator2D;
	KeyFrame kf;

	kf.CurveInfo.StartPoint		= { 0.0f,0.0f };
	kf.CurveInfo.EndPoint		= { 1.0f,1.0f };
	kf.CurveInfo.ControlPoint1	= { 0.0f,0.0f };
	kf.CurveInfo.ControlPoint2	= { 1.0f,1.0f };
	kf.StartTime = 0.0f;
	kf.EndTime = 1.0f;

	kf.StartTransform.Scale = { 1.0f, 1.0f };
	kf.EndTransform.Scale	= { 1.0f, 1.0f };
	kf.StartTransform.Position = { 0.0f,0.0f };
	kf.EndTransform.Position = { 0.0f,1.0f };

	kf.Layer = 1;
	kf.Image = new ImageRender();
	kf.Image->Init(Prt);
	kf.Image->SetTextureName("AlphaTest.png");
	
	//Test->SetViewMode(Animator2D::ViewMode::Billboard);
	Test->AddKeyFrame(kf);
	Test->SetTotalTime(2.0f);
	Test->SetLoop(true);

	kf.StartTransform.Position = { 0.0f,1.0f };
	kf.EndTransform.Position = { 0.0f,0.0f };
	kf.StartTime = 1.0f;
	kf.EndTime = 2.0f;

	Test->AddKeyFrame(kf);

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

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text(EditrGUI::GetInstance()->ShiftJISToUTF8("プロファイルパス").c_str());
		ImGui::TableSetColumnIndex(1);
		// パスの入力
		char Path[256] = "";
		if (ImGui::InputText(("Path##" + Ptr).c_str(), Path, sizeof(Path))) {}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text(EditrGUI::GetInstance()->ShiftJISToUTF8("読み込み").c_str());
		ImGui::TableSetColumnIndex(1);
		// 読み込みボタン
		if (ImGui::Button(("##Load" + Ptr).c_str())) {
			Animator2D* animator = new Animator2D;
			animator->LoadFile(Path);
		}

		for(auto& animator : _animators) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(EditrGUI::GetInstance()->ShiftJISToUTF8("プロジェクト名").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::Text(animator->GetProjectName().c_str());
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(EditrGUI::GetInstance()->ShiftJISToUTF8("総再生時間").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::Text(std::to_string(animator->GetTotalTime()).c_str());
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(EditrGUI::GetInstance()->ShiftJISToUTF8("ループ設定").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::Text(animator->GetLoop() ? EditrGUI::GetInstance()->ShiftJISToUTF8("有効").c_str() : EditrGUI::GetInstance()->ShiftJISToUTF8("無効").c_str());
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(EditrGUI::GetInstance()->ShiftJISToUTF8("ビュー設定").c_str());
			ImGui::TableSetColumnIndex(1);
			std::string viewModeStr = (animator->GetViewMode() == Animator2D::ViewMode::Billboard) ? "Billboard" : "Fixed";
			ImGui::Text(EditrGUI::GetInstance()->ShiftJISToUTF8(viewModeStr).c_str());
		}
	

		ImGui::EndTable();
	}
}
