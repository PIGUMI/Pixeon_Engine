#include "Scene.h"
#include "System.h"
#include "Object.h"
#include "ComponentManager.h"
#include "ModelRender.h"
#include "ModelManager.h"
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

	// レイヤーの解放
	for (auto& layer : _layers) {
		delete layer;
	}
	_layers.clear();
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

	for (int i = 0; i < MAX_LAYER_COUNT; i++)
	{
		Layer* layer = new Layer;
		layer->layerIndex = i;
		layer->name = "Layer " + std::to_string(i);
		_layers.push_back(layer);
	}
	CreateShadowMapResources();
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
					if (numObjects > 50) maxSubSteps = 80;
					if (numObjects > 100) maxSubSteps = 120;
					if (numObjects > 200) maxSubSteps = 150;

					float fixedTimeStep = 1.0f / 600.0f;

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

						if (numObjects > 100) {
							static int resetCounter = 0;
							resetCounter++;
							if (resetCounter > 60) {
								pPhysicsWorld->getBroadphase()->resetPool(pPhysicsWorld->getDispatcher());
								resetCounter = 0;
							}
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
	RenderShadowMap();

	UploadLightsToGPU();

	auto ctx = DirectX11::GetInstance()->GetContext();
	DirectX::XMMATRIX lightVP = GetLightViewProjection();

	struct ShadowCB {
		DirectX::XMMATRIX lightViewProj;
	};

	static Microsoft::WRL::ComPtr<ID3D11Buffer> shadowCB;
	if (!shadowCB) {
		D3D11_BUFFER_DESC bd{};
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.ByteWidth = sizeof(ShadowCB);
		bd.Usage = D3D11_USAGE_DEFAULT;
		DirectX11::GetInstance()->GetDevice()->CreateBuffer(&bd, nullptr, shadowCB.GetAddressOf());
	}

	ShadowCB shadowData;
	shadowData.lightViewProj = DirectX::XMMatrixTranspose(lightVP);
	ctx->UpdateSubresource(shadowCB.Get(), 0, nullptr, &shadowData, 0, 0);

	ID3D11Buffer* cbs3[] = { shadowCB.Get() };
	ctx->PSSetConstantBuffers(3, 1, cbs3);

	// シャドウマップをピクセルシェーダーにバインド
	ID3D11ShaderResourceView* srvs[] = { m_shadowMapSRV.Get() };
	ctx->PSSetShaderResources(1, 1, srvs);

	ID3D11SamplerState* samplers[] = { m_shadowSampler.Get() };
	ctx->PSSetSamplers(1, 1, samplers);


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

	nlohmann::json layersData = nlohmann::json::array();
	std::string LayerSizeStr = "LayerCount_" + std::to_string(_layers.size());
	for (auto& layer : _layers) {
		nlohmann::json layerJson;
		layer->SaveToJson(layerJson);
		layersData.push_back(layerJson);
	}
	SceneData["Layers"] = layersData;

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

		if (sceneData.contains("Layers") && sceneData["Layers"].is_array()) {
			auto& layersArray = sceneData["Layers"];
			for (const auto& layerJson : layersArray) {
				_layers[layerJson["layerIndex"].get<int>()]->LoadFromJson(layerJson);
			}
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

	pPhysicsWorld->getSolverInfo().m_numIterations = 300;

	pPhysicsWorld->getSolverInfo().m_solverMode |= SOLVER_USE_2_FRICTION_DIRECTIONS;
	pPhysicsWorld->getSolverInfo().m_solverMode |= SOLVER_USE_WARMSTARTING;

	pPhysicsWorld->getSolverInfo().m_splitImpulse = true;
	pPhysicsWorld->getSolverInfo().m_splitImpulsePenetrationThreshold = -0.02f;

	pPhysicsWorld->getSolverInfo().m_erp = 0.8f;
	pPhysicsWorld->getSolverInfo().m_erp2 = 0.2f;

	pPhysicsWorld->getSolverInfo().m_globalCfm = 0.00001f;

	pPhysicsWorld->getDispatchInfo().m_useContinuous = true;
	pPhysicsWorld->getDispatchInfo().m_allowedCcdPenetration = 0.001f;

	pPhysicsWorld->getSolverInfo().m_timeStep = 1.0f / 300.0f;

	pPhysicsWorld->getSolverInfo().m_numIterations = 100;
	pPhysicsWorld->getSolverInfo().m_minimumSolverBatchSize = 128;
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

bool AbstractScene::CreateShadowMapResources()
{
	auto dev = DirectX11::GetInstance()->GetDevice();
	if (!dev) return false;

	// シャドウマップテクスチャ作成
	D3D11_TEXTURE2D_DESC texDesc{};
	texDesc.Width = SHADOW_MAP_SIZE;
	texDesc.Height = SHADOW_MAP_SIZE;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(dev->CreateTexture2D(&texDesc, nullptr, m_shadowMapTexture.GetAddressOf()))) {
		return false;
	}

	// デプスステンシルビュー作成
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	if (FAILED(dev->CreateDepthStencilView(m_shadowMapTexture.Get(), &dsvDesc,
		m_shadowMapDSV.GetAddressOf()))) {
		return false;
	}

	// シェーダーリソースビュー作成
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	if (FAILED(dev->CreateShaderResourceView(m_shadowMapTexture.Get(), &srvDesc,
		m_shadowMapSRV.GetAddressOf()))) {
		return false;
	}

	// シャドウサンプラー作成
	D3D11_SAMPLER_DESC sampDesc{};
	sampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.BorderColor[0] = 1.0f;
	sampDesc.BorderColor[1] = 1.0f;
	sampDesc.BorderColor[2] = 1.0f;
	sampDesc.BorderColor[3] = 1.0f;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	if (FAILED(dev->CreateSamplerState(&sampDesc, m_shadowSampler.GetAddressOf()))) {
		return false;
	}

	return true;
}

DirectX::XMMATRIX AbstractScene::GetLightViewProjection()
{
	// メインのディレクショナルライトを取得
	DirectX::XMFLOAT3 lightDir(0, -1, 0);

	for (auto* light : _lights) {
		if (light && light->GetType() == LightComponent::LightType::Directional) {
			lightDir = light->GetWorldDirection();
			break;
		}
	}

	// カメラの位置を取得してシャドウマップの中心をカメラ位置にする
	DirectX::XMFLOAT3 cameraPos(0, 0, 0);
	if (_MainCamera) {
		cameraPos = _MainCamera->GetPosition();
	}

	// ライトの位置と方向からビュー行列を作成（カメラ位置を中心に）
	DirectX::XMVECTOR lightPos = DirectX::XMVectorSet(
		cameraPos.x - lightDir.x * 50.0f,  // ① ライトの距離（デフォルト: 50.0f）
		cameraPos.y - lightDir.y * 50.0f,  //    大きくすると影が遠くまで届く
		cameraPos.z - lightDir.z * 50.0f,  //    小さくすると影の精度が上がる
		1.0f
	);
	DirectX::XMVECTOR target = DirectX::XMLoadFloat3(&cameraPos);
	DirectX::XMVECTOR up = DirectX::XMVectorSet(0, 1, 0, 0);

	DirectX::XMMATRIX lightView = DirectX::XMMatrixLookAtLH(lightPos, target, up);

	// 正射影行列（シャドウマップ用）
	float size = 100.0f;  // ② 影の描画範囲の幅と高さ（デフォルト: 100.0f）
	//    大きくすると広範囲に影が描画される
	//    小さくすると影の精度が上がる

	DirectX::XMMATRIX lightProj = DirectX::XMMatrixOrthographicLH(
		size,       // 幅
		size,       // 高さ
		1.0f,       // ③ ニアクリップ（デフォルト: 1.0f）
		//    影が描画される最小距離
		200.0f      // ④ ファークリップ（デフォルト: 200.0f）
					//    影が描画される最大距離
					//    大きくすると深い影も描画される
	);

	return lightView * lightProj;
}

void AbstractScene::RenderShadowMap() {
	auto ctx = DirectX11::GetInstance()->GetContext();
	if (!ctx || !m_shadowMapDSV) return;

	// 現在のレンダーターゲットを保存
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> oldRTV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> oldDSV;
	ctx->OMGetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.GetAddressOf());

	D3D11_VIEWPORT oldViewport;
	UINT numViewports = 1;
	ctx->RSGetViewports(&numViewports, &oldViewport);

	// シャドウマップ用のビューポート設定
	D3D11_VIEWPORT shadowViewport{};
	shadowViewport.Width = static_cast<float>(SHADOW_MAP_SIZE);
	shadowViewport.Height = static_cast<float>(SHADOW_MAP_SIZE);
	shadowViewport.MinDepth = 0.0f;
	shadowViewport.MaxDepth = 1.0f;
	ctx->RSSetViewports(1, &shadowViewport);

	// シャドウマップをクリア
	ctx->ClearDepthStencilView(m_shadowMapDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	// レンダーターゲットなし、デプスのみ
	ID3D11RenderTargetView* nullRTV = nullptr;
	ctx->OMSetRenderTargets(1, &nullRTV, m_shadowMapDSV.Get());

	// ライトビュー行列を取得
	DirectX::XMMATRIX lightViewProj = GetLightViewProjection();

	// シャドウマップ用のシェーダーを取得
	auto* sm = ShaderManager::GetInstance();
	ID3D11VertexShader* shadowVS = sm->GetVertexShader("VS_ShadowMap");
	ID3D11PixelShader* shadowPS = sm->GetPixelShader("PS_ShadowMap");

	if (shadowVS && shadowPS) {
		ctx->VSSetShader(shadowVS, nullptr, 0);
		ctx->PSSetShader(shadowPS, nullptr, 0);

		// シャドウマップ用の定数バッファを作成・設定
		struct ShadowCB {
			DirectX::XMMATRIX lightViewProj;
			DirectX::XMMATRIX world;
		};

		static Microsoft::WRL::ComPtr<ID3D11Buffer> shadowVSCB;
		if (!shadowVSCB) {
			D3D11_BUFFER_DESC bd{};
			bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			bd.ByteWidth = sizeof(ShadowCB);
			bd.Usage = D3D11_USAGE_DEFAULT;
			DirectX11::GetInstance()->GetDevice()->CreateBuffer(&bd, nullptr, shadowVSCB.GetAddressOf());
		}

		// 全オブジェクトを再帰的に描画する関数
		std::function<void(AbstractObject*)> RenderObjectShadow = [&](AbstractObject* obj) {
			if (!obj) return;

			bool isSaveObject = std::find(_SaveObjects.begin(), _SaveObjects.end(), obj) != _SaveObjects.end();
			if (isSaveObject) return;

			// ModelRenderComponentを持つオブジェクトのみ描画
			auto modelComps = obj->GetComponentsByType<ModelRenderComponent>();
			for (auto* modelComp : modelComps) {
				if (!modelComp) continue;

				auto model = modelComp->GetModel();
				if (!model) continue;

				// ワールド行列を計算（GetWorldTransformで親の変換も含まれる）
				Transform t = obj->GetWorldTransform();
				DirectX::XMMATRIX world = DirectX::XMMatrixScaling(t.scale.x, t.scale.y, t.scale.z) *
					DirectX::XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z) *
					DirectX::XMMatrixTranslation(t.position.x, t.position.y, t.position.z);

				// グローバルオフセット・スケール・回転を適用
				DirectX::XMFLOAT3 globalOffset = modelComp->GetGlobalOffset();
				DirectX::XMFLOAT3 globalScale = modelComp->GetGlobalScale();
				DirectX::XMFLOAT3 globalRotation = modelComp->GetGlobalRotation();

				DirectX::XMMATRIX globalTransform =
					DirectX::XMMatrixScaling(globalScale.x, globalScale.y, globalScale.z) *
					DirectX::XMMatrixRotationRollPitchYaw(globalRotation.x, globalRotation.y, globalRotation.z) *
					DirectX::XMMatrixTranslation(globalOffset.x, globalOffset.y, globalOffset.z);

				DirectX::XMMATRIX finalWorld = globalTransform * world;

				// 定数バッファ更新
				ShadowCB shadowData;
				shadowData.lightViewProj = DirectX::XMMatrixTranspose(lightViewProj);
				shadowData.world = DirectX::XMMatrixTranspose(finalWorld);
				ctx->UpdateSubresource(shadowVSCB.Get(), 0, nullptr, &shadowData, 0, 0);

				ID3D11Buffer* cbs[] = { shadowVSCB.Get() };
				ctx->VSSetConstantBuffers(0, 1, cbs);

				// ボーン行列をシャドウマップシェーダーにも渡す
				if (model->hasSkin && modelComp->HasBoneMatrices()) {
					modelComp->SetupBoneMatricesForShader(ctx);
				}

				// モデル描画
				UINT stride = sizeof(ModelVertex);
				UINT offset = 0;
				ID3D11Buffer* vb = model->vb.Get();
				ID3D11Buffer* ib = model->ib.Get();

				if (!vb || !ib) continue;

				ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
				ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
				ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				// 入力レイアウトを設定
				const void* bc = nullptr;
				size_t bcSize = 0;
				if (sm->GetVSBytecode("VS_ShadowMap", &bc, &bcSize)) {
					static Microsoft::WRL::ComPtr<ID3D11InputLayout> shadowLayout;
					if (!shadowLayout) {
						D3D11_INPUT_ELEMENT_DESC desc[] = {
							{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(ModelVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
							{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(ModelVertex, normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
							{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(ModelVertex, tangent), D3D11_INPUT_PER_VERTEX_DATA, 0 },
							{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ModelVertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0 },
							{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, offsetof(ModelVertex, boneIndices), D3D11_INPUT_PER_VERTEX_DATA, 0 },
							{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(ModelVertex, boneWeights), D3D11_INPUT_PER_VERTEX_DATA, 0 },
						};
						DirectX11::GetInstance()->GetDevice()->CreateInputLayout(desc, _countof(desc), bc, bcSize, shadowLayout.GetAddressOf());
					}
					if (shadowLayout) {
						ctx->IASetInputLayout(shadowLayout.Get());
					}
				}

				// 全サブメッシュを描画
				for (const auto& submesh : model->submeshes) {
					ctx->DrawIndexed(submesh.indexCount, submesh.indexOffset, 0);
				}
			}

			// 子オブジェクトも再帰的に描画
			for (auto* child : obj->GetChildren()) {
				RenderObjectShadow(child);
			}
			};

		// ルートオブジェクトから再帰的に描画
		for (auto& obj : _objects) {
			if (obj && obj->GetParent() == nullptr) {
				RenderObjectShadow(obj);
			}
		}
	}

	// 元のレンダーターゲットとビューポートに戻す
	ctx->OMSetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.Get());
	ctx->RSSetViewports(1, &oldViewport);
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

Layer* AbstractScene::GetLayer(int index)
{
	if (index < 0 || index >= MAX_LAYER_COUNT) return nullptr;
	return _layers[index];
}