#pragma once

// 新しいエンジンマネージャークラス
// エンジン機能の管理、制御を担当
// Prefabも管理

#include "GUI.h"
#include <vector>
#include <string>

class Object;

class EngineFrame
{
public: // シングルトンパターン
	static EngineFrame* GetInstance();
	static void DestroyInstance();
public: // 4大処理
	void Init();
	void Update();
	void Draw();
	void UnInit();
public: // GUI
	void DrawGUI();
public: // Prefab管理
	bool AddPrefab(Object* prefab);
	std::vector<Object*> GetPrefabs() { return prefabs_; }
	Object* GetPrefabByName(const std::string& name);
	void RemovePrefab(Object* ptr);

	void LoadPrefabs();
	void SavePrefabs();
public: // Getter / Setter
	bool IsInGame() const { return bInGame_; }
	void SetInGame(bool inGame) { bInGame_ = inGame; }
private:
	bool bInGame_ = false;
	bool bBeginPlayCalled_ = false;

	std::vector<Object*> prefabs_;

private:
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
private:
	EngineFrame() = default;
	~EngineFrame() = default;
private:
	static EngineFrame* instance;
};

