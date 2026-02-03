// IScript.cpp
#include "IScript.h"
#include <unordered_map>
#include <mutex>

static std::unordered_map<Component, IScript*> g_scriptInstances;
static std::mutex g_scriptInstancesMutex;

void OnEnterCallback(Component collision, const APICollisionInfo* info)
{
	std::lock_guard<std::mutex> lock(g_scriptInstancesMutex);
	auto it = g_scriptInstances.find(collision);
	if (it != g_scriptInstances.end() && it->second != nullptr) {
		it->second->OnCollisionEnter(info);
	}
}

void OnStayCallback(Component collision, const APICollisionInfo* info)
{
	std::lock_guard<std::mutex> lock(g_scriptInstancesMutex);
	auto it = g_scriptInstances.find(collision);
	if (it != g_scriptInstances.end() && it->second != nullptr) {
		it->second->OnCollisionStay(info);
	}
}

void OnExitCallback(Component collision, const APICollisionInfo* info)
{
	std::lock_guard<std::mutex> lock(g_scriptInstancesMutex);
	auto it = g_scriptInstances.find(collision);
	if (it != g_scriptInstances.end() && it->second != nullptr) {
		it->second->OnCollisionExit(info);
	}
}

IScript::~IScript()
{
	UnregisterAllCollisions();
}

void IScript::BeginPlay()
{
	if (_parentObject == nullptr) {
		return;
	}

	const char* collisionTypes[] = {
		"BoxCollision",
		"CapsuleCollision",
	};

	for (const char* typeName : collisionTypes) {
		Component collisionComp = nullptr;
		if (FindComponent(_parentObject, typeName, &collisionComp) == PN_SUCCESS) {
			RegisterCollisionComponent(collisionComp);
		}
	}
}

void IScript::RegisterCollisionComponent(Component collision)
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

	for (Component collision : _registeredCollisions) {
		auto it = g_scriptInstances.find(collision);
		if (it != g_scriptInstances.end()) {
			g_scriptInstances.erase(it);
		}
	}

	_registeredCollisions.clear();
}

void IScript::Update(float DeltaTime)
{
}

void IScript::EndPlay()
{
	UnregisterAllCollisions();
}

void IScript::CallCustom(const std::string& functionName)
{
}

void IScript::ClearParent()
{
	_parentObject = nullptr;
	_parentScene = nullptr;
	_ownerComponent = nullptr;
}

void IScript::OnCollisionEnter(const APICollisionInfo* info)
{
}

void IScript::OnCollisionStay(const APICollisionInfo* info)
{
}

void IScript::OnCollisionExit(const APICollisionInfo* info)
{
}