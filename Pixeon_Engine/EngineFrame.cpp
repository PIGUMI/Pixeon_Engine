#include "EngineFrame.h"
#include "MainFrame.h"
#include "SettingManager.h"

#include "SceneManger.h"
#include "Scene.h"
#include "Object.h"
#include "_Geometry.h"
#include "GUI.h"
#include "Input.h"

EngineFrame* EngineFrame::instance = nullptr;

EngineFrame* EngineFrame::GetInstance()
{
	if (instance == nullptr)
	{
		instance = new EngineFrame();
	}
	return instance;
}

void EngineFrame::DestroyInstance()
{
	if (instance != nullptr)
	{
		delete instance;
		instance = nullptr;
	}
}

void EngineFrame::Init()
{
	// SceneManager の初期化
	SceneManger::GetInstance()->Init();
	SceneManger::GetInstance()->Load();
	LineRenderer::GetInstance()->Initialize();
	LoadPrefabs();

	ImgIcon_ = GUI::GetInstance()->LoadImg(L"SceneRoot/Editor/texture/img.png", DirectX11::GetInstance()->GetDevice());
	SoundIcon_ = GUI::GetInstance()->LoadImg(L"SceneRoot/Editor/texture/Sound.png", DirectX11::GetInstance()->GetDevice());
	FolderIcon_ = GUI::GetInstance()->LoadImg(L"SceneRoot/Editor/texture/File.png", DirectX11::GetInstance()->GetDevice());
	ShaderIcon_ = GUI::GetInstance()->LoadImg(L"SceneRoot/Editor/texture/HLSL.png", DirectX11::GetInstance()->GetDevice());
	ScriptIcon_ = GUI::GetInstance()->LoadImg(L"SceneRoot/Editor/texture/Script.png", DirectX11::GetInstance()->GetDevice());
	JsonIcon_ = GUI::GetInstance()->LoadImg(L"SceneRoot/Editor/texture/Json.png", DirectX11::GetInstance()->GetDevice());
	ArchiveIcon_ = GUI::GetInstance()->LoadImg(L"SceneRoot/Editor/texture/Archive.png", DirectX11::GetInstance()->GetDevice());
	ExeIcon_ = GUI::GetInstance()->LoadImg(L"SceneRoot/Editor/texture/Exe.png", DirectX11::GetInstance()->GetDevice());
	ObjectIcon_ = GUI::GetInstance()->LoadImg(L"SceneRoot/Editor/texture/Object.png", DirectX11::GetInstance()->GetDevice());
	FbxIcon_ = GUI::GetInstance()->LoadImg(L"SceneRoot/Editor/texture/fbx.png", DirectX11::GetInstance()->GetDevice());
	SceneIcon_ = GUI::GetInstance()->LoadImg(L"SceneRoot/Editor/texture/Scene.png", DirectX11::GetInstance()->GetDevice());
}

void EngineFrame::Update()
{
	if (bInGame_)
	{
		if (bBeginPlayCalled_ == false)
		{
			SceneManger::GetInstance()->BeginPlay();
			bBeginPlayCalled_ = true;
		}
		SceneManger::GetInstance()->PlayUpdate();
	}
	else
	{
		bBeginPlayCalled_ = false;
		SceneManger::GetInstance()->EditUpdate();
	}
	if (IsKeyPress(VK_SHIFT) && IsKeyTrigger(VK_RETURN))
	{
		bShowGUI_ = !bShowGUI_;
		if (bShowGUI_)MainFrame::GetInstance()->fixedMouseCursor(false);
	}
}

void EngineFrame::Draw(int Layer)
{
	SceneManger::GetInstance()->Draw(Layer);
}

void EngineFrame::UnInit()
{
	if (ImgIcon_)
	{
		ImgIcon_->Release();
		ImgIcon_ = nullptr;
	}
	if (SoundIcon_)
	{
		SoundIcon_->Release();
		SoundIcon_ = nullptr;
	}
	if (FbxIcon_)
	{
		FbxIcon_->Release();
		FbxIcon_ = nullptr;
	}
	if (SceneIcon_)
	{
		SceneIcon_->Release();
		SceneIcon_ = nullptr;
	}
	if (FolderIcon_)
	{
		FolderIcon_->Release();
		FolderIcon_ = nullptr;
	}
	if (ShaderIcon_)
	{
		ShaderIcon_->Release();
		ShaderIcon_ = nullptr;
	}
	if (ScriptIcon_)
	{
		ScriptIcon_->Release();
		ScriptIcon_ = nullptr;
	}
	if (JsonIcon_)
	{
		JsonIcon_->Release();
		JsonIcon_ = nullptr;
	}
	if (ArchiveIcon_)
	{
		ArchiveIcon_->Release();
		ArchiveIcon_ = nullptr;
	}
	if (ExeIcon_)
	{
		ExeIcon_->Release();
		ExeIcon_ = nullptr;
	}
	if (ObjectIcon_)
	{
		ObjectIcon_->Release();
		ObjectIcon_ = nullptr;
	}
	SavePrefabs();
	for (auto prefab : prefabs_) {
		if (prefab) {
			delete prefab;
			prefab = nullptr;
		}
	}
	LineRenderer::GetInstance()->Finalize();
	SceneManger::GetInstance()->Save();
	SceneManger::DestroyInstance();
	prefabs_.clear();
}

bool EngineFrame::AddPrefab(AbstractObject* prefab)
{
	try
	{
		if (prefab == nullptr) return false;
		AbstractObject* Copy = prefab->Clone();
		Copy->SetParentScene(nullptr);
		// 同じ名前のPrefabが存在する場合、名前に番号を付与
		std::string baseName = Copy->GetObjectName();
		int count = 1;
		while (GetPrefabByName(Copy->GetObjectName())) {
			Copy->SetObjectName(baseName + std::to_string(count));
			count++;
		}
		prefabs_.push_back(Copy);
		return true;
	}
	catch (...)
	{
		MessageBox(nullptr, "Prefabの追加に失敗しました", "Error", MB_OK);
		return false;
	}
}

AbstractObject* EngineFrame::GetPrefabByName(const std::string& name)
{
	try
	{
		for (auto prefab : prefabs_) {
			if (prefab->GetObjectName() == name) {
				return prefab;
			}
		}
		return nullptr;
	}
	catch (...)
	{
		MessageBox(nullptr, "Prefabの取得に失敗しました", "Error", MB_OK);
		return nullptr;
	}
}

void EngineFrame::RemovePrefab(AbstractObject* ptr)
{
	try
	{
		prefabs_.erase(std::remove(prefabs_.begin(), prefabs_.end(), ptr), prefabs_.end());
		delete ptr;
		ptr = nullptr;
	}
	catch (...)
	{
		MessageBox(nullptr, "Prefabの削除に失敗しました", "Error", MB_OK);
	}
}

void EngineFrame::SavePrefabs()
{
	std::vector<AbstractObject*> SaveObjects;
	SaveObjects = prefabs_;
	// 現在時刻の取得
	auto Now = std::chrono::system_clock::now();
	auto in_time_t = std::chrono::system_clock::to_time_t(Now);
	std::tm localtime;
	localtime_s(&localtime, &in_time_t);

	nlohmann::json SceneData;
	SceneData["SceneSettings"]["Name"] = "prefabs";

	// オブジェクトデータの保存
	nlohmann::json ObjectArray = nlohmann::json::array();

	for (const auto& Object : SaveObjects) {
		if (Object) {
			// オブジェクトの基本情報の保存
			nlohmann::json ObjectData;
			ObjectData["Name"] = Object->GetObjectName();
			ObjectData["Transform"]["Position"] = { Object->GetTransform().position.x, Object->GetTransform().position.y, Object->GetTransform().position.z };
			ObjectData["Transform"]["Rotation"] = { Object->GetTransform().rotation.x, Object->GetTransform().rotation.y, Object->GetTransform().rotation.z };
			ObjectData["Transform"]["Scale"] = { Object->GetTransform().scale.x,    Object->GetTransform().scale.y,    Object->GetTransform().scale.z };

			// コンポーネントデータの保存
			nlohmann::json ComponentData = nlohmann::json::array();
			for (const auto& comp : Object->GetComponents()) {
				if (comp) {
					nlohmann::json CompJson;
					CompJson["Type"] = comp->GetComponentType();
					CompJson["Name"] = comp->GetComponentName();
					std::ostringstream oss;
					comp->SaveToFile(oss);
					CompJson["Data"] = oss.str();
					ComponentData.push_back(CompJson);
				}
			}
			ObjectData["Components"] = ComponentData;
			ObjectArray.push_back(ObjectData);
		}
	}
	SceneData["Objects"] = ObjectArray;

	// ファイル名の生成
	std::string File;
	File = SettingManager::GetInstance()->GetSceneFilePath() + "Prefab" + ".meta";
	std::ofstream outFile(File);
	if (outFile.is_open()) {
		outFile << SceneData.dump(4); // インデント幅4で保存
		outFile.close();
	}
}

void EngineFrame::LoadPrefabs()
{
	std::string filePath = SettingManager::GetInstance()->GetSceneFilePath() + "/" + "Prefab" + ".meta";
	std::ifstream inFile(filePath);
	if (!inFile.is_open()) {
		return;
	}

	nlohmann::json sceneData;
	inFile >> sceneData;
	inFile.close();

	// Objectsの読み込み
	for (const auto& objData : sceneData["Objects"]) {
		AbstractObject* newObj = new AbstractObject();
		newObj->SetParentScene(nullptr);
		newObj->SetObjectName(objData["Name"].get<std::string>());
		// Transformの読み込み
		auto pos = objData["Transform"]["Position"];
		auto rot = objData["Transform"]["Rotation"];
		auto scl = objData["Transform"]["Scale"];
		Transform transform;
		transform.position = { pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>() };
		transform.rotation = { rot[0].get<float>(), rot[1].get<float>(), rot[2].get<float>() };
		transform.scale = { scl[0].get<float>(), scl[1].get<float>(), scl[2].get<float>() };
		newObj->SetTransform(transform);
		// コンポーネントの読み込み
		for (const auto& compData : objData["Components"]) {
			auto type = static_cast<ComponentManager::COMPONENT_TYPE>(compData["Type"].get<int>());
			auto name = compData["Name"].get<std::string>();
			auto data = compData["Data"].get<std::string>();
			AbstractComponent* newComp = ComponentManager::GetInstance()->AddComponent(newObj, type);
			if (newComp) {
				newComp->SetComponentName(name);
				std::istringstream iss(data);
				newComp->LoadFromFile(iss);
			}
			else {
				MessageBox(nullptr, "コンポーネントの追加に失敗しました", "Error", MB_OK);
			}
		}
		prefabs_.push_back(newObj);
	}
}