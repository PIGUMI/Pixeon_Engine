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

	ImGui::End();
}

void EditrGUI::Animator2DViewGUI()
{
	ImGui::Begin(ShiftJISToUTF8("Animator2DView").c_str(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

	ImGui::End();
}


