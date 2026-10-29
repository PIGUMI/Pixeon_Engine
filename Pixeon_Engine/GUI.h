#pragma once
// Editor GUI Class
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "RHI/RHI.h"
#include <string>
#include <iostream>
#include <filesystem>

class GUI
{
public: // Singleton Pattern
	static GUI* GetInstance();
	static void DestroyInstance();
public:
	void Init();
	void BeginDraw();
	void EndDraw();
public:
	std::string ShiftJISToUTF8(const std::string& str);
	ImTextureID TextureIdGet(RHI::Texture* texture);
private:
	void Shutdown();
	void MainMenuBar();
	void SceneCreateWindow();
	void ShaderListWindow();
	void AssetsManagerWindow();
	void TextureManagerWindow();
	void ModelManagerWindow();
	void SoundManagerWindow();
	void EffectManagerWindow();
	void SettingWindow();
	void InputDebugWindow();
	void ShaderEditorWindow();
	void AISettingWindow();
public:
	bool bSceneCreateWindow_;
	bool bShaderListWindow_;
	bool bAssetsManagerWindow_;
	bool bTextureManagerWindow_;
	bool bModelManagerWindow_;
	bool bSoundManagerWindow_;
	bool bSettingWindow_;
	bool bInputDebugWindow_;
	bool bShaderEditorWindow_;
	bool bEffectManagerWindow_;
	bool bAISettingWindow_;
	bool bBuildWindow_ = false;
private:
	GUI() = default;
	~GUI() = default;
private:
	static GUI* instance;
};
