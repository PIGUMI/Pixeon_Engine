// シーンクラス
// オブジェクトなどの管理を行う

#pragma once
#include "ComponentManager.h"
#include "CameraComponent.h"
#include "CollisionManager.h"
#include <string>
#include <vector>
#include <mutex>

class AbstractObject;
class LightComponent;

class Scene
{
public:
	Scene() {}
	virtual ~Scene();
	virtual void Init();
	virtual void BeginPlay();
	virtual void EditUpdate();
	virtual void PlayUpdate();
	virtual void Draw(int Layer);
	virtual void DrawUI();

public: // オブジェクトの追加と削除
	bool AddObject(AbstractObject* obj);
public: // セーブとロード
	void SaveToFile();
	void LoadToFile();
	void AddObjectLocal(AbstractObject* obj);
	void RemoveObject(AbstractObject* obj);
public: // Setter And Getter
	void SetName(std::string name) { _name = name; }
	std::string GetName() { return _name; }
	// すべてのオブジェクトを取得
	std::vector<AbstractObject*> GetObjects() { return _objects; }

	CameraComponent* GetMainCamera() { return _MainCamera; }
	void SetMainCamera(CameraComponent* camera) { _MainCamera = camera; }
	int GetMainCameraNumber() { return _MainCameraNumber; }
	void SetMainCameraNumber(int num);

	AbstractObject* FindObjectByName(const char* name);

	std::vector<LightComponent*>* GetLights() { return &_lights; }

	void RegisterLight(LightComponent* l);
	void UnregisterLight(LightComponent* l);

	btDiscreteDynamicsWorld* GetPhysicsWorld() { return pPhysicsWorld; }
	CollisionManager* GetCollisionManager() { return _collisionManager; }

private://内部処理
	void ProcessThreadSafeAdditions();
	void UploadLightsToGPU();

	void InitPhysics();
	void CleanupPhysics();
	void CleanupAndReinitializePhysics();
private:
	std::string _name = "DefaultScene";

	std::vector<AbstractObject*> _objects;
	std::vector<AbstractObject*> _SaveObjects;
	std::vector<AbstractObject*> _ToBeRemoved;
	std::vector<AbstractObject*> _ToBeAdded;
	std::vector<AbstractObject*> _ToBeAddedBuffer;
	std::vector<LightComponent*> _lights;
	std::mutex _mtx;
	CameraComponent* _MainCamera = nullptr;
	CollisionManager* _collisionManager = nullptr;
	int _MainCameraNumber = -1;
	bool EndPlayCalled = false;
	bool InGame = false;

	// 物理演算に関する変数
	btDiscreteDynamicsWorld* pPhysicsWorld = nullptr;
	btDefaultCollisionConfiguration* pCollisionConfig = nullptr;
	btCollisionDispatcher* pDispatcher = nullptr;
	btDbvtBroadphase* pOverlappingPairCache = nullptr;
	btSequentialImpulseConstraintSolver* pSolver = nullptr;
};
