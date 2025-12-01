#include "Scene.h"
#include "Object.h"
#include "Component.h"
#include "ComponentManager.h"
#include "SettingManager.h"
#include "LightComponent.h"
#include "EngineManager.h"
#include <thread>
#include <mutex>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include "System.h"
#include "RigidBody.h"

// ライト用GPU定数バッファ構造体
struct LightGPU {
	DirectX::XMFLOAT3 position;
	float intensity;
	DirectX::XMFLOAT3 direction;
	float type;      // type: 0=Dir,1=Point,2=Spot
	DirectX::XMFLOAT3 color;
	float range;
	float innerCos;
	float outerCos;
	float enabled;
	float pad; // 16B アライメント
};

// ライト用定数バッファ構造体
static ID3D11Buffer* gLightCB = nullptr;
static const int kMaxLights = 8;

// 開放処理
Scene::~Scene()
{
	SaveToFile();

	for (auto& obj : _objects) {
		if (obj) {
			obj->UInit();
			delete obj;
		}
	}
	_objects.clear();
	CleanupPhysics();
	for (auto& obj : _ToBeAdded) {
		if (obj) {
			obj->UInit();
			delete obj;
		}
	}
	_ToBeAdded.clear();
	for (auto& obj : _SaveObjects) {
		if (obj) {
			obj->UInit();
			delete obj;
		}
	}
	_SaveObjects.clear();
	if (gLightCB) {
		gLightCB->Release();
		gLightCB = nullptr;
	}
}

void Scene::Init() {
	// 動的配列の初期化
	_objects.clear();
	_ToBeAdded.clear();
	_ToBeRemoved.clear();
	_SaveObjects.clear();
	EndPlayCalled = true;
	InitPhysics();
	_collisionManager = new CollisionManager;
	_collisionManager->Initialize(pPhysicsWorld);
}

void Scene::BeginPlay() {
	for (auto& obj : _objects) {
		if (obj) {
			Object* cloneObj = obj->Clone();
			_SaveObjects.push_back(cloneObj);
		}
	}

	// 物理演算に関するコード
	if (pPhysicsWorld)
	{
		while (pPhysicsWorld->getNumCollisionObjects() > 0)
		{
			int last = pPhysicsWorld->getNumCollisionObjects() - 1;
			btCollisionObject* obj = pPhysicsWorld->getCollisionObjectArray()[last];
			pPhysicsWorld->removeCollisionObject(obj);
		}
		pPhysicsWorld->clearForces();
		pPhysicsWorld->getBroadphase()->resetPool(pPhysicsWorld->getDispatcher());
		pPhysicsWorld->getConstraintSolver()->reset();
	}
	//

	// BeginPlayを呼び出す
	for (auto& obj : _objects) {
		if (obj)obj->BeginPlay();
	}
	EndPlayCalled = false;
}

void Scene::EditUpdate() {
	if (!EndPlayCalled)
	{
		EndPlayCalled = true;
		for (auto& obj : _objects)
		{
			if (obj) obj->UInit();
			delete obj;
		}
		_objects.clear();
		_objects = _SaveObjects;
		_SaveObjects.clear();
		for (auto& obj : _objects) {
			if (!obj) continue;
			for (auto& comp : obj->GetComponents()) {
				if (!comp) continue;
				if (comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA) {
					CameraComponent* cam = dynamic_cast<CameraComponent*>(comp);
					if (cam->GetCameraNumber() == _MainCameraNumber)
					{
						_MainCamera = cam;
					}
				}
			}
		}
	}
	// 非同期追加の処理
	ProcessThreadSafeAdditions();
	// オブジェクトの追加処理
	for (auto& obj : _ToBeAdded) {
		if (obj) {
			obj->SetParentScene(this);
			_objects.push_back(obj);
		}
	}
	_ToBeAdded.clear();

	// カメラコンポーネントの更新
	int i = 0;
	for (auto& obj : _objects) {
		if (!obj) continue;
		for (auto& comp : obj->GetComponents()) {
			if (!comp) continue;
			if (comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA) {
				CameraComponent* cam = dynamic_cast<CameraComponent*>(comp);
				cam->SetCameraNumber(i);
				i++;
				comp->EditUpdate();
			}
		}
	}

	if (_MainCamera)_MainCameraNumber = _MainCamera->GetCameraNumber();
	else _MainCameraNumber = -1;

	// オブジェクトの更新
	for (auto& obj : _objects) if (obj)obj->EditUpdate();

	// オブジェクトの削除処理
	for (auto& obj : _ToBeRemoved) {
		if (!obj) continue;
		auto it = std::find(_objects.begin(), _objects.end(), obj);
		if (it != _objects.end()) {
			_objects.erase(it);
			delete obj;
		}
	}
	_ToBeRemoved.clear();
}

void Scene::PlayUpdate() {
	// 非同期追加の処理
	ProcessThreadSafeAdditions();
	// オブジェクトの追加処理
	for (auto& obj : _ToBeAdded) {
		if (obj) {
			obj->SetParentScene(this);
			obj->BeginPlay();
			_objects.push_back(obj);
		}
	}
	_ToBeAdded.clear();

	// カメラコンポーネントの更新
	int i = 0;
	for (auto& obj : _objects) {
		if (!obj) continue;
		for (auto& comp : obj->GetComponents()) {
			if (!comp) continue;
			if (comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA) {
				CameraComponent* cam = dynamic_cast<CameraComponent*>(comp);
				cam->SetCameraNumber(i);
				i++;
				comp->EditUpdate();
			}
		}
	}

	if (_MainCamera)_MainCameraNumber = _MainCamera->GetCameraNumber();
	else _MainCameraNumber = -1;

	/* 物理シュミレーションのステップ */
	if (pPhysicsWorld)
	{
		try
		{
			int numObjects = pPhysicsWorld->getNumCollisionObjects();
			if (numObjects >= 0 && numObjects < 10000)
			{
				bool hasInvakudObjects = false;
				for (int i = 0; i < numObjects; i++)
				{
					btCollisionObject* obj = pPhysicsWorld->getCollisionObjectArray()[i];
					if (!obj || !obj->getCollisionShape())
					{
						hasInvakudObjects = true;
						break;
					}
				}

				// 無効なオブジェクトがない場合のみステップを進める
				if (!hasInvakudObjects)
				{
					float timeStep = 1.0f / 60.0f;
					int maxSubSteps = 10;
					bool valid = true;
					for (int i = 0; i < pPhysicsWorld->getNumCollisionObjects(); ++i)
					{
						btCollisionObject* obj = pPhysicsWorld->getCollisionObjectArray()[i];
						btRigidBody* body = btRigidBody::upcast(obj);
						if (!body || !body->getCollisionShape() || !body->getMotionState())
						{
							valid = false;
							break;
						}
					}
					if (valid)
					{
						pPhysicsWorld->stepSimulation(timeStep, maxSubSteps);
					}
				}
				else
				{
					CleanupAndReinitializePhysics();
				}
			}
			else
			{
				CleanupAndReinitializePhysics();
			}
		}
		catch (...)
		{
			CleanupAndReinitializePhysics();
		}
	}

	// オブジェクトの更新
	for (auto& obj : _objects) if (obj)obj->InGameUpdate();

	if (_collisionManager)_collisionManager->Update();

	// オブジェクトの削除処理
	for (auto& obj : _ToBeRemoved) {
		if (!obj) continue;
		auto it = std::find(_objects.begin(), _objects.end(), obj);
		if (it != _objects.end()) {
			_objects.erase(it);
			delete obj;
		}
	}
	_ToBeRemoved.clear();
}

void Scene::Draw() {
	UploadLightsToGPU();
	// オブジェクトの描画
	std::vector<Object*> sortedList = _objects;
	if (_MainCamera) {
		std::sort(sortedList.begin(), sortedList.end(), [this](Object* a, Object* b) {
			if (!a || !b) return false;
			// カメラからの距離を計算
			DirectX::XMFLOAT3 camPos = _MainCamera->GetPosition();
			DirectX::XMFLOAT3 posA = a->GetTransform().position;
			DirectX::XMFLOAT3 posB = b->GetTransform().position;
			float distA = (camPos.x - posA.x) * (camPos.x - posA.x) + (camPos.y - posA.y) * (camPos.y - posA.y) + (camPos.z - posA.z) * (camPos.z - posA.z);
			float distB = (camPos.x - posB.x) * (camPos.x - posB.x) + (camPos.y - posB.y) * (camPos.y - posB.y) + (camPos.z - posB.z) * (camPos.z - posB.z);
			// 距離が近い順にソート
			return distA < distB;
			});
	}
	for (auto& obj : sortedList) if (obj)obj->Draw();
}

void Scene::SaveToFile() {
	std::vector<Object*> SaveObjects;
	if (EngineManager::GetInstance()->IsInGame()) {
		SaveObjects = _SaveObjects;
	}
	else {
		SaveObjects = _objects;
	}
	// 現在時刻の取得
	auto Now = std::chrono::system_clock::now();
	auto in_time_t = std::chrono::system_clock::to_time_t(Now);
	std::tm localtime;
	localtime_s(&localtime, &in_time_t);

	nlohmann::json SceneData;
	SceneData["SceneSettings"]["Name"] = _name;
	SceneData["SceneSettings"]["MainCameraNumber"] = _MainCameraNumber;

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
	File = SettingManager::GetInstance()->GetSceneFilePath() + _name + ".scene";
	std::ofstream outFile(File);
	if (outFile.is_open()) {
		outFile << SceneData.dump(4); // インデント幅4で保存
		outFile.close();
	}
}

void Scene::LoadToFile() {
	std::string filePath = SettingManager::GetInstance()->GetSceneFilePath() + "/" + _name + ".scene";
	std::ifstream inFile(filePath);
	if (!inFile.is_open()) {
		// ファイルが開けなかった場合、falseを返す
		return;
	}

	nlohmann::json sceneData;
	inFile >> sceneData;
	inFile.close();

	_name = sceneData["SceneSettings"]["Name"].get<std::string>();
	_MainCameraNumber = sceneData["SceneSettings"]["MainCameraNumber"].get<int>();

	// Objectsの読み込み
	for (const auto& objData : sceneData["Objects"]) {
		Object* newObj = new Object();
		newObj->SetParentScene(this);
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
			Component* newComp = ComponentManager::GetInstance()->AddComponent(newObj, type);
			if (newComp) {
				newComp->SetComponentName(name);
				std::istringstream iss(data);
				newComp->LoadFromFile(iss);
			}
			else {
				MessageBox(nullptr, "コンポーネントの追加に失敗しました", "Error", MB_OK);
			}
		}
		AddObjectLocal(newObj);
	}

	// 登録されているメインカメラと同じ番号のカメラコンポーネントを探す
	for (auto& obj : _ToBeAdded) {
		if (!obj) continue;
		for (auto& comp : obj->GetComponents()) {
			if (!comp) continue;
			if (comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA) {
				CameraComponent* cam = dynamic_cast<CameraComponent*>(comp);
				if (cam->GetCameraNumber() == _MainCameraNumber) {
					SetMainCamera(cam);
					break;
				}
			}
		}
		if (_MainCamera) break;
	}
}

Object* Scene::FindObjectByName(const char* name)
{
	std::string strName(name);
	if (_objects.empty())return nullptr;
	for (auto& obj : _objects) {
		if (obj) {
			if (obj->GetObjectName() == strName) {
				return obj;
			}
		}
	}
	return nullptr;
}

void Scene::RegisterLight(LightComponent* l) {
	if (!l) return;
	if (std::find(_lights.begin(), _lights.end(), l) == _lights.end())
		_lights.push_back(l);
}

void Scene::UnregisterLight(LightComponent* l) {
	auto it = std::remove(_lights.begin(), _lights.end(), l);
	if (it != _lights.end()) _lights.erase(it, _lights.end());
}

void Scene::ProcessThreadSafeAdditions() {
	std::lock_guard<std::mutex> lock(_mtx);
	for (auto& obj : _ToBeAddedBuffer) {
		if (obj)_ToBeAdded.push_back(obj);
	}
	_ToBeAddedBuffer.clear();
}

void Scene::UploadLightsToGPU() {
	auto dev = DirectX11::GetInstance()->GetDevice();
	auto ctx = DirectX11::GetInstance()->GetContext();
	if (!dev || !ctx) return;

	if (!gLightCB) {
		D3D11_BUFFER_DESC bd{};
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.ByteWidth = sizeof(LightGPU) * kMaxLights + 16; // 余裕
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		dev->CreateBuffer(&bd, nullptr, &gLightCB);
	}
	if (!gLightCB) return;

	LightGPU lights[kMaxLights]{};
	int count = 0;
	for (auto* l : _lights) {
		if (!l || !l->IsEnabled()) continue;
		if (count >= kMaxLights) break;
		auto pos = l->GetWorldPosition();
		auto dir = l->GetWorldDirection();
		lights[count].position = pos;
		lights[count].direction = dir;
		lights[count].intensity = l->GetIntensity();
		lights[count].color = l->GetColor();
		lights[count].type = (float)((int)l->GetType());
		lights[count].range = l->GetRange();
		float innerRad = DirectX::XMConvertToRadians(l->GetSpotInner());
		float outerRad = DirectX::XMConvertToRadians(l->GetSpotOuter());
		lights[count].innerCos = cosf(innerRad * 0.5f); // 半角で扱うなら適宜
		lights[count].outerCos = cosf(outerRad * 0.5f);
		lights[count].enabled = 1.0f;
		++count;
	}

	// 末尾に LightCount を埋める別 cbuffer に分けてもよいが今回は同バッファ末尾に書かず別 CB 用意
	// 簡素化のため LightCount 用追加 cbuffer
	struct LightCountCB { int count; float pad[3]; };
	static ID3D11Buffer* gLightCountCB = nullptr;
	if (!gLightCountCB) {
		D3D11_BUFFER_DESC bd{};
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.ByteWidth = sizeof(LightCountCB);
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		dev->CreateBuffer(&bd, nullptr, &gLightCountCB);
	}

	// 更新
	{
		D3D11_MAPPED_SUBRESOURCE mp{};
		if (SUCCEEDED(ctx->Map(gLightCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mp))) {
			memcpy(mp.pData, lights, sizeof(LightGPU) * kMaxLights);
			ctx->Unmap(gLightCB, 0);
		}
		D3D11_MAPPED_SUBRESOURCE mp2{};
		if (SUCCEEDED(ctx->Map(gLightCountCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mp2))) {
			((LightCountCB*)mp2.pData)->count = count;
			ctx->Unmap(gLightCountCB, 0);
		}
	}

	// PS ステージへバインド (b1=LightArray, b2=LightCount 例)
	ID3D11Buffer* cbs1[] = { gLightCB };
	ctx->PSSetConstantBuffers(1, 1, cbs1);
	ID3D11Buffer* cbs2[] = { gLightCountCB };
	ctx->PSSetConstantBuffers(2, 1, cbs2);
}

void Scene::InitPhysics()
{
	pCollisionConfig = new btDefaultCollisionConfiguration();
	pDispatcher = new btCollisionDispatcher(pCollisionConfig);
	pOverlappingPairCache = new btDbvtBroadphase();
	pSolver = new btSequentialImpulseConstraintSolver();
	pPhysicsWorld = new btDiscreteDynamicsWorld(pDispatcher, pOverlappingPairCache, pSolver, pCollisionConfig);
	// 重力の設定
	pPhysicsWorld->setGravity(btVector3(0, -9.81f, 0));
}

void Scene::CleanupPhysics()
{
	if (pPhysicsWorld)
	{
		for (int i = pPhysicsWorld->getNumCollisionObjects() - 1; i >= 0; i--)
		{
			btCollisionObject* obj = pPhysicsWorld->getCollisionObjectArray()[i];
			btRigidBody* body = btRigidBody::upcast(obj);
			pPhysicsWorld->removeCollisionObject(obj);
			delete obj;
		}
		delete pPhysicsWorld;
		pPhysicsWorld = nullptr;
	}
	if (pSolver)
	{
		delete pSolver;
		pSolver = nullptr;
	}
	if (pOverlappingPairCache)
	{
		delete pOverlappingPairCache;
		pOverlappingPairCache = nullptr;
	}
	if (pDispatcher)
	{
		delete pDispatcher;
		pDispatcher = nullptr;
	}
	if (pCollisionConfig)
	{
		delete pCollisionConfig;
		pCollisionConfig = nullptr;
	}
}

void Scene::CleanupAndReinitializePhysics()
{
	CleanupPhysics();
	InitPhysics();
	for (auto& obj : _objects)
	{
		if (obj)
		{
			auto rb = obj->GetComponent<RigidBody>();
			if (rb)
			{
				rb->ResetAddedToWorldFlag();
				rb->BeginPlay();
			}
		}
	}
}

void Scene::AddObjectLocal(Object* obj) {
	if (obj) {
		_ToBeAdded.push_back(obj);
	}
}

void Scene::RemoveObject(Object* obj) {
	if (!obj) return;
	_ToBeRemoved.push_back(obj);
}

bool Scene::AddObject(Object* obj)
{
	if (!obj) return false;
	std::thread([this, obj]() {
		Object* newObj = obj->Clone();
		if (newObj) {
			std::lock_guard<std::mutex>lock(_mtx);
			_ToBeAddedBuffer.push_back(newObj);
		}
		}).detach();
	return true;
}