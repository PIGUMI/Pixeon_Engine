#include "ComponentManager.h"
#include "Component.h"
#include "CameraComponent.h"
#include "ModelRender.h"
#include "LightComponent.h"
#include "ImageRender.h"
#include "ScripComponent.h"
#include "AnimationComponent.h"
#include "RigidBody.h"
#include "BoxCollision.h"
#include "CapsuleCollision.h"
#include "Animator2DComponent.h"
#include <Windows.h>

ComponentManager* ComponentManager::_instance;

ComponentManager* ComponentManager::GetInstance() {
	if (_instance == nullptr) {
		_instance = new ComponentManager();
	}
	return _instance;
}

void ComponentManager::DestroyInstance() {
	if (_instance) {
		delete _instance;
		_instance = nullptr;
	}
}

void ComponentManager::Init() {
	_ComponentName[(int)COMPONENT_TYPE::CAMERA] = "Camera";
	_ComponentName[(int)COMPONENT_TYPE::MODEL] = "Model";
	_ComponentName[(int)COMPONENT_TYPE::LIGHT] = "Light";
	_ComponentName[(int)COMPONENT_TYPE::IMAGE] = "ImageRender";
	_ComponentName[(int)COMPONENT_TYPE::SCRIPT] = "Script";
	_ComponentName[(int)COMPONENT_TYPE::ANIMATION] = "Animation";
	_ComponentName[(int)COMPONENT_TYPE::RIGIDBODY] = "RigidBody";
	_ComponentName[(int)COMPONENT_TYPE::BOX_COLLISION] = "BoxCollision";
	_ComponentName[(int)COMPONENT_TYPE::CAPSULE_COLLISION] = "CapsuleCollision";
	_ComponentName[(int)COMPONENT_TYPE::ANIMATOR2D] = "Animator2D";
}

AbstractComponent* ComponentManager::AddComponent(AbstractObject* owner, COMPONENT_TYPE type) {
	if (!owner) return nullptr;

	AbstractComponent* component = nullptr;

	switch (type)
	{
	case ComponentManager::COMPONENT_TYPE::CAMERA:
		component = owner->AddComponent<CameraComponent>();
		break;
	case ComponentManager::COMPONENT_TYPE::MODEL:
		component = owner->AddComponent<ModelRenderComponent>();
		break;
	case ComponentManager::COMPONENT_TYPE::LIGHT:
		component = owner->AddComponent<LightComponent>();
		break;
	case ComponentManager::COMPONENT_TYPE::IMAGE:
		component = owner->AddComponent<ImageRender>();
		break;
	case ComponentManager::COMPONENT_TYPE::SCRIPT:
		component = owner->AddComponent<ScripComponent>();
		break;
	case ComponentManager::COMPONENT_TYPE::ANIMATION:
		component = owner->AddComponent<AnimationComponent>();
		break;
	case ComponentManager::COMPONENT_TYPE::RIGIDBODY:
		component = owner->AddComponent<RigidBody>();
		break;
	case ComponentManager::COMPONENT_TYPE::BOX_COLLISION:
		component = owner->AddComponent<BoxCollision>();
		break;
	case ComponentManager::COMPONENT_TYPE::CAPSULE_COLLISION:
		component = owner->AddComponent<CapsuleCollision>();
		break;
	case ComponentManager::COMPONENT_TYPE::ANIMATOR2D:
		component = owner->AddComponent<Animator2DComponent>();
		break;
	case ComponentManager::COMPONENT_TYPE::MAX:
		MessageBox(nullptr, "—áŠO‚È’l‚Å‚·\nCode : CMMAX", "Error", MB_OK);
		break;
	default:
		MessageBox(nullptr, "“o˜^‚³‚ê‚Ä‚¢‚Ü‚¹‚ñ\nCode CMFIND", "Error", MB_OK);
		break;
	}
	return component;
}