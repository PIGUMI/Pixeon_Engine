#pragma once
// Editor GUI Class
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include <string>
#include <d3d11.h>
#include <wincodec.h>
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
	ID3D11ShaderResourceView* LoadImg(const std::wstring& filename, ID3D11Device* device);
	std::string ShiftJISToUTF8(const std::string& str);
private:
	void MainMenuBar();
	void SceneCreateWindow();
	void ShaderListWindow();
	void AssetsManagerWindow();
	void TextureManagerWindow();
	void ModelManagerWindow();
	void SoundManagerWindow();
	void SettingWindow();
	void InputDebugWindow();
	void ShaderEditorWindow();
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
private:
	GUI() = default;
	~GUI() = default;
private:
	static GUI* instance;
};

