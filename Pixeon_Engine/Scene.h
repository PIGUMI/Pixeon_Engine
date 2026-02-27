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
#include <wrl/client.h>
#include <d3d11.h>

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
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_shadowMapTexture;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_shadowMapDSV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_shadowMapSRV;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> m_shadowSampler;

	static constexpr int SHADOW_MAP_SIZE = 4096;
};
