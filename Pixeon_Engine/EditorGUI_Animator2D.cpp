#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "System.h"
#include "File.h"
#include "EngineManager.h"
#include "TimelineEditor.h"
#include "StartUp.h"
#include "SettingManager.h"
#include "ShaderManager.h"
#include "AssetManager.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "SoundManager.h"
#include "SceneManger.h"
#include "Scene.h"
#include "Input.h"
#include "Animator2D.h"


void EditrGUI::TimeLineEditorGUI()
{
	ImGui::Begin(ShiftJISToUTF8("Animator2DTimeLine").c_str(), nullptr,ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
	timelineEditor_->DrawTimeline(SelectedAnimator2D);
	ImGui::End();
}

void EditrGUI::KeyFrameEditorGUI()
{
	ImGui::Begin(ShiftJISToUTF8("KeyFrameEditor").c_str(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

	if (!SelectedAnimator2D || !SelectedKeyFrame)
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button(ShiftJISToUTF8("削除").c_str()) || IsKeyTrigger(VK_DELETE))
	{
		SelectedAnimator2D->RemoveKeyFrame(SelectedKeyFrame);
		SelectedKeyFrame = nullptr;
	}



	ImGui::End();
}

void EditrGUI::Animator2DViewGUI()
{
	ImGui::Begin(ShiftJISToUTF8("Animator2DView").c_str(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
	ID3D11ShaderResourceView* srv = EngineManager::GetInstance()->GetGameRender();
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


