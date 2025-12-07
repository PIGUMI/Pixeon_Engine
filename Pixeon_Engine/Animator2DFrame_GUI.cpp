#include "Animator2DFrame.h"
#include "MainFrame.h"
#include "GUI.h"
#include "Input.h"

#include "TimelineEditor.h"

void Animator2DFrame::DrawGUI()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGuiID DockSpace = ImGui::GetID("Animator2DFrameDockSpace");
	ImGui::DockSpace(DockSpace, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

	static bool Animator2DFrame_dock_init = false;
	if (!Animator2DFrame_dock_init)
	{
		Animator2DFrame_dock_init = true;

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
		ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.3f, &dock_id_bottom, &dock_main_id);
		// 左にHierarchy
		ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.18f, &dock_id_left, &dock_main_id);

		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("Animator2DTimeLine").c_str(), dock_id_bottom);
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("KeyFrameEditor").c_str(), dock_id_right);
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("Animator2DView").c_str(), dock_main_id);
		ImGui::DockBuilderFinish(DockSpace);
	}

	ImGui::End();
	ImGui::PopStyleVar();

	//if(animator_)animator_->Debug();

	DrawTimeline();
	DrawView();
	DrawKeyFrameEditor();
}

void Animator2DFrame::DrawTimeline()
{
	if (ImGui::Begin(GUI::GetInstance()->ShiftJISToUTF8("Animator2DTimeLine").c_str()))
	{
		if (timelineEditor_)
		{
			timelineEditor_->DrawTimeline(animator_);
		}
		ImGui::End();
	}
	if (timelineEditor_ && timelineEditor_->ConsumeOpenProjectPopupRequest()) {
		ImGui::OpenPopup("LoadProject");
	}

	// モーダル描画（必ず End() の外で実行）
	if (timelineEditor_) {
		timelineEditor_->DrawProjectLoadPopup();
	}
}

void Animator2DFrame::DrawView()
{
	ImGui::Begin(GUI::GetInstance()->ShiftJISToUTF8("Animator2DView").c_str(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
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
	ImGui::End();
}

void Animator2DFrame::DrawKeyFrameEditor()
{
	ImGui::Begin(GUI::GetInstance()->ShiftJISToUTF8("KeyFrameEditor").c_str(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

	if (!animator_ || !selectedKeyFrame_)
	{
		ImGui::End();
		return;
	}

	ImGui::InputFloat(GUI::GetInstance()->ShiftJISToUTF8("開始時間").c_str(), &selectedKeyFrame_->StartTime, 0.1f, 1.0f, "%.2f");
	ImGui::InputFloat(GUI::GetInstance()->ShiftJISToUTF8("終了時間").c_str(), &selectedKeyFrame_->EndTime, 0.1f, 1.0f, "%.2f");
	ImGui::InputInt(GUI::GetInstance()->ShiftJISToUTF8("レイヤー").c_str(), &selectedKeyFrame_->Layer);
	DirectX::XMFLOAT2 startPos = { selectedKeyFrame_->StartTransform.Position.x, selectedKeyFrame_->StartTransform.Position.y };
	ImGui::InputFloat2(GUI::GetInstance()->ShiftJISToUTF8("開始位置").c_str(), &startPos.x);
	selectedKeyFrame_->StartTransform.Position.x = startPos.x;
	selectedKeyFrame_->StartTransform.Position.y = startPos.y;
	DirectX::XMFLOAT2 endPos = { selectedKeyFrame_->EndTransform.Position.x, selectedKeyFrame_->EndTransform.Position.y };
	ImGui::InputFloat2(GUI::GetInstance()->ShiftJISToUTF8("終了位置").c_str(), &endPos.x);
	selectedKeyFrame_->EndTransform.Position.x = endPos.x;
	selectedKeyFrame_->EndTransform.Position.y = endPos.y;
	DirectX::XMFLOAT2 startScale = { selectedKeyFrame_->StartTransform.Scale.x, selectedKeyFrame_->StartTransform.Scale.y };
	ImGui::InputFloat2(GUI::GetInstance()->ShiftJISToUTF8("開始スケール").c_str(), &startScale.x);
	selectedKeyFrame_->StartTransform.Scale.x = startScale.x;
	selectedKeyFrame_->StartTransform.Scale.y = startScale.y;
	DirectX::XMFLOAT2 endScale = { selectedKeyFrame_->EndTransform.Scale.x, selectedKeyFrame_->EndTransform.Scale.y };
	ImGui::InputFloat2(GUI::GetInstance()->ShiftJISToUTF8("終了スケール").c_str(), &endScale.x);
	selectedKeyFrame_->EndTransform.Scale.x = endScale.x;
	selectedKeyFrame_->EndTransform.Scale.y = endScale.y;
	DirectX::XMFLOAT2 startRot = { selectedKeyFrame_->StartTransform.Rotation.x, selectedKeyFrame_->StartTransform.Rotation.y };
	ImGui::InputFloat2(GUI::GetInstance()->ShiftJISToUTF8("開始回転").c_str(), &startRot.x);
	selectedKeyFrame_->StartTransform.Rotation.x = startRot.x;
	selectedKeyFrame_->StartTransform.Rotation.y = startRot.y;
	DirectX::XMFLOAT2 endRot = { selectedKeyFrame_->EndTransform.Rotation.x, selectedKeyFrame_->EndTransform.Rotation.y };
	ImGui::InputFloat2(GUI::GetInstance()->ShiftJISToUTF8("終了回転").c_str(), &endRot.x);
	selectedKeyFrame_->EndTransform.Rotation.x = endRot.x;
	selectedKeyFrame_->EndTransform.Rotation.y = endRot.y;

	DirectX::XMFLOAT4 startColor = selectedKeyFrame_->Image->GetColor();
	ImGui::ColorEdit4(GUI::GetInstance()->ShiftJISToUTF8("開始カラー").c_str(), &startColor.x);
	selectedKeyFrame_->Image->SetColor(startColor);

	//テクスチャの設定
	static char TexturePath[256] = {};
	// 入力
	ImGui::InputText(GUI::GetInstance()->ShiftJISToUTF8("テクスチャパス").c_str(), TexturePath, sizeof(TexturePath));
	if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("適用").c_str()))
	{
		selectedKeyFrame_->Texture = TexturePath;
		selectedKeyFrame_->Image->SetTextureName(TexturePath);
	}

	if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("削除").c_str()) || IsKeyTrigger(VK_DELETE))
	{
		animator_->RemoveKeyFrame(selectedKeyFrame_);
		animator_ = nullptr;
	}
	ImGui::End();
}