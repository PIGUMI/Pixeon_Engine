#include "SceneManger.h"
#include "Scene.h"
#include "SettingManager.h"
#include "AssetManager.h"
#include "MainFrame.h"

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

void SceneManger::Init() {
	_StartSceneName = "SampleScene";
	_sceneList = ListSceneFiles();
	for (const auto& fileName : _sceneList)
	{
		std::string sceneName = fileName.substr(0, fileName.find_last_of('.'));
		if (CreateAndRegisterDefaultScene(sceneName)) {
		}
		else {
		}
	}
	ChangeScene(_StartSceneName);
}

// シーン開始
void SceneManger::BeginPlay() {
	if (_currentScene) {
		_currentScene->SaveToFile();
		_currentScene->BeginPlay();
	}
}

// 更新
void SceneManger::EditUpdate() {
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
	DWORD nowTime = (DWORD)GetTickCount64();
	_AutoNowTime = SettingManager::GetInstance()->GetAutoSaveInterval() * 1000;
	if (nowTime - _AutoSaveCurrentTime >= _AutoNowTime) {
		Save();
		_AutoSaveCurrentTime = nowTime;
	}
}

// 更新
void SceneManger::PlayUpdate() {
	if (_nextScene) {
		if (_currentScene)delete _currentScene;
		_currentScene = _nextScene;
		_nextScene = nullptr;
		_currentScene->Init();
		_currentScene->LoadToFile();
		_currentScene->EditUpdate();
		_currentScene->BeginPlay();
	}
	if (_currentScene)_currentScene->PlayUpdate();
}

// 描画
void SceneManger::Draw(int Layer) {
	if (_currentScene)_currentScene->Draw(Layer);
}

// シーンの変更
void SceneManger::ChangeScene(std::string SceneName) {
	auto it = _SceneCreators.find(SceneName);

	if (it != _SceneCreators.end()) {
		_nextScene = it->second();
	}
}

// 新規シーンの作成と登録 作成時にセーブを行う
bool SceneManger::CreateAndRegisterScene(std::string SceneName) {
	for (const auto& Name : _sceneList) {
		if (Name == SceneName) {
			return false;
		}
	}

	RegisterScene(SceneName, [SceneName]() -> AbstractScene* {
		AbstractScene* newScene = new AbstractScene();
		newScene->SetName(SceneName);
		return newScene;
		});

	AbstractScene* Scene = new AbstractScene();
	Scene->SetName(SceneName);
	Scene->SaveToFile();
	delete Scene;
	Scene = nullptr;

	return true;
}
// シーンの作成と登録　セーブは行わない
bool SceneManger::CreateAndRegisterDefaultScene(std::string SceneName)
{
	for (const auto& Name : _sceneList) {
		if (Name == SceneName) {
			return false;
		}
	}

	RegisterScene(SceneName, [SceneName]() -> AbstractScene* {
		AbstractScene* newScene = new AbstractScene();
		newScene->SetName(SceneName);
		return newScene;
		});

	return true;
}

// シーンの登録
void SceneManger::RegisterScene(std::string Name, std::function<AbstractScene* ()> creator) {
	_SceneCreators[Name] = creator;
}

/*
* 関数名　ListSceneFiles
* 引　数　なし
* 戻り値　シーンファイル名（"Name.scene"）の一覧
* 説　明　仮想ファイルシステムの scene/ 以下から .scene を列挙する
*         （以前は FindFirstFile で直接ディレクトリを走査していたため、ビルド後のゲームで使えなかった）
*/
std::vector<std::string> SceneManger::ListSceneFiles() {
	std::vector<std::string> sceneFiles;
	const std::string prefix = VfsPrefix::SCENE;
	for (auto& info : AssetManager::Instance()->FileListGet(prefix)) {
		if (AssetTypeUtil::TypeFromPath(info.path) != AssetType::Scene) continue;
		const std::string rel = info.path.substr(prefix.size());
		if (rel.find('/') != std::string::npos) continue;  // サブディレクトリは対象外
		sceneFiles.push_back(rel);
	}

	if (sceneFiles.empty() && MainFrame::GetInstance()->IsEditorMode()) {
		MessageBox(nullptr, "シーンが見つからないため\n自動作成を行います", "Info", MB_OK);
		if (!CreateAndRegisterScene("SampleScene")) {
			MessageBox(nullptr, "正常に作成ができませんでした", "Error", MB_OK);
		}
		_StartSceneName = "SampleScene";
	}
	return sceneFiles;
}

SceneManger::SceneManger()
{
}

SceneManger::~SceneManger() {
	if (_currentScene) {
		delete _currentScene;
		_currentScene = nullptr;
	}
}

// ファイル名の変更
bool SceneManger::RenameFileInDirectory(const std::string& oldName, const std::string& newName) {
	std::string oldPath = SettingManager::GetInstance()->GetSceneFilePath() + oldName;
	std::string newPath = SettingManager::GetInstance()->GetSceneFilePath() + newName;
	oldPath += ".Scene";
	newPath += ".Scene";
	if (std::rename(oldPath.c_str(), newPath.c_str()) != 0) {
		return false;
	}

	return true;
}

void SceneManger::Save() {
	if (!MainFrame::GetInstance()->IsEditorMode()) return;
	if (_currentScene) {
		_currentScene->SaveToFile();
	}
	/*  ビルド処理を制作する際に処理を変えてください */
	nlohmann::json configJson;
	if (_currentScene)
	{
		configJson["StartScene"] = _currentScene->GetName();
	}
	else
	{
		configJson["StartScene"] = _StartSceneName;
	}

	std::string PATH = SettingManager::GetInstance()->GetSceneFilePath();
	PATH += "SceneConfig.sceneconfig";

	// JSONファイルに書き込む
	std::ofstream configFile(PATH);
	if (configFile.is_open())
	{
		configFile << configJson.dump(4);
	}
}

void SceneManger::Load() {
	// 起動設定（ビルド済みゲームの game.json など）で開始シーンが指定されていれば優先する
	const std::string& overrideScene = MainFrame::GetInstance()->GetStartSceneOverride();
	if (!overrideScene.empty()) {
		_StartSceneName = overrideScene;
		ChangeScene(_StartSceneName);
		return;
	}

	std::string text;
	if (!AssetManager::Instance()->TextRead(std::string(VfsPrefix::SCENE) + "SceneConfig.sceneconfig", text)) {
		if (MainFrame::GetInstance()->IsEditorMode())
			MessageBox(nullptr, "シーン設定ファイルが見つからないため\nデフォルトのシーンを読み込みます", "Info", MB_OK);
		_StartSceneName = "SampleScene";
		return;
	}
	nlohmann::json configJson = nlohmann::json::parse(text, nullptr, false);
	if (configJson.is_object() && configJson.contains("StartScene")) {
		_StartSceneName = configJson["StartScene"].get<std::string>();
	}

	ChangeScene(_StartSceneName);
}
