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
	static bool TLEGInit = true;
	static Animator2D* animator;
	if(TLEGInit)
	{
		animator = new Animator2D();
		TLEGInit = false;
	}
	ImGui::Begin(ShiftJISToUTF8("Animator2DTimeLine").c_str(), nullptr,ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
	timelineEditor_->DrawTimeline(animator);
	ImGui::End();
}


