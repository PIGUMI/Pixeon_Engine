#pragma once

// 新しいエンジンマネージャークラス
// エンジン機能の管理、制御を担当
// Prefabも管理

#include "GUI.h"
#include "LayerSettings.h"
#include <vector>
#include <string>
#include <filesystem>
#include <Windows.h>
#include <comdef.h>
#include <oleauto.h>

class AbstractObject;

class EngineFrame
{
public: // シングルトンパターン
	static EngineFrame* GetInstance();
	static void DestroyInstance();
public: // 4大処理
	void Init();
	void Update();
	void Draw(int Layer);
	void UnInit();
public: // GUI
	void DrawGUI();
public: // Prefab管理
	bool AddPrefab(AbstractObject* prefab);
	std::vector<AbstractObject*> GetPrefabs() { return prefabs_; }
	AbstractObject* GetPrefabByName(const std::string& name);
	void RemovePrefab(AbstractObject* ptr);
	void LoadPrefabs();
	void SavePrefabs();
public: // Getter / Setter
	bool IsInGame() const { return bInGame_; }
	void SetInGame(bool inGame) { bInGame_ = inGame; }
	bool IsShowGUI() const { return bShowGUI_; }
	void SetShowGUI(bool showGUI) { bShowGUI_ = showGUI; }
private:
	void GameViewWindow();
	void HierarchyWindow();
	void InspectorWindow();
	void ContentWindow();
	void PrefabWindow();
	void LayerWindow();
	void LayerInspectorWindow();
	void HandleAssetClick(const std::filesystem::path& path);
	void SceneRenameWindow();
	void HandleAssetContextMenu(const std::filesystem::path& path);
	void DrawObjectNode(AbstractObject* obj);
	void StatusBarWindow();
	ImTextureID GetAssetIcon(const std::string& name);
private:
	bool bInGame_ = false;
	bool bShowGUI_ = true;
	bool bBeginPlayCalled_ = false;
	std::string SceneRenameNewName_ = "";
	std::vector<AbstractObject*> prefabs_;
	bool ShowSceneRename = false;

private:
	AbstractObject* SelectedObject = nullptr;
	std::string selectedExt = "";
	std::filesystem::path currentDir;
	ID3D11ShaderResourceView* ImgIcon_;
	ID3D11ShaderResourceView* SoundIcon_;
	ID3D11ShaderResourceView* FbxIcon_;
	ID3D11ShaderResourceView* SceneIcon_;
	ID3D11ShaderResourceView* FolderIcon_;
	ID3D11ShaderResourceView* ShaderIcon_;
	ID3D11ShaderResourceView* ScriptIcon_;
	ID3D11ShaderResourceView* JsonIcon_;
	ID3D11ShaderResourceView* ArchiveIcon_;
	ID3D11ShaderResourceView* ExeIcon_;
	ID3D11ShaderResourceView* ObjectIcon_;
	Layer* selectedLayer_ = nullptr;
private:
	EngineFrame() = default;
	~EngineFrame() = default;
private:
	static EngineFrame* instance;
};
