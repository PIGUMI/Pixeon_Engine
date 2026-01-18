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
	ID3D11ShaderResourceView* srv = MainFrame::GetInstance()->GetFinalRenderTargetSRV();
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

		/* KeyFrame名設定 */
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("KeyFrame名").c_str());
		ImGui::TableSetColumnIndex(1);
		char nameBuffer[256];
		strcpy_s(nameBuffer, selectedKeyFrame_->KeyFrameName.c_str());
		if (ImGui::InputText(GUI::GetInstance()->ShiftJISToUTF8("##KeyFrameName").c_str(), nameBuffer, sizeof(nameBuffer)))
		{
			selectedKeyFrame_->KeyFrameName = std::string(nameBuffer);
		}

		/* 位置設定 */
		if (!selectedKeyFrame_->editorFlag.bPosition)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); 
			ImGui::Text(SJ("位置").c_str());
			ImGui::SameLine();
			ImGui::Text(SJ("X:Y").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::DragFloat2(GUI::GetInstance()->ShiftJISToUTF8("##Position").c_str(), &selectedKeyFrame_->StartTransform.Position.x);
			selectedKeyFrame_->EndTransform.Position = selectedKeyFrame_->StartTransform.Position;
			ImGui::SameLine();
			if(ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("詳細展開##pos").c_str()))
			{
				selectedKeyFrame_->editorFlag.bPosition = true;
			}
		}
		else
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(SJ("位置:Start X:Y").c_str());
			ImGui::Text(SJ("位置:End X:Y").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::DragFloat2(GUI::GetInstance()->ShiftJISToUTF8("##StartPosition").c_str(), &selectedKeyFrame_->StartTransform.Position.x);
			ImGui::DragFloat2(GUI::GetInstance()->ShiftJISToUTF8("##EndPosition").c_str(), &selectedKeyFrame_->EndTransform.Position.x);
			ImGui::SameLine();
			if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("簡易表示##pos").c_str()))
			{
				selectedKeyFrame_->editorFlag.bPosition = false;
				selectedKeyFrame_->StartTransform.Position = selectedKeyFrame_->EndTransform.Position;
			}
		}

		/* 回転 */
		if (!selectedKeyFrame_->editorFlag.bRotation)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("回転").c_str());
			ImGui::SameLine();
			ImGui::Text(SJ("X:Y").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::DragFloat2(GUI::GetInstance()->ShiftJISToUTF8("##Rotation").c_str(), &selectedKeyFrame_->StartTransform.Rotation.x);
			selectedKeyFrame_->EndTransform.Rotation = selectedKeyFrame_->StartTransform.Rotation;
			ImGui::SameLine();
			if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("詳細展開##rot").c_str()))
			{
				selectedKeyFrame_->editorFlag.bRotation = true;
			}
		}
		else
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(SJ("回転:Start X:Y").c_str());
			ImGui::Text(SJ("回転:End X:Y").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::DragFloat2(GUI::GetInstance()->ShiftJISToUTF8("##StartRotation").c_str(), &selectedKeyFrame_->StartTransform.Rotation.x);
			ImGui::DragFloat2(GUI::GetInstance()->ShiftJISToUTF8("##EndRotation").c_str(), &selectedKeyFrame_->EndTransform.Rotation.x);
			ImGui::SameLine();
			if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("簡易表示##rot").c_str()))
			{
				selectedKeyFrame_->editorFlag.bRotation = false;
				selectedKeyFrame_->StartTransform.Rotation = selectedKeyFrame_->EndTransform.Rotation;
			}
		}

		/* サイズ */
		if(!selectedKeyFrame_->editorFlag.bScale)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("サイズ").c_str());
			ImGui::SameLine();
			ImGui::Text(SJ("X:Y").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::DragFloat2(GUI::GetInstance()->ShiftJISToUTF8("##Scale").c_str(), &selectedKeyFrame_->StartTransform.Scale.x,0.01f);
			selectedKeyFrame_->EndTransform.Scale = selectedKeyFrame_->StartTransform.Scale;
			ImGui::SameLine();
			if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("詳細展開##scl").c_str()))
			{
				selectedKeyFrame_->editorFlag.bScale = true;
			}
		}
		else
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(SJ("サイズ:Start X:Y").c_str());
			ImGui::Text(SJ("サイズ:End X:Y").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::DragFloat2(GUI::GetInstance()->ShiftJISToUTF8("##StartScale").c_str(), &selectedKeyFrame_->StartTransform.Scale.x,0.01f);
			ImGui::DragFloat2(GUI::GetInstance()->ShiftJISToUTF8("##EndScale").c_str(), &selectedKeyFrame_->EndTransform.Scale.x,0.01f);
			ImGui::SameLine();
			if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("簡易表示##scl").c_str()))
			{
				selectedKeyFrame_->editorFlag.bScale = false;
				selectedKeyFrame_->StartTransform.Scale = selectedKeyFrame_->EndTransform.Scale;
			}
		}

		/* vertex */
		if (!selectedKeyFrame_->editorFlag.bVertexOffset)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("頂点オフセット").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("上##VertexOffsetUp").c_str(), &selectedKeyFrame_->vertexOffset.Up,1.0f,-50.0f,50.0f);
			ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("下##VertexOffsetDown").c_str(), &selectedKeyFrame_->vertexOffset.Down, 1.0f, -50.0f, 50.0f);
			ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("左##VertexOffsetLeft").c_str(), &selectedKeyFrame_->vertexOffset.Left, 1.0f, -50.0f, 50.0f);
			ImGui::DragFloat(GUI::GetInstance()->ShiftJISToUTF8("右##VertexOffsetRight").c_str(), &selectedKeyFrame_->vertexOffset.Right, 1.0f, -50.0f, 50.0f);
			ImGui::SameLine();
		}

		/* 色の設定 */
		if (!selectedKeyFrame_->editorFlag.bColor)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("色").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::ColorEdit4(GUI::GetInstance()->ShiftJISToUTF8("##Color").c_str(), &selectedKeyFrame_->StartTransform.Color.x);
			selectedKeyFrame_->EndTransform.Color = selectedKeyFrame_->StartTransform.Color;
			ImGui::SameLine();
			if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("詳細展開##color").c_str()))
			{
				selectedKeyFrame_->editorFlag.bColor = true;
			}
		}
		else
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(SJ("色:Start RGBA").c_str());
			ImGui::Text(SJ("色:End RGBA").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::ColorEdit4(GUI::GetInstance()->ShiftJISToUTF8("##StartColor").c_str(), &selectedKeyFrame_->StartTransform.Color.x);
			ImGui::ColorEdit4(GUI::GetInstance()->ShiftJISToUTF8("##EndColor").c_str(), &selectedKeyFrame_->EndTransform.Color.x);
			ImGui::SameLine();
			if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("簡易表示##color").c_str()))
			{
				selectedKeyFrame_->editorFlag.bColor = false;
				selectedKeyFrame_->StartTransform.Color = selectedKeyFrame_->EndTransform.Color;
			}
		}

		/* UV サイズ */
		if (!selectedKeyFrame_->editorFlag.bUVScale)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("UVサイズ").c_str());
			ImGui::SameLine();
			ImGui::Text(SJ("X:Y").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::DragFloat2(GUI::GetInstance()->ShiftJISToUTF8("##UVScale").c_str(), &selectedKeyFrame_->StartTransform.UVScale.x, 0.01f, 0.0f, 1.0f);
			selectedKeyFrame_->EndTransform.UVScale = selectedKeyFrame_->StartTransform.UVScale;
			ImGui::SameLine();
			if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("詳細展開##uvscl").c_str()))
			{
				selectedKeyFrame_->editorFlag.bUVScale = true;
			}
		}
		else
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(SJ("UVサイズ:Start X:Y").c_str());
			ImGui::Text(SJ("UVサイズ:End X:Y").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::DragFloat2(GUI::GetInstance()->ShiftJISToUTF8("##StartUVScale").c_str(), &selectedKeyFrame_->StartTransform.UVScale.x, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat2(GUI::GetInstance()->ShiftJISToUTF8("##EndUVScale").c_str(), &selectedKeyFrame_->EndTransform.UVScale.x, 0.01f, 0.0f, 1.0f);
			ImGui::SameLine();
			if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("簡易表示##uvscl").c_str()))
			{
				selectedKeyFrame_->editorFlag.bUVScale = false;
				selectedKeyFrame_->StartTransform.UVScale = selectedKeyFrame_->EndTransform.UVScale;
			}
		}

		/* UV位置 */ 
		if (!selectedKeyFrame_->editorFlag.bUVPosition)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text(SJ("UV位置").c_str());
			ImGui::SameLine();
			ImGui::Text(SJ("X:Y").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::DragFloat2(GUI::GetInstance()->ShiftJISToUTF8("##UVPosition").c_str(), &selectedKeyFrame_->StartTransform.UVPosition.x,0.01f, 0.0f, 1.0f);
			selectedKeyFrame_->EndTransform.UVPosition = selectedKeyFrame_->StartTransform.UVPosition;
			ImGui::SameLine();
			if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("詳細展開##uvpos").c_str()))
			{
				selectedKeyFrame_->editorFlag.bUVPosition = true;
			}
		}
		else
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text(SJ("UV位置:Start X:Y").c_str());
			ImGui::Text(SJ("UV位置:End X:Y").c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::DragFloat2(GUI::GetInstance()->ShiftJISToUTF8("##StartUVPosition").c_str(), &selectedKeyFrame_->StartTransform.UVPosition.x, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat2(GUI::GetInstance()->ShiftJISToUTF8("##EndUVPosition").c_str(), &selectedKeyFrame_->EndTransform.UVPosition.x, 0.01f, 0.0f, 1.0f);
			ImGui::SameLine();
			if (ImGui::Button(GUI::GetInstance()->ShiftJISToUTF8("簡易表示##uvpos").c_str()))
			{
				selectedKeyFrame_->editorFlag.bUVPosition = false;
				selectedKeyFrame_->StartTransform.UVPosition = selectedKeyFrame_->EndTransform.UVPosition;
			}
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