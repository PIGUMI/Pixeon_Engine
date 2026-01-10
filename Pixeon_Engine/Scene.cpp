#include "Scene.h"
#include "System.h"
#include "Object.h"
#include "ComponentManager.h"
#include "SettingManager.h"
#include "Component.h"
#include "LightComponent.h"
#include "RigidBody.h"
#include <thread>
#include <mutex>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

struct LightGPU {
	DirectX::XMFLOAT3 position;
	float intensity;
	DirectX::XMFLOAT3 direction;
	float type;
	DirectX::XMFLOAT3 color;
	float range;
	float innerCos;
	float outerCos;
	float enabled;
	float pad;
};

static ID3D11Buffer* gLightCB = nullptr;
static const int kMaxLights = 8;
static ID3D11Buffer* gLightCountCB = nullptr;

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
	if (gLightCountCB) {  // ← 追加
		gLightCountCB->Release();
		gLightCountCB = nullptr;
	}
}

void Scene::Init() {
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
	// Objects Clone
	for (auto& obj : _objects) {
		if (obj) {
			AbstractObject* cloneObj = obj->Clone();
			_SaveObjects.push_back(cloneObj);
		}
	}
	// Physics Cleanup
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
	// CameraNumber Save
	editorCameraNumber = _MainCameraNumber;
	// BeginPlay Call
	for (auto& obj : _objects) {
		if (obj)obj->BeginPlay();
	}
	EndPlayCalled = false;
}

void Scene::EditUpdate() {
	InGame = false;
	if (!EndPlayCalled){
		EndPlayCalled = true;
		for (auto& obj : _objects){
			if (obj) obj->UInit();
			delete obj;
		}
		_ToBeAdded.clear();
		_ToBeAddedBuffer.clear();
		_objects.clear();
		_objects = _SaveObjects;
		_SaveObjects.clear();
		_MainCameraNumber = editorCameraNumber;
		for (auto& obj : _objects) {
			if (!obj) continue;
			for (auto& comp : obj->GetComponents()) {
				if (!comp) continue;
				if (comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA) {
					CameraComponent* cam = dynamic_cast<CameraComponent*>(comp);
					if (cam->GetCameraNumber() == _MainCameraNumber)_MainCamera = cam;
				}
			}
		}
	}
	ProcessThreadSafeAdditions();
	for (auto& obj : _ToBeAdded) {
		if (obj) {
			obj->SetParentScene(this);
			_objects.push_back(obj);
		}
	}
	_ToBeAdded.clear();
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
	for (auto& obj : _objects) if (obj)obj->EditUpdate();
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
	InGame = true;
	ProcessThreadSafeAdditions();
	for (auto& obj : _ToBeAdded) {
		if (obj) {
			obj->SetParentScene(this);
			obj->BeginPlay();
			_objects.push_back(obj);
		}
	}
	_ToBeAdded.clear();
	int i = 0;
	for (auto& obj : _objects) {
		if (!obj) continue;
		for (auto& comp : obj->GetComponents()) {
			if (!comp) continue;
			if (comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA) {
				CameraComponent* cam = dynamic_cast<CameraComponent*>(comp);
				cam->SetCameraNumber(i);
				i++;
				comp->InGameUpdate();
			}
		}
	}
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

				if (!hasInvakudObjects){
					float timeStep = 1.0f / 60.0f;
					int maxSubSteps = 10;
					bool valid = true;
					for (int i = 0; i < pPhysicsWorld->getNumCollisionObjects(); ++i){
						btCollisionObject* obj = pPhysicsWorld->getCollisionObjectArray()[i];
						btRigidBody* body = btRigidBody::upcast(obj);
						if (!body || !body->getCollisionShape() || !body->getMotionState()){
							valid = false;
							break;
						}
					}
					if (valid){
						pPhysicsWorld->stepSimulation(timeStep, maxSubSteps);
					}
				}
				else{
					CleanupAndReinitializePhysics();
				}
			}
			else{
				CleanupAndReinitializePhysics();
			}
		}
		catch (...)
		{
			CleanupAndReinitializePhysics();
		}
	}
	for (auto& obj : _objects) if (obj)obj->InGameUpdate();
	if (_collisionManager)_collisionManager->Update();
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

void Scene::Draw(int Layer) {
	UploadLightsToGPU();
	// オブジェクトの描画
	std::vector<AbstractObject*> sortedList = _objects;
	if (_MainCamera) {
		std::sort(sortedList.begin(), sortedList.end(), [this](AbstractObject* a, AbstractObject* b) {
			if (!a || !b) return false;
			DirectX::XMFLOAT3 camPos = _MainCamera->GetPosition();
			DirectX::XMFLOAT3 posA = a->GetTransform().position;
			DirectX::XMFLOAT3 posB = b->GetTransform().position;
			float distA = (camPos.x - posA.x) * (camPos.x - posA.x) + (camPos.y - posA.y) * (camPos.y - posA.y) + (camPos.z - posA.z) * (camPos.z - posA.z);
			float distB = (camPos.x - posB.x) * (camPos.x - posB.x) + (camPos.y - posB.y) * (camPos.y - posB.y) + (camPos.z - posB.z) * (camPos.z - posB.z);
			// 距離が近い順にソート
			return distA > distB;
			});
	}
	for (auto& obj : sortedList)
	{
		if (obj)
		{
			obj->Draw(Layer);
		}
	}
}

void Scene::DrawUI()
{
}


void Scene::SaveToFile() {
	std::vector<AbstractObject*> SaveObjects;
	if (InGame) {
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
		return;
	}

	nlohmann::json sceneData;
	inFile >> sceneData;
	inFile.close();

	_name = sceneData["SceneSettings"]["Name"].get<std::string>();
	_MainCameraNumber = sceneData["SceneSettings"]["MainCameraNumber"].get<int>();

	// Objectsの読み込み
	for (const auto& objData : sceneData["Objects"]) {
		AbstractObject* newObj = new AbstractObject();
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


void Scene::SetMainCamera(CameraComponent* camera){
	_MainCamera = camera;
	if (_MainCamera)
		_MainCameraNumber = _MainCamera->GetCameraNumber();
	else
		_MainCameraNumber = -1;
}

void Scene::SetMainCameraNumber(int num)
{
	_MainCameraNumber = num;
	for (auto& obj : _objects) {
		if (!obj) continue;
		for (auto& comp : obj->GetComponents()) {
			if (!comp) continue;
			if (comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA) {
				CameraComponent* cam = dynamic_cast<CameraComponent*>(comp);
				if (cam->GetCameraNumber() == _MainCameraNumber) {
					_MainCamera = cam;
					return;
				}
			}
		}
	}
}


AbstractObject* Scene::FindObjectByName(const char* name)
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
		bd.ByteWidth = sizeof(LightGPU) * kMaxLights + 16;
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		dev->CreateBuffer(&bd, nullptr, &gLightCB);
	}
	if (!gLightCB) return;

	// 配列を明示的にゼロクリア
	LightGPU lights[kMaxLights];
	memset(lights, 0, sizeof(lights));

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
		lights[count].innerCos = cosf(innerRad * 0.5f);
		lights[count].outerCos = cosf(outerRad * 0.5f);
		lights[count].enabled = 1.0f;
		++count;
	}

	for (int i = count; i < kMaxLights; ++i) {
		lights[i].enabled = 0.0f;
	}

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

void Scene::AddObjectLocal(AbstractObject* obj) {
	if (obj) {
		_ToBeAdded.push_back(obj);
	}
}

void Scene::RemoveObject(AbstractObject* obj) {
	if (!obj) return;
	_ToBeRemoved.push_back(obj);
}

bool Scene::AddObject(AbstractObject* obj)
{
	if (!obj) return false;
	std::thread([this, obj]() {
		AbstractObject* newObj = obj->Clone();
		if (newObj) {
			std::lock_guard<std::mutex>lock(_mtx);
			_ToBeAddedBuffer.push_back(newObj);
		}
		}).detach();
	return true;
}