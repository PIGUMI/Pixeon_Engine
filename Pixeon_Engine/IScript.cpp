#include "IScript.h"
#include <unordered_map>

static std::unordered_map<Component, IScript*> g_scriptInstances;


void OnEnterCallback(Component collision, const APICollisionInfo* info)
{
	auto it = g_scriptInstances.find(collision);
	if (it != g_scriptInstances.end()) {
		it->second->OnCollisionEnter(info);
	}
}

void OnStayCallback(Component collision, const APICollisionInfo* info)
{
	auto it = g_scriptInstances.find(collision);
	if (it != g_scriptInstances.end()) {
		it->second->OnCollisionStay(info);
	}
}

void OnExitCallback(Component collision, const APICollisionInfo* info)
{
	auto it = g_scriptInstances.find(collision);
	if (it != g_scriptInstances.end()) {
		it->second->OnCollisionExit(info);
	}
}


void IScript::BeginPlay() {
	APIResult result;
	Component collisionComp;
	if (FindComponent(_parentObject, "BoxCollision", &collisionComp) == PN_SUCCESS)
	{
		g_scriptInstances[collisionComp] = this;
		CollisionSetCollisionEnterCallback(collisionComp, OnEnterCallback);
		CollisionSetCollisionStayCallback(collisionComp, OnStayCallback);
		CollisionSetCollisionExitCallback(collisionComp, OnExitCallback);
	}
	if (FindComponent(_parentObject, "CapsuleCollision", &collisionComp) == PN_SUCCESS)
	{
		g_scriptInstances[collisionComp] = this;
		CollisionSetCollisionEnterCallback(collisionComp, OnEnterCallback);
		CollisionSetCollisionStayCallback(collisionComp, OnStayCallback);
		CollisionSetCollisionExitCallback(collisionComp, OnExitCallback);
	}
}

void IScript::Update(float DeltaTime) {
}

void IScript::EndPlay() {
}

void IScript::CallCustom(const std::string& functionName)
{
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
