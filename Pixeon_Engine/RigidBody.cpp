#include "RigidBody.h"
#include "Object.h"
#include "Scene.h"
#include "EditrGUI.h"
#include <iostream>
#include <sstream>
#include <algorithm>

void RigidBody::Init(Object* Prt)
{
	_Parent = Prt;
	_ComponentName = "RigidBody";
	_Type = ComponentManager::COMPONENT_TYPE::RIGIDBODY;

	pCompoundShape_ = new btCompoundShape();

	CreateRigidBody();
}

void RigidBody::BeginPlay()
{
}