#include "Scene.h"
#include "System.h"
#include "Object.h"
#include "ComponentManager.h"
#include "ModelRender.h"
#include "ModelManager.h"
#include "SettingManager.h"
#include "AssetManager.h"
#include "MainFrame.h"
#include "EffectManager.h"
#include "Component.h"
#include "LightComponent.h"
#include "EffectComponent.h"
#include "RigidBody.h"
#include "_Geometry.h"
#include "ShaderManager.h"
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

static const int kMaxLights = 8;

namespace {
	struct ShadowPSCB {
		DirectX::XMMATRIX lightViewProj;
	};
	struct ShadowVSCB {
		DirectX::XMMATRIX lightViewProj;
		DirectX::XMMATRIX world;
	};
	struct LightCountCB { int count; float pad[3]; };

	// 定数バッファを 1 つ作る
	RHI::BufferRef ConstantBufferMake(uint32_t size, RHI::Usage usage = RHI::Usage::Default)
	{
		RHI::BufferDesc bd;
		bd.i_Size = size;
		bd.i_BindFlags = RHI::Bind_ConstantBuffer;
		bd.e_Usage = usage;
		return GraphicsSystem::GetInstance()->GetDevice()->BufferCreate(bd);
	}
}

AbstractScene::~AbstractScene()
{
	SaveToFile();

	for (auto& obj : _objects) {
		if (obj) {
			obj->_parentObject = nullptr;
			obj->_children.clear();
		}
	}

	for (auto& obj : _objects) {
		if (obj) {
			obj->UnInit();
			delete obj;
		}
	}
	_objects.clear();

	CleanupPhysics();

	for (auto& obj : _ToBeAdded) {
		if (obj) {
			obj->_parentObject = nullptr;
			obj->_children.clear();
			obj->UnInit();
			delete obj;
		}
	}
	_ToBeAdded.clear();

	for (auto& obj : _SaveObjects) {
		if (obj) {
			obj->_parentObject = nullptr;
			obj->_children.clear();
			obj->UnInit();
			delete obj;
		}
	}
	_SaveObjects.clear();

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

		for (auto& obj : _objects) {
			if (obj) {
				obj->_parentObject = nullptr;
				obj->_children.clear();
				obj->UnInit();
				delete obj;
			}
		}
		_ToBeAdded.clear();
		_ToBeAddedBuffer.clear();
		_objects.clear();

		for (auto& obj : _SaveObjects) {
			if (obj && obj->GetParent() == nullptr) {
				_objects.push_back(obj);

				std::function<void(AbstractObject*)> addChildren = [&](AbstractObject* parent) {
					for (auto child : parent->GetChildren()) {
						if (child) {
							_objects.push_back(child);
							addChildren(child);
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

	for (auto& obj : _objects) {
		if (obj && obj->GetParent() == nullptr) {
			obj->EditUpdate();
		}
	}

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

	for (auto& obj : _objects) {
		if (obj && obj->GetParent() == nullptr) {
			obj->InGameUpdate();
		}
	}

	if (_collisionManager) _collisionManager->Update();

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
	PrepareShadowAndLights(Layer);
	for (auto& obj : _objects) {
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
	// ビルド済みゲームではシーンを書き換えない
	if (!MainFrame::GetInstance()->IsEditorMode()) return;

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
}

void AbstractScene::LoadToFile() {
	// エディタではディレクトリ、ゲームではアーカイブから同じ仮想パスで読む
	const std::string filePath = std::string(VfsPrefix::SCENE) + _name + ".scene";
	std::string text;
	if (!AssetManager::Instance()->TextRead(filePath, text)) {
		OutputDebugStringA(("[AbstractScene] Failed to open:  " + filePath + "\n").c_str());
		return;
	}

	try {
		nlohmann::json sceneData = nlohmann::json::parse(text);

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
		OutputDebugStringA(("[AbstractScene] Parse error: " + filePath + " : " + e.what() + "\n").c_str());
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
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	if (!dev) return;

	if (!m_lightArrayCB) m_lightArrayCB = ConstantBufferMake(sizeof(LightGPU) * kMaxLights, RHI::Usage::Dynamic);
	if (!m_lightCountCB) m_lightCountCB = ConstantBufferMake(sizeof(LightCountCB), RHI::Usage::Dynamic);
	if (!m_lightArrayCB || !m_lightCountCB) return;

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

	const LightCountCB countData{ count, { 0.0f, 0.0f, 0.0f } };
	dev->BufferUpdate(m_lightArrayCB.get(), lights, sizeof(lights));
	dev->BufferUpdate(m_lightCountCB.get(), &countData, sizeof(countData));

	dev->ConstantBufferSet(RHI::ShaderStage::Pixel, 1, m_lightArrayCB.get());
	dev->ConstantBufferSet(RHI::ShaderStage::Pixel, 2, m_lightCountCB.get());
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
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	if (!dev) return false;

	RHI::TextureDesc td;
	td.i_Width = SHADOW_MAP_SIZE;
	td.i_Height = SHADOW_MAP_SIZE;
	td.e_Format = RHI::Format::D32_Float;
	td.i_BindFlags = RHI::Bind_DepthStencil | RHI::Bind_ShaderResource;
	td.c_DebugName = "ShadowMap";
	m_shadowMap = dev->TextureCreate(td, nullptr);

	RHI::SamplerDesc sd;
	sd.e_Filter = RHI::Filter::ComparisonMinMagLinearMipPoint;
	sd.e_AddressU = sd.e_AddressV = sd.e_AddressW = RHI::AddressMode::Border;
	for (float& c : sd.f_BorderColor) c = 1.0f;
	sd.e_Compare = RHI::CompareFunc::LessEqual;
	m_shadowSampler = dev->SamplerCreate(sd);

	return m_shadowMap && m_shadowSampler;
}

DirectX::XMMATRIX AbstractScene::GetLightViewProjection()
{
	DirectX::XMFLOAT3 lightDir(0, -1, 0);

	for (auto* light : _lights) {
		if (light && light->GetType() == LightComponent::LightType::Directional) {
			lightDir = light->GetWorldDirection();
			break;
		}
	}

	DirectX::XMFLOAT3 cameraPos(0, 0, 0);
	if (_MainCamera) {
		cameraPos = _MainCamera->GetPosition();
	}

	DirectX::XMVECTOR lightPos = DirectX::XMVectorSet(
		cameraPos.x - lightDir.x * 80.0f,
		cameraPos.y - lightDir.y * 80.0f,
		cameraPos.z - lightDir.z * 80.0f,
		1.0f
	);
	DirectX::XMVECTOR target = DirectX::XMLoadFloat3(&cameraPos);
	DirectX::XMVECTOR up = DirectX::XMVectorSet(0, 1, 0, 0);

	DirectX::XMMATRIX lightView = DirectX::XMMatrixLookAtLH(lightPos, target, up);

	float size = 20.0f;
	DirectX::XMMATRIX lightProj = DirectX::XMMatrixOrthographicLH(
		size, size,
		0.5f,
		250.0f
	);

	return lightView * lightProj;
}

void AbstractScene::RenderShadowMap() {
	GraphicsSystem* gs = GraphicsSystem::GetInstance();
	RHI::Device* dev = gs->GetDevice();
	if (!dev || !m_shadowMap) return;

	auto* sm = ShaderManager::GetInstance();
	RHI::Shader* shadowVS = sm->GetVertexShader("VS_ShadowMap");
	RHI::Shader* shadowPS = sm->GetPixelShader("PS_ShadowMap");
	if (!shadowVS || !shadowPS) return;

	const RHI::RenderTargetState oldTargets = dev->RenderTargetsGet();
	const RHI::Viewport oldViewport = dev->ViewportGet();

	gs->RenderTargetsSet(0, nullptr, m_shadowMap.get());
	dev->DepthStencilClear(m_shadowMap.get(), true, false, 1.0f, 0);

	if (!m_shadowVSCB) m_shadowVSCB = ConstantBufferMake(sizeof(ShadowVSCB));
	if (!m_shadowLayout) {
		const RHI::InputElement desc[] = {
			{ "POSITION", 0, RHI::Format::R32G32B32_Float, 0, offsetof(ModelVertex, position) },
			{ "NORMAL", 0, RHI::Format::R32G32B32_Float, 0, offsetof(ModelVertex, normal) },
			{ "TANGENT", 0, RHI::Format::R32G32B32A32_Float, 0, offsetof(ModelVertex, tangent) },
			{ "TEXCOORD", 0, RHI::Format::R32G32_Float, 0, offsetof(ModelVertex, uv) },
			{ "BLENDINDICES", 0, RHI::Format::R32G32B32A32_UInt, 0, offsetof(ModelVertex, boneIndices) },
			{ "BLENDWEIGHT", 0, RHI::Format::R32G32B32A32_Float, 0, offsetof(ModelVertex, boneWeights) },
		};
		m_shadowLayout = dev->InputLayoutCreate(desc, (uint32_t)std::size(desc), shadowVS);
	}

	dev->ShaderSet(RHI::ShaderStage::Vertex, shadowVS);
	dev->ShaderSet(RHI::ShaderStage::Pixel, shadowPS);
	dev->InputLayoutSet(m_shadowLayout.get());
	dev->TopologySet(RHI::Topology::TriangleList);

	const DirectX::XMMATRIX lightViewProj = GetLightViewProjection();
	std::function<void(AbstractObject*)> RenderObjectShadow = [&](AbstractObject* obj) {
		if (!obj) return;
		bool isSaveObject = std::find(_SaveObjects.begin(), _SaveObjects.end(), obj) != _SaveObjects.end();
		if (isSaveObject) return;

		for (auto* modelComp : obj->GetComponentsByType<ModelRenderComponent>()) {
			if (!modelComp) continue;
			auto model = modelComp->GetModel();
			if (!model || !model->vb || !model->ib) continue;

			Transform t = obj->GetWorldTransform();
			DirectX::XMMATRIX world = DirectX::XMMatrixScaling(t.scale.x, t.scale.y, t.scale.z) *
				DirectX::XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z) *
				DirectX::XMMatrixTranslation(t.position.x, t.position.y, t.position.z);

			DirectX::XMFLOAT3 globalOffset = modelComp->GetGlobalOffset();
			DirectX::XMFLOAT3 globalScale = modelComp->GetGlobalScale();
			DirectX::XMFLOAT3 globalRotation = modelComp->GetGlobalRotation();
			DirectX::XMMATRIX globalTransform =
				DirectX::XMMatrixScaling(globalScale.x, globalScale.y, globalScale.z) *
				DirectX::XMMatrixRotationRollPitchYaw(globalRotation.x, globalRotation.y, globalRotation.z) *
				DirectX::XMMatrixTranslation(globalOffset.x, globalOffset.y, globalOffset.z);

			ShadowVSCB shadowData;
			shadowData.lightViewProj = DirectX::XMMatrixTranspose(lightViewProj);
			shadowData.world = DirectX::XMMatrixTranspose(globalTransform * world);
			dev->BufferUpdate(m_shadowVSCB.get(), &shadowData, sizeof(shadowData));
			dev->ConstantBufferSet(RHI::ShaderStage::Vertex, 0, m_shadowVSCB.get());

			if (model->hasSkin && modelComp->HasBoneMatrices()) {
				modelComp->SetupBoneMatricesForShader();
			}

			dev->VertexBufferSet(0, model->vb.get(), sizeof(ModelVertex), 0);
			dev->IndexBufferSet(model->ib.get(), RHI::IndexFormat::UInt32, 0);
			for (const auto& submesh : model->submeshes) {
				dev->IndexedDraw(submesh.indexCount, submesh.indexOffset, 0);
			}
		}

		for (auto* child : obj->GetChildren()) {
			RenderObjectShadow(child);
		}
	};

	for (auto& obj : _objects) {
		if (obj && obj->GetParent() == nullptr) {
			RenderObjectShadow(obj);
		}
	}

	dev->RenderTargetsSet(oldTargets.i_Count, oldTargets.p_Colors, oldTargets.p_Depth);
	dev->ViewportSet(oldViewport);
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

void AbstractScene::DrawForwardObjects(int Layer)
{

	if (!InGame)
	{
		if (Layer == 0) {
			DirectX::XMFLOAT4X4 view, proj;
			DirectX::XMFLOAT3 Pos = { 0,0,0 };
			if (_MainCamera) {
				view = _MainCamera->GetViewMatrix();
				proj = _MainCamera->GetProjectionMatrix();
				Pos = _MainCamera->GetWorldPosition();
			}
			Draw1mGrid(20.0f, view, proj, Pos);
		}
	}

	for (auto& obj : _objects) {
		if (obj) obj->Draw(Layer);
	}
}

void AbstractScene::DrawForGBuffer(int Layer)
{
	for (auto& obj : _objects)
	{
		if (!obj) continue;

		for (auto& comp : obj->GetComponents())
		{
			if (!comp) continue;
			auto* mr = dynamic_cast<ModelRenderComponent*>(comp);
			if (mr) mr->DrawForGBuffer(Layer);
		}
	}
}

void AbstractScene::PrepareShadowAndLights(int Layer)
{
	RenderShadowMap();
	UploadLightsToGPU();

	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	if (!m_shadowPSCB) m_shadowPSCB = ConstantBufferMake(sizeof(ShadowPSCB));
	ShadowPSCB shadowData;
	shadowData.lightViewProj = DirectX::XMMatrixTranspose(GetLightViewProjection());
	dev->BufferUpdate(m_shadowPSCB.get(), &shadowData, sizeof(shadowData));

	dev->ConstantBufferSet(RHI::ShaderStage::Pixel, 3, m_shadowPSCB.get());
	dev->TextureSet(RHI::ShaderStage::Pixel, 1, m_shadowMap.get());
	dev->SamplerSet(RHI::ShaderStage::Pixel, 1, m_shadowSampler.get());
}
