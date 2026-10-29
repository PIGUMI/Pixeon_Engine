// シーンクラス
// オブジェクトなどの管理を行う

#pragma once
#include "ComponentManager.h"
#include "CameraComponent.h"
#include "CollisionManager.h"
#include "LayerSettings.h"
#include <string>
#include <vector>
#include <mutex>
#include <Windows.h>
#include "RHI/RHI.h"

class AbstractObject;
class LightComponent;
class EffectComponent;

class AbstractScene
{
public:
	AbstractScene() {}
	virtual ~AbstractScene();
	virtual void Init();
	virtual void BeginPlay();
	virtual void EditUpdate();
	virtual void PlayUpdate();
	virtual void Draw(int Layer);
	virtual void DrawUI();

	void UpdateEffects(float deltaTime);
	void DrawEffects(int Layer, CameraComponent* camera);
public:
	bool AddObject(AbstractObject* obj);
public:
	void SaveToFile();
	void LoadToFile();
	void AddObjectLocal(AbstractObject* obj);
	void RemoveObject(AbstractObject* obj);
public:
	void SetName(std::string name) { _name = name; }
	std::string GetName() { return _name; }
	std::vector<AbstractObject*> GetObjects() { return _objects; }

	CameraComponent* GetMainCamera() { return _MainCamera; }
	void SetMainCamera(CameraComponent* camera);
	int GetMainCameraNumber() { return _MainCameraNumber; }
	void SetMainCameraNumber(int num);

	AbstractObject* FindObjectByName(const char* name);

	std::vector<LightComponent*>* GetLights() { return &_lights; }

	void RegisterLight(LightComponent* l);
	RHI::Texture* GetShadowMap() const { return m_shadowMap.get(); }
	void DrawForGBuffer(int Layer);
	void PrepareShadowAndLights(int Layer);
	void DrawForwardObjects(int Layer);  // ImageRender/LineRenderer等のForward描画
	RHI::Buffer* GetLightArrayCB()  const { return m_lightArrayCB.get(); }
	RHI::Buffer* GetLightCountCB()  const { return m_lightCountCB.get(); }
	void UnregisterLight(LightComponent* l);

	btDiscreteDynamicsWorld* GetPhysicsWorld() { return pPhysicsWorld; }
	CollisionManager* GetCollisionManager() { return _collisionManager; }

	std::vector<Layer*> GetLayers() { return _layers; }
	Layer* GetLayer(int index);
private:
	void ProcessThreadSafeAdditions();
	void UploadLightsToGPU();

	void InitPhysics();
	void CleanupPhysics();
	void CleanupAndReinitializePhysics();
	bool CreateShadowMapResources();
	void RenderShadowMap();
	DirectX::XMMATRIX GetLightViewProjection();
	std::vector<EffectComponent*> CollectEffectComponents(int layer);
private:
	std::string _name = "DefaultScene";

	std::vector<AbstractObject*> _objects;
	std::vector<AbstractObject*> _SaveObjects;
	std::vector<AbstractObject*> _ToBeRemoved;
	std::vector<AbstractObject*> _ToBeAdded;
	std::vector<AbstractObject*> _ToBeAddedBuffer;
	std::vector<LightComponent*> _lights;
	std::vector<Layer*> _layers;
	std::mutex _mtx;
	CameraComponent* _MainCamera = nullptr;
	CollisionManager* _collisionManager = nullptr;
	int _MainCameraNumber = -1;
	int editorCameraNumber = -1;
	bool EndPlayCalled = false;
	bool InGame = false;

	btDiscreteDynamicsWorld* pPhysicsWorld = nullptr;
	btDefaultCollisionConfiguration* pCollisionConfig = nullptr;
	btCollisionDispatcher* pDispatcher = nullptr;
	btDbvtBroadphase* pOverlappingPairCache = nullptr;
	btSequentialImpulseConstraintSolver* pSolver = nullptr;
	RHI::TextureRef m_shadowMap;
	RHI::SamplerRef m_shadowSampler;
	RHI::BufferRef m_shadowVSCB;
	RHI::BufferRef m_shadowPSCB;
	RHI::InputLayoutRef m_shadowLayout;

	static constexpr int SHADOW_MAP_SIZE = 8192;

	// ライト定数バッファ（LightingPassに渡すため保持）
	RHI::BufferRef m_lightArrayCB;
	RHI::BufferRef m_lightCountCB;
};
