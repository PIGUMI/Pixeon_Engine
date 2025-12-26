#include "Object.h"
#include "Component.h"
#include "ImageRender.h"
#include "Animator2DComponent.h"

void AbstractObject::Init() {
}

void AbstractObject::BeginPlay() {
	for (auto comp : _components)if (comp)comp->BeginPlay();
}

void AbstractObject::EditUpdate() {
	for (auto comp : _components) {
		if (comp && comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA) continue;
		if (comp)comp->EditUpdate();
	}
}

void AbstractObject::InGameUpdate() {
	for (auto comp : _components) {
		if (comp && comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA) continue;
		if (comp)comp->InGameUpdate();
	}
}

void AbstractObject::Draw(int Layer) {
	for (auto comp : _components)
	{
		comp->Draw(Layer);
	}
}

void AbstractObject::UInit() {
	for (auto comp : _components) {
		comp->UInit();
		delete comp;
	}
	_components.clear();
}

AbstractObject* AbstractObject::Clone() {
	AbstractObject* newObj = new AbstractObject();
	newObj->_transform = this->_transform;
	newObj->_ObjectName = this->_ObjectName;
	newObj->SetParentScene(this->GetParentScene());
	for (auto comp : _components) {
		if (comp) {
			AbstractComponent* newComp = ComponentManager::GetInstance()->AddComponent(newObj, comp->GetComponentType());
			if (newComp) {
				newComp->SetComponentName(comp->GetComponentName());
				std::stringstream ss;
				comp->SaveToFile(ss);
				newComp->LoadFromFile(ss);
			}
		}
	}
	return newObj;
}

AbstractComponent* AbstractObject::GetComponent(const std::string& name)
{
	for (auto comp : _components) {
		if (comp->GetComponentName() == name) {
			return comp;
		}
	}
	return nullptr;
}

std::vector<AbstractComponent*> AbstractObject::GetComponentsByTypeID(int typeID)
{
	std::vector<AbstractComponent*> result;
	for (auto comp : _components) {
		if (static_cast<int>(comp->GetComponentType()) == typeID) {
			result.push_back(comp);
		}
	}
	return result;
}

void AbstractObject::RemoveComponent(AbstractComponent* comp) {
	if (comp == nullptr) return;
	auto it = std::remove(_components.begin(), _components.end(), comp);
	if (it != _components.end()) {
		_components.erase(it, _components.end());
		comp->UInit();
		delete comp;
	}
}