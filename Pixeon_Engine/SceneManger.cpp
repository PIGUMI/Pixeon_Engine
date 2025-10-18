#include "SceneManger.h"
#include "Scene.h"
#include "SettingManager.h"

SceneManger* SceneManger::instance = nullptr;

SceneManger* SceneManger::GetInstance()
{
	if (instance == nullptr)
	{
		instance = new SceneManger();
	}
	return instance;
}

void SceneManger::DestroyInstance()
{
	if (instance != nullptr)
	{
		delete instance;
		instance = nullptr;
	}
}

void SceneManger::Init(){
	_StartSceneName = "SampleScene";
	_sceneList = ListSceneFiles();
	for (const auto& fileName : _sceneList)
	{
		std::string sceneName = fileName.substr(0, fileName.find_last_of('.'));
		if (CreateAndRegisterScene(sceneName)){
		}
		else{
		}
	}
	ChangeScene(_StartSceneName);
}

// シーン開始
void SceneManger::BeginPlay(){
	if (_currentScene) {
		_currentScene->SaveToFile();
		_currentScene->BeginPlay();
	}
}

// 更新
void SceneManger::EditUpdate(){
	//// シーンの切り替え
	if (_nextScene) {
		if (_currentScene)delete _currentScene;
		_currentScene = _nextScene;
		_currentScene->Init();
		_currentScene->LoadToFile();
		_nextScene = nullptr;
	}
	//// 更新
	if (_currentScene)_currentScene->EditUpdate();

	// オートセーブの処理
	DWORD nowTime = GetTickCount64();
	_AutoNowTime = SettingManager::GetInstance()->GetAutoSaveInterval() * 1000;
	if (nowTime - _AutoSaveCurrentTime >= _AutoNowTime) {
		Save();
		_AutoSaveCurrentTime = nowTime;
	}
}

// 更新
void SceneManger::PlayUpdate(){
	if (_nextScene) {
		if (_currentScene)delete _currentScene;
		_currentScene = _nextScene;
		_nextScene = nullptr;
		_currentScene->Init();
		_currentScene->LoadToFile();
		_currentScene->BeginPlay();
	}
	if (_currentScene)_currentScene->PlayUpdate();
}

// 描画
void SceneManger::Draw(){
	if (_currentScene)_currentScene->Draw();
}

// シーンの変更
void SceneManger::ChangeScene(std::string SceneName){
	auto it = _SceneCreators.find(SceneName);
	
	if (it != _SceneCreators.end()) {
		_nextScene = it->second();
	}
}

// 新規シーンの作成と登録
bool SceneManger::CreateAndRegisterScene(std::string SceneName) {

	for (const auto& Name : _sceneList){
		if (Name == SceneName){
			return false;
		}
	}

	RegisterScene(SceneName, [SceneName]() -> Scene* {
		Scene* newScene = new Scene();
		newScene->SetName(SceneName);
		return newScene;
	});

	Scene* scene = new Scene();
	scene->SetName(SceneName);
	scene->SaveToFile();
	delete scene;
	scene = nullptr;

	return true;
}

// シーンの登録
void SceneManger::RegisterScene(std::string Name, std::function<Scene* ()> creator){
	_SceneCreators[Name] = creator;
}

// ファイル名の取得
std::vector<std::string> SceneManger::ListSceneFiles(){
	std::vector<std::string> sceneFiles;
	std::string sceneDir = SettingManager::GetInstance()->GetSceneFilePath();
	std::string searchPath = sceneDir + "\\*.scene";
	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

	if (hFind == INVALID_HANDLE_VALUE) {
		// シーンファイルが見つからない場合
		MessageBox(nullptr, "シーンが見つからないため\n自動作成を行います", "Info", MB_OK);	
		if (!CreateAndRegisterScene("SampleScene")) {
			MessageBox(nullptr, "正常に作成ができませんでした", "Error", MB_OK);
		}
		_StartSceneName = "SampleScene";
	}

	do {
		if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			sceneFiles.push_back(findData.cFileName);
		}
	} while (FindNextFileA(hFind, &findData));
	FindClose(hFind);

	return sceneFiles;
}

SceneManger::SceneManger()
{
}

SceneManger::~SceneManger(){
	if (_currentScene){
		delete _currentScene;
		_currentScene = nullptr;
	}
}

// ファイル名の変更
bool SceneManger::RenameFileInDirectory(const std::string& oldName, const std::string& newName){
	std::string oldPath = SettingManager::GetInstance()->GetSceneFilePath() + oldName;
	std::string newPath = SettingManager::GetInstance()->GetSceneFilePath() + newName;
	oldPath += ".scene";
	newPath += ".scene";
	if (std::rename(oldPath.c_str(), newPath.c_str()) != 0) {
		return false;
	}
	return true;
}

void SceneManger::Save(){
	if (_currentScene){
		_currentScene->SaveToFile();
	}
}

void SceneManger::Load(){

}
