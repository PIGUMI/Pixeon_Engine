// IScript.cpp
#include "IScript.h"
#include <unordered_map>
#include <mutex>
#include <iostream>

static std::unordered_map<component, IScript*> g_scriptInstances;
static std::mutex g_scriptInstancesMutex;

// ========================================
// コリジョンコールバック
// ========================================
void OnEnterCallback(component collision, const APICollisionInfo* info)
{
	std::lock_guard<std::mutex> lock(g_scriptInstancesMutex);
	auto it = g_scriptInstances.find(collision);
	if (it != g_scriptInstances.end() && it->second != nullptr) {
		it->second->OnCollisionEnter(info);
	}
}

void OnStayCallback(component collision, const APICollisionInfo* info)
{
	std::lock_guard<std::mutex> lock(g_scriptInstancesMutex);
	auto it = g_scriptInstances.find(collision);
	if (it != g_scriptInstances.end() && it->second != nullptr) {
		it->second->OnCollisionStay(info);
	}
}

void OnExitCallback(component collision, const APICollisionInfo* info)
{
	std::lock_guard<std::mutex> lock(g_scriptInstancesMutex);
	auto it = g_scriptInstances.find(collision);
	if (it != g_scriptInstances.end() && it->second != nullptr) {
		it->second->OnCollisionExit(info);
	}
}

// ========================================
// IScript
// ========================================
IScript::~IScript()
{
	CleanupAllResources();
	UnregisterAllCollisions();
}

void IScript::SetParentObject(object obj)
{
	if (_parentObject) {
		delete _parentObject;
	}
	_parentObject = new Object(obj);
}

void IScript::SetParentScene(scene scn)
{
	if (_parentScene) {
		delete _parentScene;
	}
	_parentScene = new Scene(scn);
}

void IScript::BeginPlay()
{
	if (!_parentObject) {
		return;
	}

	const char* collisionTypes[] = {
		"BoxCollision",
		"CapsuleCollision",
	};

	for (const char* typeName : collisionTypes) {
		component collisionComp = nullptr;
		if (FindComponent(_parentObject->GetHandle(), typeName, &collisionComp) == PN_SUCCESS) {
			RegisterCollisionComponent(collisionComp);
		}
	}
}

void IScript::Update(float DeltaTime)
{
	// 継承先で実装
}

void IScript::FixedUpdate(float DeleteTime)
{
	// 継承先で実装
}

void IScript::EndPlay()
{
	CleanupAllResources();
	UnregisterAllCollisions();
}

void IScript::CallCustom(const std::string& functionName)
{
	// 継承先で実装
}

void IScript::ClearParent()
{
	if (_parentObject) {
		delete _parentObject;
		_parentObject = nullptr;
	}
	if (_parentScene) {
		delete _parentScene;
		_parentScene = nullptr;
	}
	_ownerComponent = nullptr;
}

// ========================================
// コリジョン管理
// ========================================
void IScript::RegisterCollisionComponent(component collision)
{
	if (collision == nullptr) {
		return;
	}

	APIResult result;
	result = CollisionSetCollisionEnterCallback(collision, OnEnterCallback);
	if (result != PN_SUCCESS) {
		return;
	}

	result = CollisionSetCollisionStayCallback(collision, OnStayCallback);
	if (result != PN_SUCCESS) {
		return;
	}

	result = CollisionSetCollisionExitCallback(collision, OnExitCallback);
	if (result != PN_SUCCESS) {
		return;
	}

	std::lock_guard<std::mutex> lock(g_scriptInstancesMutex);
	g_scriptInstances[collision] = this;

	_registeredCollisions.push_back(collision);
}

void IScript::UnregisterAllCollisions()
{
	std::lock_guard<std::mutex> lock(g_scriptInstancesMutex);

	for (component collision : _registeredCollisions) {
		auto it = g_scriptInstances.find(collision);
		if (it != g_scriptInstances.end()) {
			g_scriptInstances.erase(it);
		}
	}

	_registeredCollisions.clear();
}

void IScript::OnCollisionEnter(const APICollisionInfo* info) {}
void IScript::OnCollisionStay(const APICollisionInfo* info) {}
void IScript::OnCollisionExit(const APICollisionInfo* info) {}

// ========================================
// 自動メモリ管理
// ========================================
Object* IScript::CreateManagedObject(const std::string& prefabName)
{
	if (!_parentScene) {
		std::cerr << "[IScript] Error: _parentScene is null" << std::endl;
		return nullptr;
	}

	Object* prefab = _parentScene->FindPrefabObject(prefabName);
	if (!prefab) {
		std::cerr << "[IScript] Error: Prefab not found: " << prefabName << std::endl;
		return nullptr;
	}

	Object* newObj = _parentScene->AddObject(prefab);
	if (newObj) {
		RegisterManagedObject(newObj);
	}

	return newObj;
}

void IScript::RegisterManagedObject(Object* obj)
{
	if (!obj) return;
	_createdObjects.push_back(obj);
}

void IScript::ReleaseAllManagedResources()
{
	CleanupAllResources();
}

void IScript::CleanupAllResources()
{
	// 生成したオブジェクトをシーンから削除
	if (_parentScene) {
		for (auto obj : _createdObjects) {
			if (obj) {
				try {
					_parentScene->RemoveObject(obj);
					delete obj;  // ラッパーのみ削除
				}
				catch (const std::exception& e) {
					std::cerr << "[IScript] Exception during object cleanup: "
						<< e.what() << std::endl;
				}
			}
		}
	}
	_createdObjects.clear();

	// カスタムリソースの解放
	for (auto& cleanup : _managedResources) {
		if (cleanup) {
			try {
				cleanup();
			}
			catch (const std::exception& e) {
				std::cerr << "[IScript] Exception during resource cleanup: "
					<< e.what() << std::endl;
			}
		}
	}
	_managedResources.clear();

	// shared_ptr のクリア
	_sharedResources.clear();
}