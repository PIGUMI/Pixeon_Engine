#include "Scene.h"
#include "System.h"
#include "Object.h"
#include "ComponentManager.h"
#include "SettingManager.h"
#include "EffectManager.h"
#include "Component.h"
#include "LightComponent.h"
#include "EffectComponent.h"
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

AbstractScene::~AbstractScene()
{
	SaveToFile();

	// 全オブジェクトの親子関係を先に切断
	for (auto& obj : _objects) {
		if (obj) {
			obj->_parentObject = nullptr;
			obj->_children.clear();
		}
	}

	// オブジェクトを削除
	for (auto& obj : _objects) {
		if (obj) {
			obj->UnInit();
			delete obj;
		}
	}
	_objects.clear();

	CleanupPhysics();

	// ToBeAdded も同様に処理
	for (auto& obj : _ToBeAdded) {
		if (obj) {
			obj->_parentObject = nullptr;
			obj->_children.clear();
			obj->UnInit();
			delete obj;
		}
	}
	_ToBeAdded.clear();

	// SaveObjects も同様に処理
	for (auto& obj : _SaveObjects) {
		if (obj) {
			obj->_parentObject = nullptr;
			obj->_children.clear();
			obj->UnInit();
			delete obj;
		}
	}
	_SaveObjects.clear();

	_lights.clear();
}

void AbstractScene::Init() {
	_objects.clear();
	_ToBeAdded.clear();
	_ToBeRemoved.clear();
	_SaveObjects.clear();
	EndPlayCalled = true;
	InitPhysics();
	_collisionManager = new CollisionManager;
	_collisionManager->Initialize(pPhysicsWorld);
}

void AbstractScene::BeginPlay() {
	for (auto& obj : _objects) {
		if (obj && obj->GetParent() == nullptr) {
			AbstractObject* cloneObj = obj->Clone();
			_SaveObjects.push_back(cloneObj);

			std::function<void(AbstractObject*)> collectChildren = [&](AbstractObject* parent) {
				for (auto child : parent->GetChildren()) {
					if (child) {
						_SaveObjects.push_back(child);
						collectChildren(child);
					}
				}
				};
			collectChildren(cloneObj);
		}
	}

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

	editorCameraNumber = _MainCameraNumber;

	for (auto& obj : _objects) {
		if (obj) obj->BeginPlay();
	}
	EndPlayCalled = false;

	UploadLightsToGPU();
}

void AbstractScene::EditUpdate() {
	InGame = false;
	if (!EndPlayCalled) {
		EndPlayCalled = true;

		// 既存のオブジェクトを削除
		for (auto& obj : _objects) {
			if (obj) {
				obj->_parentObject = nullptr;  // 親参照をクリア
				obj->_children.clear();        // 子リストをクリア
				obj->UnInit();
				delete obj;
			}
		}
		_ToBeAdded.clear();
		_ToBeAddedBuffer.clear();
		_objects.clear();

		// SaveObjectsから復元（ルートのみ）
		for (auto& obj : _SaveObjects) {
			if (obj && obj->GetParent() == nullptr) {
				_objects.push_back(obj);

				// 子オブジェクトも_objectsに追加（再帰的）
				std::function<void(AbstractObject*)> addChildren = [&](AbstractObject* parent) {
					for (auto child : parent->GetChildren()) {
						if (child) {
							_objects.push_back(child);
							addChildren(child);  // 再帰的に孫も追加
						}
					}
					};
				addChildren(obj);
			}
		}
		_SaveObjects.clear();

		_MainCameraNumber = editorCameraNumber;
		for (auto& obj : _objects) {
			if (!obj) continue;
			for (auto& comp : obj->GetComponents()) {
				if (!comp) continue;
				if (comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA) {
					CameraComponent* cam = dynamic_cast<CameraComponent*>(comp);
					if (cam->GetCameraNumber() == _MainCameraNumber) _MainCamera = cam;
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

	// ルートオブジェクトのみ更新（子は自動的に更新される）
	for (auto& obj : _objects) {
		if (obj && obj->GetParent() == nullptr) {
			obj->EditUpdate();
		}
	}

	// 削除処理
	if (!_ToBeRemoved.empty()) {
		// 削除前に親子関係を全て切断
		for (auto& obj : _ToBeRemoved) {
			if (!obj) continue;

			// 親から切り離す
			if (obj->GetParent()) {
				obj->GetParent()->RemoveChild(obj);
				obj->_parentObject = nullptr;
			}

			// 子オブジェクトの親参照をクリア
			for (auto child : obj->GetChildren()) {
				if (child) {
					child->_parentObject = nullptr;
				}
			}
			obj->_children.clear();
		}

		// オブジェクトを削除
		for (auto& obj : _ToBeRemoved) {
			if (!obj) continue;

			auto it = std::find(_objects.begin(), _objects.end(), obj);
			if (it != _objects.end()) {
				_objects.erase(it);
			}

			obj->UnInit();
			delete obj;
		}
		_ToBeRemoved.clear();
	}
}

void AbstractScene::PlayUpdate() {
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

				if (i == _MainCameraNumber) {
					_MainCamera = cam;
				}

				i++;
				comp->InGameUpdate();
			}
		}
	}

	// 物理演算
	if (pPhysicsWorld)
	{
		try
		{
			int numObjects = pPhysicsWorld->getNumCollisionObjects();
			if (numObjects >= 0 && numObjects < 10000)
			{
				bool hasInvalidObjects = false;
				for (int i = 0; i < numObjects; i++)
				{
					btCollisionObject* obj = pPhysicsWorld->getCollisionObjectArray()[i];
					if (!obj || !obj->getCollisionShape())
					{
						hasInvalidObjects = true;
						break;
					}
				}

				if (!hasInvalidObjects) {
					float timeStep = 1.0f / 60.0f;

					int maxSubSteps = 10;
					if (numObjects > 20) maxSubSteps = 15;
					if (numObjects > 50) maxSubSteps = 20;

					float fixedTimeStep = 1.0f / 240.0f;

					bool valid = true;
					for (int i = 0; i < pPhysicsWorld->getNumCollisionObjects(); ++i) {
						btCollisionObject* obj = pPhysicsWorld->getCollisionObjectArray()[i];
						btRigidBody* body = btRigidBody::upcast(obj);
						if (!body || !body->getCollisionShape() || !body->getMotionState()) {
							valid = false;
							break;
						}
					}

					if (valid) {
						pPhysicsWorld->stepSimulation(timeStep, maxSubSteps, fixedTimeStep);

						if (numObjects > 50) {
							pPhysicsWorld->getBroadphase()->resetPool(pPhysicsWorld->getDispatcher());
						}
					}
				}
				else {
					CleanupAndReinitializePhysics();
				}
			}
			else {
				CleanupAndReinitializePhysics();
			}
		}
		catch (...)
		{
			CleanupAndReinitializePhysics();
		}
	}

	// ルートオブジェクトのみ更新（子は自動的に更新される）
	for (auto& obj : _objects) {
		if (obj && obj->GetParent() == nullptr) {
			obj->InGameUpdate();
		}
	}

	if (_collisionManager) _collisionManager->Update();

	// 削除処理
	if (!_ToBeRemoved.empty()) {
		for (auto& obj : _ToBeRemoved) {
			if (!obj) continue;

			if (obj->GetParent()) {
				obj->GetParent()->RemoveChild(obj);
				obj->_parentObject = nullptr;
			}

			for (auto child : obj->GetChildren()) {
				if (child) {
					child->_parentObject = nullptr;
				}
			}
			obj->_children.clear();
		}

		for (auto& obj : _ToBeRemoved) {
			if (!obj) continue;

			auto it = std::find(_objects.begin(), _objects.end(), obj);
			if (it != _objects.end()) {
				_objects.erase(it);
			}

			obj->UnInit();
			delete obj;
		}
		_ToBeRemoved.clear();
	}
}

void AbstractScene::Draw(int Layer) {
	UploadLightsToGPU();

	std::vector<AbstractObject*> sortedList;

	for (auto& obj : _objects) {
		if (obj && obj->GetParent() == nullptr) {
			sortedList.push_back(obj);
		}
	}

	if (_MainCamera) {
		std::sort(sortedList.begin(), sortedList.end(), [this](AbstractObject* a, AbstractObject* b) {
			if (!a || !b) return false;
			DirectX::XMFLOAT3 camPos = _MainCamera->GetPosition();
			DirectX::XMFLOAT3 posA = a->GetWorldPosition();
			DirectX::XMFLOAT3 posB = b->GetWorldPosition();
			float distA = (camPos.x - posA.x) * (camPos.x - posA.x) +
				(camPos.y - posA.y) * (camPos.y - posA.y) +
				(camPos.z - posA.z) * (camPos.z - posA.z);
			float distB = (camPos.x - posB.x) * (camPos.x - posB.x) +
				(camPos.y - posB.y) * (camPos.y - posB.y) +
				(camPos.z - posB.z) * (camPos.z - posB.z);
			return distA > distB;
			});
	}

	for (auto& obj : sortedList) {
		if (obj) {
			obj->Draw(Layer);
		}
	}
}

void AbstractScene::DrawUI()
{
}

void AbstractScene::UpdateEffects(float deltaTime)
{
	EffectManager::Instance()->Update(deltaTime);
}

void AbstractScene::DrawEffects(int Layer, CameraComponent* camera)
{
	if (!camera) return;

	auto effects = CollectEffectComponents(Layer);
	if (effects.empty()) return;

	auto view = camera->GetView();
	auto proj = camera->GetProjection();
	EffectManager::Instance()->SetCamera(view, proj);

	EffectManager::Instance()->Draw();
}

void AbstractScene::SaveToFile() {
	std::vector<AbstractObject*> SaveObjects;

	if (InGame) {
		SaveObjects = _SaveObjects;
	}
	else {
		for (auto& obj : _objects) {
			if (obj) {
				SaveObjects.push_back(obj);
			}
		}
	}

	auto Now = std::chrono::system_clock::now();
	auto in_time_t = std::chrono::system_clock::to_time_t(Now);
	std::tm localtime;
	localtime_s(&localtime, &in_time_t);

	nlohmann::json SceneData;
	SceneData["SceneSettings"]["Name"] = _name;
	SceneData["SceneSettings"]["MainCameraNumber"] = _MainCameraNumber;

	nlohmann::json ObjectArray = nlohmann::json::array();

	for (const auto& Object : SaveObjects) {
		if (Object) {
			nlohmann::json ObjectData;
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
				ObjectData["Parent"] = Object->GetParent()->GetObjectName();
			}
			else {
				ObjectData["Parent"] = "";
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

	std::string File = SettingManager::GetInstance()->GetSceneFilePath() + _name + ".scene";
	std::ofstream outFile(File);
	if (outFile.is_open()) {
		outFile << SceneData.dump(4);
		outFile.close();

		std::string msg = "[AbstractScene] Saved " + std::to_string(SaveObjects.size()) + " objects to " + File + "\n";
		OutputDebugStringA(msg.c_str());
	}
	else {
		MessageBox(nullptr, ("シーンファイルの保存に失敗:  " + File).c_str(), "Error", MB_OK);
	}
}

void AbstractScene::LoadToFile() {
	std::string filePath = SettingManager::GetInstance()->GetSceneFilePath() + "/" + _name + ".scene";
	std::ifstream inFile(filePath);
	if (!inFile.is_open()) {
		OutputDebugStringA(("[AbstractScene] Failed to open:  " + filePath + "\n").c_str());
		return;
	}

	try {
		nlohmann::json sceneData;
		inFile >> sceneData;
		inFile.close();

		_name = sceneData["SceneSettings"]["Name"].get<std::string>();
		_MainCameraNumber = sceneData["SceneSettings"]["MainCameraNumber"].get<int>();

		std::map<std::string, AbstractObject*> objectMap;
		std::map<AbstractObject*, std::string> parentNames;

		int objectCount = 0;
		for (const auto& objData : sceneData["Objects"]) {
			AbstractObject* newObj = new AbstractObject();
			newObj->SetParentScene(this);

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

			if (objData.contains("Parent") && !objData["Parent"].get<std::string>().empty()) {
				parentNames[newObj] = objData["Parent"].get<std::string>();
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

			objectMap[objName] = newObj;
			AddObjectLocal(newObj);
			objectCount++;
		}

		for (const auto& pair : parentNames) {
			AbstractObject* child = pair.first;
			const std::string& parentName = pair.second;

			auto it = objectMap.find(parentName);
			if (it != objectMap.end()) {
				child->SetParent(it->second);
			}
			else {
				OutputDebugStringA(("[AbstractScene] Warning: Parent not found: " + parentName + "\n").c_str());
			}
		}

		std::string msg = "[AbstractScene] Loaded " + std::to_string(objectCount) + " objects from " + filePath + "\n";
		OutputDebugStringA(msg.c_str());

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
	catch (const std::exception& e) {
		MessageBox(nullptr, ("シーン読み込みエラー: " + std::string(e.what())).c_str(), "Error", MB_OK);
		inFile.close();
	}
}

void AbstractScene::SetMainCamera(CameraComponent* camera) {
	_MainCamera = camera;
	if (_MainCamera)
		_MainCameraNumber = _MainCamera->GetCameraNumber();
	else
		_MainCameraNumber = -1;
}

void AbstractScene::SetMainCameraNumber(int num)
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

AbstractObject* AbstractScene::FindObjectByName(const char* name)
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

void AbstractScene::RegisterLight(LightComponent* l) {
	if (!l) return;
	if (std::find(_lights.begin(), _lights.end(), l) == _lights.end())
		_lights.push_back(l);
}

void AbstractScene::UnregisterLight(LightComponent* l) {
	auto it = std::remove(_lights.begin(), _lights.end(), l);
	if (it != _lights.end()) _lights.erase(it, _lights.end());
}

void AbstractScene::ProcessThreadSafeAdditions() {
	std::lock_guard<std::mutex> lock(_mtx);
	for (auto& obj : _ToBeAddedBuffer) {
		if (obj)_ToBeAdded.push_back(obj);
	}
	_ToBeAddedBuffer.clear();
}

void AbstractScene::UploadLightsToGPU() {
	auto dev = DirectX11::GetInstance()->GetDevice();
	auto ctx = DirectX11::GetInstance()->GetContext();
	if (!dev || !ctx) return;

	if (!gLightCB) {
		D3D11_BUFFER_DESC bd{};
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.ByteWidth = sizeof(LightGPU) * kMaxLights;
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		dev->CreateBuffer(&bd, nullptr, &gLightCB);
	}
	if (!gLightCB) return;

	LightGPU lights[kMaxLights];
	memset(lights, 0, sizeof(LightGPU) * kMaxLights);

	int count = 0;

	for (auto* obj : _objects) {
		if (!obj) continue;

		bool isSaveObject = std::find(_SaveObjects.begin(), _SaveObjects.end(), obj) != _SaveObjects.end();
		if (isSaveObject) continue;

		auto lightComps = obj->GetComponentsByType<LightComponent>();
		for (auto* l : lightComps) {
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
		if (count >= kMaxLights) break;
	}

	struct LightCountCB { int count; float pad[3]; };

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
	}

	if (gLightCountCB) {
		D3D11_MAPPED_SUBRESOURCE mp2{};
		if (SUCCEEDED(ctx->Map(gLightCountCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mp2))) {
			LightCountCB* countData = (LightCountCB*)mp2.pData;
			countData->count = count;
			countData->pad[0] = 0.0f;
			countData->pad[1] = 0.0f;
			countData->pad[2] = 0.0f;
			ctx->Unmap(gLightCountCB, 0);
		}
	}

	ID3D11Buffer* cbs1[] = { gLightCB };
	ctx->PSSetConstantBuffers(1, 1, cbs1);
	ID3D11Buffer* cbs2[] = { gLightCountCB };
	ctx->PSSetConstantBuffers(2, 1, cbs2);
}

void AbstractScene::InitPhysics()
{
	pCollisionConfig = new btDefaultCollisionConfiguration();
	pDispatcher = new btCollisionDispatcher(pCollisionConfig);
	pOverlappingPairCache = new btDbvtBroadphase();
	pSolver = new btSequentialImpulseConstraintSolver();
	pPhysicsWorld = new btDiscreteDynamicsWorld(pDispatcher, pOverlappingPairCache, pSolver, pCollisionConfig);

	pPhysicsWorld->setGravity(btVector3(0, -9.81f, 0));

	pPhysicsWorld->getSolverInfo().m_numIterations = 20;
	pPhysicsWorld->getSolverInfo().m_solverMode |= SOLVER_USE_2_FRICTION_DIRECTIONS;
	pPhysicsWorld->getSolverInfo().m_solverMode |= SOLVER_USE_WARMSTARTING;

	pPhysicsWorld->getSolverInfo().m_splitImpulse = true;
	pPhysicsWorld->getSolverInfo().m_splitImpulsePenetrationThreshold = -0.01f;

	pPhysicsWorld->getSolverInfo().m_erp = 0.3f;
	pPhysicsWorld->getSolverInfo().m_erp2 = 0.3f;

	pPhysicsWorld->getSolverInfo().m_globalCfm = 0.00001f;

	pPhysicsWorld->getDispatchInfo().m_useContinuous = true;

	pPhysicsWorld->getSolverInfo().m_timeStep = 1.0f / 240.0f;
}

void AbstractScene::CleanupPhysics()
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

void AbstractScene::CleanupAndReinitializePhysics()
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

std::vector<EffectComponent*> AbstractScene::CollectEffectComponents(int layer)
{
	std::vector<EffectComponent*> effects;

	for (auto& obj : _objects)
	{
		if (!obj) continue;

		bool isSaveObject = std::find(_SaveObjects.begin(), _SaveObjects.end(), obj) != _SaveObjects.end();
		if (isSaveObject) continue;

		auto comps = obj->GetComponentsByType<EffectComponent>();
		for (auto* comp : comps)
		{
			if (comp && comp->GetLayerNumber() == layer)
			{
				effects.push_back(comp);
			}
		}
	}

	return effects;
}

void AbstractScene::AddObjectLocal(AbstractObject* obj) {
	if (!obj) return;
	obj->SetParentScene(this);

	auto it = std::find(_ToBeAdded.begin(), _ToBeAdded.end(), obj);
	if (it != _ToBeAdded.end()) return;

	auto it2 = std::find(_objects.begin(), _objects.end(), obj);
	if (it2 != _objects.end()) return;

	_ToBeAdded.push_back(obj);
}

void AbstractScene::RemoveObject(AbstractObject* obj) {
	if (!obj) return;

	if (obj->GetParent()) {
		obj->RemoveParent();
	}

	std::vector<AbstractObject*> toRemove;
	std::function<void(AbstractObject*)> collectAllChildren = [&](AbstractObject* parent) {
		toRemove.push_back(parent);
		for (auto child : parent->GetChildren()) {
			if (child) {
				collectAllChildren(child);
			}
		}
		};
	collectAllChildren(obj);

	for (auto removeObj : toRemove) {
		auto it = std::find(_ToBeRemoved.begin(), _ToBeRemoved.end(), removeObj);
		if (it == _ToBeRemoved.end()) {
			_ToBeRemoved.push_back(removeObj);
		}
	}
}

bool AbstractScene::AddObject(AbstractObject* obj)
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