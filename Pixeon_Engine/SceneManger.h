// シーン管理クラス
#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

class Scene;

class SceneManger
{
private:
	struct SceneInfo{
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
	void Draw();
	void ChangeScene(std::string SceneName);

	bool RenameFileInDirectory(const std::string& oldName, const std::string& newName);
	std::vector<std::string> GetSceneList() { return _sceneList; }

	void Save();
	void Load();

	Scene* GetCurrentScene() { return _currentScene; }
	bool CreateAndRegisterScene(std::string SceneName);

private:
	
	void RegisterScene(std::string Name, std::function<Scene* ()> creator);
	std::vector<std::string> ListSceneFiles();

private:
	SceneManger();
	~SceneManger();

private:
	// シーンリスト
	std::vector<std::string> _sceneList;
	std::map<std::string, std::function<Scene* ()>> _SceneCreators;

	Scene* _currentScene = nullptr;
	Scene* _nextScene = nullptr;

	std::string _StartSceneName;

	static SceneManger* instance;

	DWORD _AutoSaveCurrentTime;
	DWORD _AutoNowTime;
};

