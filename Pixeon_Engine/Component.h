#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include "IMGUI/imgui_impl_win32.h"
#include "ComponentManager.h"
#include "Object.h"

class Component
{
public:
	virtual void Init(Object* Prt) {}
	virtual void BeginPlay() {}
	virtual void EditUpdate() {}
	virtual void InGameUpdate() {}
	virtual void Draw(int Layer) {}
	virtual void UInit() {}
	virtual void DrawInspector() {}

	void SetParent(Object* parent) { _Parent = parent; }
	void SetLayerNumber(int layer) { _LayerNumber = layer; }
	int GetLayerNumber() const { return _LayerNumber; }
public:
	virtual void SaveToFile(std::ostream& out) {}
	virtual void LoadFromFile(std::istream& in) {}

	Object* GetParent() const { return _Parent; }

	std::string GetComponentName() const { return _ComponentName; }
	void SetComponentName(std::string name) { _ComponentName = name; }

	ComponentManager::COMPONENT_TYPE GetComponentType() const { return _Type; }
	void SetComponentType(ComponentManager::COMPONENT_TYPE type) { _Type = type; }

protected:
	std::string _ComponentName;
	Object* _Parent;
	int _LayerNumber = 0;
	ComponentManager::COMPONENT_TYPE _Type = ComponentManager::COMPONENT_TYPE::NONE;
};
