#include "EngineFrame.h"
#include "MainFrame.h"
#include "SettingManager.h"

#include "SceneManger.h"
#include "Scene.h"
#include "Object.h"
#include "_Geometry.h"
#include "GUI.h"
#include "Input.h"
#include "ChatWindow.h"
#include <nlohmann/json.hpp>
#include <set>

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
	SceneManger::GetInstance()->Init();
	SceneManger::GetInstance()->Load();
	LineRenderer::GetInstance()->Initialize();
	ChatWindow::GetInstance()->Initialize();
	ChatWindow::GetInstance()->SetVisible(true);
	LoadPrefabs();
}

void EngineFrame::Update()
{
	if (bInGame_)
	{
		if (bBeginPlayCalled_ == false)
		{
			MainFrame::GetInstance()->fixedMouseCursor(false);
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
	ChatWindow::GetInstance()->Update();
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
	ChatWindow::GetInstance()->DestroyInstance();
	prefabs_.clear();
}

bool EngineFrame::AddPrefab(AbstractObject* prefab)
{
	try
	{
		if (prefab == nullptr) return false;
		AbstractObject* Copy = prefab->Clone();
		Copy->SetParentScene(nullptr);
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

	for (auto& prefab : prefabs_) {
		if (prefab) {
			SaveObjects.push_back(prefab);
			std::function<void(AbstractObject*)> collectChildren = [&](AbstractObject* parent) {
				for (auto child : parent->GetChildren()) {
					if (child) {
						SaveObjects.push_back(child);
						collectChildren(child);
					}
				}
				};
			collectChildren(prefab);
		}
	}

	std::map<AbstractObject*, size_t> objectToId;
	for (size_t i = 0; i < SaveObjects.size(); ++i) {
		objectToId[SaveObjects[i]] = i;
	}

	auto Now = std::chrono::system_clock::now();
	auto in_time_t = std::chrono::system_clock::to_time_t(Now);
	std::tm localtime;
	localtime_s(&localtime, &in_time_t);

	nlohmann::json SceneData;
	SceneData["SceneSettings"]["Name"] = "prefabs";

	nlohmann::json ObjectArray = nlohmann::json::array();

	for (const auto& Object : SaveObjects) {
		if (Object) {
			nlohmann::json ObjectData;
			ObjectData["ID"] = objectToId[Object];
			ObjectData["Name"] = Object->GetObjectName();
			ObjectData["Transform"]["Position"] = {
				Object->GetTransform().position.x,
				Object->GetTransform().position.y,
				Object->GetTransform().position.z
			};
			ObjectData["Transform"]["Rotation"] = {
				Object->GetTransform().rotation.x,
				Object->GetTransform().rotation.y,
				Object->GetTransform().rotation.z
			};
			ObjectData["Transform"]["Scale"] = {
				Object->GetTransform().scale.x,
				Object->GetTransform().scale.y,
				Object->GetTransform().scale.z
			};

			if (Object->GetParent()) {
				ObjectData["ParentID"] = objectToId[Object->GetParent()];
			}
			else {
				ObjectData["ParentID"] = -1;
			}

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

	std::string File = SettingManager::GetInstance()->GetSceneFilePath() + "Prefab.meta";
	std::ofstream outFile(File);
	if (outFile.is_open()) {
		outFile << SceneData.dump(4);
		outFile.close();
	}
	else {
		MessageBox(nullptr, ("Prefabファイルの保存に失敗:   " + File).c_str(), "Error", MB_OK);
	}
}

void EngineFrame::LoadPrefabs()
{
	std::string filePath = SettingManager::GetInstance()->GetSceneFilePath() + "Prefab.meta";
	std::ifstream inFile(filePath);

	if (!inFile.is_open()) {
		return;
	}

	try {
		nlohmann::json sceneData;
		inFile >> sceneData;
		inFile.close();

		if (!sceneData.contains("Objects")) {
			MessageBox(nullptr, "Prefabファイルが不正です(Objects が含まれません)", "Error", MB_OK);
			return;
		}

		std::map<size_t, AbstractObject*> idToObject;
		std::map<size_t, int> childToParentId;

		for (const auto& objData : sceneData["Objects"]) {
			AbstractObject* newObj = new AbstractObject();
			newObj->SetParentScene(nullptr);

			size_t objId = objData["ID"].get<size_t>();
			std::string objName = objData["Name"].get<std::string>();
			newObj->SetObjectName(objName);

			auto pos = objData["Transform"]["Position"];
			auto rot = objData["Transform"]["Rotation"];
			auto scl = objData["Transform"]["Scale"];
			Transform transform;
			transform.position = { pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>() };
			transform.rotation = { rot[0].get<float>(), rot[1].get<float>(), rot[2].get<float>() };
			transform.scale = { scl[0].get<float>(), scl[1].get<float>(), scl[2].get<float>() };
			newObj->SetTransform(transform);

			int parentId = objData["ParentID"].get<int>();
			if (parentId != -1) {
				childToParentId[objId] = parentId;
			}

			if (objData.contains("Components")) {
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
				}
			}

			idToObject[objId] = newObj;
		}

		for (const auto& pair : childToParentId) {
			size_t childId = pair.first;
			size_t parentId = pair.second;

			auto childIt = idToObject.find(childId);
			auto parentIt = idToObject.find(parentId);

			if (childIt != idToObject.end() && parentIt != idToObject.end()) {
				childIt->second->SetParent(parentIt->second);
			}
		}

		for (const auto& pair : idToObject) {
			AbstractObject* obj = pair.second;
			if (obj && !obj->GetParent()) {
				prefabs_.push_back(obj);
			}
		}
	}
	catch (const std::exception& e) {
		MessageBox(nullptr, ("Prefab読み込みエラー:   " + std::string(e.what())).c_str(), "Error", MB_OK);
		inFile.close();
	}
	catch (...) {
		MessageBox(nullptr, "Prefab読み込み中に不明なエラーが発生しました", "Error", MB_OK);
		inFile.close();
	}
}