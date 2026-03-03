// シーン管理クラス
#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

class AbstractScene;

class SceneManger
{
private:
	struct SceneInfo {
		nlohmann::json data;
	};
public:
	static SceneManger* GetInstance();
	static void DestroyInstance();

public:
	void Init();
	void BeginPlay();
	void EditUpdate();
	void PlayUpdate();
	void Draw(int Layer);
	void ChangeScene(std::string SceneName);

	bool RenameFileInDirectory(const std::string& oldName, const std::string& newName);
	std::vector<std::string> GetSceneList() { return _sceneList; }

	void Save();
	void Load();

	AbstractScene* GetCurrentScene() { return _currentScene; }
	bool CreateAndRegisterScene(std::string SceneName);

private:
	bool CreateAndRegisterDefaultScene(std::string SceneName);
	void RegisterScene(std::string Name, std::function<AbstractScene* ()> creator);
	std::vector<std::string> ListSceneFiles();

private:
	SceneManger();
	~SceneManger();

private:
	// シーンリスト
	std::vector<std::string> _sceneList;
	std::map<std::string, std::function<AbstractScene* ()>> _SceneCreators;

	AbstractScene* _currentScene = nullptr;
	AbstractScene* _nextScene = nullptr;

	std::string _StartSceneName;

	static SceneManger* instance;

	DWORD _AutoSaveCurrentTime;
	DWORD _AutoNowTime;
};
