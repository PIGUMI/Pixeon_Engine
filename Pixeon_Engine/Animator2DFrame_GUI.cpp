#include "Animator2DFrame.h"
#include "MainFrame.h"
#include "GUI.h"
#include "Input.h"
#include "AssetManager.h"
#include "TimelineEditor.h"
#include "EasingGraph.h"

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
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("Animator2DControl").c_str(), dock_id_right);
		ImGui::DockBuilderDockWindow(GUI::GetInstance()->ShiftJISToUTF8("Animator2DView").c_str(), dock_main_id);
		ImGui::DockBuilderFinish(DockSpace);
	}

	ImGui::End();
	ImGui::PopStyleVar();

	//if(animator_)animator_->Debug();

	DrawTimeline();
	DrawView();
	DrawAnimatorControl();
	DrawKeyFrameEditor();
	if (wantOpenTexturePopup_)ImGui::OpenPopup("LoadTexture");
	DrawTextureLoadPopup();
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
	auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };
	ImGui::Begin(GUI::GetInstance()->ShiftJISToUTF8("KeyFrameEditor").c_str(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
	if (!animator_ || !selectedKeyFrame_)
	{
		ImGui::End();
		return;
	}
	if (ImGui::BeginTable(SJ("KeyFrameEditor").c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
		std::string msg;

		/* 位置の設定GUI */
		if (!selectedKeyFrame_->editorFlag.bPosition)
			msg = "位置 ";
		else
			msg = "開始位置 ";
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ(msg.c_str()).c_str());
		ImGui::TableSetColumnIndex(1); ImGui::DragFloat2(SJ("##開始位置").c_str(), &selectedKeyFrame_->StartTransform.Position.x, 0.1f);
		if (selectedKeyFrame_->editorFlag.bPosition)
		{
			ImGui::TableNextRow();/*終了位置*/
			ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("終了位置").c_str());
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat2(SJ("##終了位置").c_str(), &selectedKeyFrame_->EndTransform.Position.x, 0.1f);
			ImGui::SameLine();
			if (ImGui::Button(SJ("詳細縮小##位置").c_str()))
			{
				selectedKeyFrame_->editorFlag.bPosition = false;
			}
		}
		else
		{
			ImGui::SameLine();
			if (ImGui::Button(SJ("詳細展開##位置").c_str()))
			{
				selectedKeyFrame_->editorFlag.bPosition = true;
			}
			// 終了位置を開始位置と同じにする
			selectedKeyFrame_->EndTransform.Position = selectedKeyFrame_->StartTransform.Position;
		}

		/* 回転の設定GUI */
		if (!selectedKeyFrame_->editorFlag.bRotation)
			msg = "回転 ";
		else
			msg = "開始回転 ";
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ(msg.c_str()).c_str());
		ImGui::TableSetColumnIndex(1); ImGui::DragFloat2(SJ("##開始角度").c_str(), &selectedKeyFrame_->StartTransform.Rotation.x, 0.1f);
		if (selectedKeyFrame_->editorFlag.bRotation)
		{
			ImGui::TableNextRow();/*終了角度*/
			ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("終了回転").c_str());
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat2(SJ("##終了角度").c_str(), &selectedKeyFrame_->EndTransform.Rotation.x, 0.1f);
			ImGui::SameLine();
			if (ImGui::Button(SJ("詳細縮小##回転").c_str()))
			{
				selectedKeyFrame_->editorFlag.bRotation = false;
			}
		}
		else
		{
			ImGui::SameLine();
			if (ImGui::Button(SJ("詳細展開##回転").c_str()))
			{
				selectedKeyFrame_->editorFlag.bRotation = true;
			}
			// 終了角度を開始角度と同じにする
			selectedKeyFrame_->EndTransform.Rotation = selectedKeyFrame_->StartTransform.Rotation;
		}

		/* スケールの設定GUI */
		if (!selectedKeyFrame_->editorFlag.bScale)
			msg = "スケール ";
		else
			msg = "開始スケール ";
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ(msg.c_str()).c_str());
		ImGui::TableSetColumnIndex(1); ImGui::DragFloat2(SJ("##開始スケール").c_str(), &selectedKeyFrame_->StartTransform.Scale.x, 0.1f, 0.0f);
		if (selectedKeyFrame_->editorFlag.bScale)
		{
			ImGui::TableNextRow();/*終了スケール*/
			ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("終了スケール").c_str());
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat2(SJ("##終了スケール").c_str(), &selectedKeyFrame_->EndTransform.Scale.x, 0.1f, 0.0f);
			ImGui::SameLine();
			if (ImGui::Button(SJ("詳細縮小##スケール").c_str()))
			{
				selectedKeyFrame_->editorFlag.bScale = false;
			}
		}
		else
		{
			ImGui::SameLine();
			if (ImGui::Button(SJ("詳細展開##スケール").c_str()))
			{
				selectedKeyFrame_->editorFlag.bScale = true;
			}
			// 終了スケールを開始スケールと同じにする
			selectedKeyFrame_->EndTransform.Scale = selectedKeyFrame_->StartTransform.Scale;
		}

		/* UV位置の設定GUI */
		if (!selectedKeyFrame_->editorFlag.bUVPosition)
			msg = "UV位置 ";
		else
			msg = "開始UV位置 ";
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ(msg.c_str()).c_str());
		ImGui::TableSetColumnIndex(1); ImGui::DragFloat2(SJ("##開始UV位置").c_str(), &selectedKeyFrame_->StartTransform.UVPosition.x, 0.01f);
		if (selectedKeyFrame_->editorFlag.bUVPosition)
		{
			ImGui::TableNextRow();/*終了UV位置*/
			ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("終了UV位置").c_str());
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat2(SJ("##終了UV位置").c_str(), &selectedKeyFrame_->EndTransform.UVPosition.x, 0.01f);
			ImGui::SameLine();
			if (ImGui::Button(SJ("詳細縮小##UV位置").c_str()))
			{
				selectedKeyFrame_->editorFlag.bUVPosition = false;
			}
		}
		else
		{
			ImGui::SameLine();
			if (ImGui::Button(SJ("詳細展開##UV位置").c_str()))
			{
				selectedKeyFrame_->editorFlag.bUVPosition = true;
			}
			// 終了UV位置を開始UV位置と同じにする
			selectedKeyFrame_->EndTransform.UVPosition = selectedKeyFrame_->StartTransform.UVPosition;
		}

		/* UVスケールの設定GUI */
		if (!selectedKeyFrame_->editorFlag.bUVScale)
			msg = "UVスケール ";
		else
			msg = "開始UVスケール ";
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ(msg.c_str()).c_str());
		ImGui::TableSetColumnIndex(1); ImGui::DragFloat2(SJ("##開始UVスケール").c_str(), &selectedKeyFrame_->StartTransform.UVScale.x, 0.01f, 0.0f);
		if (selectedKeyFrame_->editorFlag.bUVScale)
		{
			ImGui::TableNextRow();/*UVスケール*/
			ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("終了UVスケール").c_str());
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat2(SJ("##終了UVスケール").c_str(), &selectedKeyFrame_->EndTransform.UVScale.x, 0.01f, 0.0f);
			ImGui::SameLine();
			if (ImGui::Button(SJ("詳細縮小##UVスケール").c_str()))
			{
				selectedKeyFrame_->editorFlag.bUVScale = false;
			}
		}
		else
		{
			ImGui::SameLine();
			if (ImGui::Button(SJ("詳細展開##UVスケール").c_str()))
			{
				selectedKeyFrame_->editorFlag.bUVScale = true;
			}
			// 終了UVスケールを開始UVスケールと同じにする
			selectedKeyFrame_->EndTransform.UVScale = selectedKeyFrame_->StartTransform.UVScale;
		}

		/* テクスチャの設定GUI */
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("テクスチャ").c_str());
		ImGui::TableSetColumnIndex(1);
		if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("選択").c_str()))
		{
			wantOpenTexturePopup_ = true;
		}

		ImGui::EndTable();
	}

	/* イージングの作成 */
	ImGui::Separator();
	DrawEasingGraph(selectedKeyFrame_->CurveInfo, "Easing", { 430.0f,430.0f });
	//////////////////////

	if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("削除").c_str()) || IsKeyTrigger(VK_DELETE))
	{
		animator_->RemoveKeyFrame(selectedKeyFrame_);
		animator_ = nullptr;
	}
	ImGui::End();
}

void Animator2DFrame::DrawAnimatorControl()
{
	if (ImGui::Begin("Animator2DControl", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse)) {
		if (animator_) {
			std::string ProjectName = animator_->Name_;
			char buf[256];
			strcpy_s(buf, ProjectName.c_str());
			if (ImGui::InputText(GUI::GetInstance()->ShiftJISToUTF8("プロジェクト名").c_str(), buf, sizeof(buf))) {
				animator_->Name_ = std::string(buf);
			}
			std::string msg;
			msg = animator_->bLoop_ ? "ループ再生: 有効" : "ループ再生: 無効";
			ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8(msg).c_str(), animator_->fTotalDuration_);
			ImGui::SameLine();
			ImGui::Checkbox(GUI::GetInstance()->ShiftJISToUTF8("ループ再生").c_str(), &animator_->bLoop_);
			msg = "総再生時間: " + std::to_string(animator_->fTotalDuration_) + " 秒";
			ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8(msg).c_str(), animator_->fTotalDuration_);
			msg = "現在の再生時間: " + std::to_string(animator_->fNowTime_) + " 秒";
			ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8(msg).c_str(), animator_->fNowTime_);
			if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("プロジェクトを閉じる").c_str())) {
				delete animator_;
				animator_ = nullptr;
				isPlaying_ = false;
			}
		}
	}
	ImGui::End();
}

void Animator2DFrame::DrawTextureLoadPopup()
{
	if (ImGui::BeginPopupModal("LoadTexture", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static char filter[128] = "";
		ImGui::InputText(GUI::GetInstance()->ShiftJISToUTF8("フィルタ").c_str(), filter, sizeof(filter));
		auto texList = AssetManager::Instance()->GetCachedTextureNames();
		ImGui::BeginChild("ImgTexList", ImVec2(420, 260), true);
		for (int i = 0; i < (int)texList.size(); ++i) {
			const std::string& n = texList[i];
			if (filter[0] && n.find(filter) == std::string::npos) continue;
			if (ImGui::Selectable(n.c_str(), false)) {
				selectedKeyFrame_->Texture = n;
				wantOpenTexturePopup_ = false;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndChild();
		if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("キャンセル").c_str()))
		{
			wantOpenTexturePopup_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}