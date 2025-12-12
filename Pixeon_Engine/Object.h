#ifndef _OBJECT_H_
#define _OBJECT_H_

#include "Struct.h"
#include <string>
#include <vector>
#include <map>

class Component;
class Scene;

class Object
{
public:
	virtual void Init();
	virtual void BeginPlay();
	virtual void EditUpdate();
	virtual void InGameUpdate();
	virtual void Draw(int Layer);
	virtual void UInit();

	Object* Clone();
public:
	// Setter And Getter
	Transform GetTransform() { return _transform; }
	void SetTransform(Transform transform) { _transform = transform; }

	std::string GetObjectName() { return _ObjectName; }
	void SetObjectName(const std::string& name) { _ObjectName = name; }

	void SetPosition(float x, float y, float z) {
		_transform.position = { x, y, z };
	}

	void SetRotation(float x, float y, float z) {
		_transform.rotation = { x, y, z };
	}

	void SetScale(float x, float y, float z) {
		_transform.scale = { x, y, z };
	}

public:

	// 名前からコンポーネントを取得
	Component* GetComponent(const std::string& name);
	// 型からコンポーネントを取得
	template<typename T = Component>
	T* GetComponent() {
		for (auto comp : _components) {
			T* castedComp = dynamic_cast<T*>(comp);
			if (castedComp) {
				return castedComp;
			}
		}
		return nullptr;
	}
	// 全コンポーネントの取得
	std::vector<Component*> GetComponents() { return _components; }
	// コンポーネントの削除
	void RemoveComponent(Component* comp);
	// コンポーネントの追加
	template<typename T = Component>
	T* AddComponent() {
		T* newComp = new T();
		newComp->Init(this);
		// 同じ型のコンポーネントが既に存在する場合は名前に番号を付与
		std::string baseName = newComp->GetComponentName();

		int count = 1;
		while (GetComponent(newComp->GetComponentName())) {
			newComp->SetComponentName(baseName + std::to_string(count));
			count++;
		}
		_components.push_back(newComp);
		return newComp;
	}

	void SetParentScene(Scene* scene) { _ParentScene = scene; }
	Scene* GetParentScene() const { return _ParentScene; }
public:
	// variable Setter And Getter
	void SetInt(const std::string& key, int value) { _intValues[key] = value; }
	void SetFloat(const std::string& key, float value) { _floatValues[key] = value; }
	void SetBool(const std::string& key, bool value) { _boolValues[key] = value; }
	int GetInt(const std::string& key) { return _intValues[key]; }
	float GetFloat(const std::string& key) { return _floatValues[key]; }
	bool GetBool(const std::string& key) { return _boolValues[key]; }
protected:
	std::string _ObjectName;
	Transform _transform;
	std::vector<Component*> _components;
	Scene* _ParentScene = nullptr;
	std::map<std::string, int>		_intValues;
	std::map<std::string, float>	_floatValues;
	std::map<std::string, bool>		_boolValues;
};

#endif // !_OBJECT_H_