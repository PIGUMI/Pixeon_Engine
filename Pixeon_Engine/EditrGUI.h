//エディタ用GUIクラス

#pragma once
#include <string>
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include <d3d11.h>
#include <wincodec.h>
#include <iostream>
#include <filesystem>

class Object;

class EditrGUI
{
public:
	static EditrGUI* GetInstance();
	static void DestroyInstance();

	void Init();
	void Update();
	void Draw();

public:
	std::string ShiftJISToUTF8(const std::string& str);
	ImTextureID GetAssetIcon(EditrGUI* gui, const std::string& name);
	void WriteLog(std::string Log);
	void WriteLogBuffer();
private:
	void WindowGUI();

	void ShowContentDrawer();
	void ShowHierarchy();
	void ShowInspector();
	void ShowGameView();
	void ShowConsole();

	void ShowSceneCreateWindow();
	void ShaderListWindow();
	void ExternalToolsWindow();
	void SettingWindow();
	void ShaderEditorWindow();


	void AssetManagerWindow();
	void TextureManagerWindow();
	void ModelManagerWindow();
	void SoundManagerWindow();
	void ShowLicenseWindow();
	void HandleAssetClick(const std::filesystem::path& path);
	void HandleAssetContextMenu(const std::filesystem::path& path);
	void ShowSceneRenameWindow();
	void ShowInputDebug();


	bool ShowLicense				= false;
	bool dockNeedsReset				= false;
	bool ShowSettingsWindow			= false;
	bool ShowConsoleWindow			= false;
	bool ShowArchiveWindow			= false;
	bool ShowExternalToolsWindow	= false;
	bool ShowShaderEditorWindow		= false;
	bool ShowShaderListWindow		= false;
	bool ShowAssetManagerWindow		= false;
	bool ShowTextureManagerWindow	= false;
	bool ShowModelManagerWindow		= false;
	bool ShowSoundManagerWindow		= false;
	bool ShowSceneCreate			= false;
	bool ShowSceneRename			= false;
	bool ShowInPutDebug				= false;

private:
	static ID3D11ShaderResourceView* LoadImg(const std::wstring& filename, ID3D11Device* device);
private:
	ID3D11ShaderResourceView* img;
	ID3D11ShaderResourceView* Sound;
	ID3D11ShaderResourceView* fbx;
	ID3D11ShaderResourceView* sceneIcon;
	ID3D11ShaderResourceView* folderIcon;
	ID3D11ShaderResourceView* shaderIcon;
	ID3D11ShaderResourceView* scriptIcon;
	ID3D11ShaderResourceView* JsonIcon;
	ID3D11ShaderResourceView* archiveIcon;
	ID3D11ShaderResourceView* ExeIcon;
	std::string LogBuffer;

private:
	static EditrGUI*	instance;
	Object*				SelectedObject = nullptr;
	std::string			SceneRenameNewName_;
private:
	EditrGUI() {}
	~EditrGUI() {}
};

