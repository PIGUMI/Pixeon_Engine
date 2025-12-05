#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "System.h"
#include "File.h"
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

	ImGui::InputFloat(ShiftJISToUTF8("開始時間").c_str(), &SelectedKeyFrame->StartTime, 0.1f, 1.0f, "%.2f");
	ImGui::InputFloat(ShiftJISToUTF8("終了時間").c_str(), &SelectedKeyFrame->EndTime, 0.1f, 1.0f, "%.2f");
	ImGui::InputInt(ShiftJISToUTF8("レイヤー").c_str(), &SelectedKeyFrame->Layer);
	DirectX::XMFLOAT2 startPos = { SelectedKeyFrame->StartTransform.Position.x, SelectedKeyFrame->StartTransform.Position.y };
	ImGui::InputFloat2(ShiftJISToUTF8("開始位置").c_str(), &startPos.x);
	SelectedKeyFrame->StartTransform.Position.x = startPos.x;
	SelectedKeyFrame->StartTransform.Position.y = startPos.y;
	DirectX::XMFLOAT2 endPos = { SelectedKeyFrame->EndTransform.Position.x, SelectedKeyFrame->EndTransform.Position.y };
	ImGui::InputFloat2(ShiftJISToUTF8("終了位置").c_str(), &endPos.x);
	SelectedKeyFrame->EndTransform.Position.x = endPos.x;
	SelectedKeyFrame->EndTransform.Position.y = endPos.y;
	DirectX::XMFLOAT2 startScale = { SelectedKeyFrame->StartTransform.Scale.x, SelectedKeyFrame->StartTransform.Scale.y };
	ImGui::InputFloat2(ShiftJISToUTF8("開始スケール").c_str(), &startScale.x);
	SelectedKeyFrame->StartTransform.Scale.x = startScale.x;
	SelectedKeyFrame->StartTransform.Scale.y = startScale.y;
	DirectX::XMFLOAT2 endScale = { SelectedKeyFrame->EndTransform.Scale.x, SelectedKeyFrame->EndTransform.Scale.y };
	ImGui::InputFloat2(ShiftJISToUTF8("終了スケール").c_str(), &endScale.x);
	SelectedKeyFrame->EndTransform.Scale.x = endScale.x;
	SelectedKeyFrame->EndTransform.Scale.y = endScale.y;
	DirectX::XMFLOAT2 startRot = { SelectedKeyFrame->StartTransform.Rotation.x, SelectedKeyFrame->StartTransform.Rotation.y };
	ImGui::InputFloat2(ShiftJISToUTF8("開始回転").c_str(), &startRot.x);
	SelectedKeyFrame->StartTransform.Rotation.x = startRot.x;
	SelectedKeyFrame->StartTransform.Rotation.y = startRot.y;
	DirectX::XMFLOAT2 endRot = { SelectedKeyFrame->EndTransform.Rotation.x, SelectedKeyFrame->EndTransform.Rotation.y };
	ImGui::InputFloat2(ShiftJISToUTF8("終了回転").c_str(), &endRot.x);
	SelectedKeyFrame->EndTransform.Rotation.x = endRot.x;
	SelectedKeyFrame->EndTransform.Rotation.y = endRot.y;

	//テクスチャの設定
	char TexturePath[256] = {};
	// 入力
	ImGui::InputText(ShiftJISToUTF8("テクスチャパス").c_str(), TexturePath, sizeof(TexturePath));
	if (ImGui::Button(ShiftJISToUTF8("適用").c_str()))
	{
		SelectedKeyFrame->Image->SetTextureName(TexturePath);
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


