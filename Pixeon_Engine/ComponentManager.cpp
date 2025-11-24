#include "ComponentManager.h"
#include "Component.h"
#include "CameraComponent.h"
#include "Geometry.h"
#include "ModelRender.h"
#include "LightComponent.h"
#include "ImageRender.h"
#include "ScripComponent.h"
#include "Component.h"
#include "AnimationComponent.h"
//#include "AnimationComponentV2.h"
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
	_ComponentName[(int)COMPONENT_TYPE::GEOMETRY] = "Geometry";
	_ComponentName[(int)COMPONENT_TYPE::MODEL] = "Model";
	_ComponentName[(int)COMPONENT_TYPE::LIGHT] = "Light";
	_ComponentName[(int)COMPONENT_TYPE::IMAGE] = "ImageRender";
	_ComponentName[(int)COMPONENT_TYPE::SCRIPT] = "Script";
	_ComponentName[(int)COMPONENT_TYPE::ANIMATION] = "Animation";
}

Component* ComponentManager::AddComponent(Object* owner, COMPONENT_TYPE type) {
	if (!owner) return nullptr;

	Component* component = nullptr;

	switch (type)
	{
	case ComponentManager::COMPONENT_TYPE::NONE:

		break;
	case ComponentManager::COMPONENT_TYPE::CAMERA:
		component = owner->AddComponent<CameraComponent>();
		break;
	case ComponentManager::COMPONENT_TYPE::GEOMETRY:
		component = owner->AddComponent<Geometry>();
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
	case ComponentManager::COMPONENT_TYPE::MAX:
		MessageBox(nullptr, "—áŠO‚È’l‚Å‚·\nCode : CMMAX", "Error", MB_OK);
		break;
	default:
		MessageBox(nullptr, "“o˜^‚³‚ê‚Ä‚¢‚Ü‚¹‚ñ\nCode CMFIND", "Error", MB_OK);
		break;
	}
	return component;
}