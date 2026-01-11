#ifndef _OBJECT_H_
#define _OBJECT_H_

#include "Struct.h"
#include <string>
#include <vector>
#include <type_traits>
#include <map>

class AbstractComponent;
class Scene;

class AbstractObject
{
public:
	virtual void Init();
	virtual void BeginPlay();
	virtual void EditUpdate();
	virtual void InGameUpdate();
	virtual void Draw(int Layer);
	virtual void UInit();

	AbstractObject* Clone();
public:
	// Setter And Getter
	Transform GetTransform() { return _transform; }
	void SetTransform(Transform transform) { _transform = transform; }

	// ローカルトランスフォームの取得・設定
	Transform GetLocalTransform() { return _transform; }
	void SetLocalTransform(const Transform& transform) { _transform = transform; }

	// ワールドトランスフォームの取得
	Transform GetWorldTransform();

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

	// ワールド位置の取得
	DirectX::XMFLOAT3 GetWorldPosition();

public:
	// 親子関係の管理
	void SetParent(AbstractObject* parent);
	AbstractObject* GetParent() const { return _parentObject; }
	void RemoveParent();

	void AddChild(AbstractObject* child);
	void RemoveChild(AbstractObject* child);
	std::vector<AbstractObject*>& GetChildren() { return _children; }
	const std::vector<AbstractObject*>& GetChildren() const { return _children; }

public:
	// 名前からコンポーネントを取得
	AbstractComponent* GetComponent(const std::string& name);

	// 型からコンポーネントを取得
	template<typename T = AbstractComponent>
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
	std::vector<AbstractComponent*> GetComponents() { return _components; }

	template<typename T = AbstractComponent>
	std::vector<T*> GetComponentsByType() {
		std::vector<T*> result;
		for (auto comp : _components) {
			T* castedComp = dynamic_cast<T*>(comp);
			if (castedComp) {
				result.push_back(castedComp);
			}
		}
		return result;
	}

	std::vector<AbstractComponent*> GetComponentsByTypeID(int typeID);

	// コンポーネントの削除
	void RemoveComponent(AbstractComponent* comp);

	// コンポーネントの追加
	template<typename T = AbstractComponent>
	T* AddComponent() {
		T* newComp = new T();
		newComp->Init(this);
		// 同一型のコンポーネントが既に存在する場合は名前に番号を付与
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
	std::vector<AbstractComponent*> _components;
	Scene* _ParentScene = nullptr;

	// 親子関係
	AbstractObject* _parentObject = nullptr;
	std::vector<AbstractObject*> _children;

	std::map<std::string, int> _intValues;
	std::map<std::string, float> _floatValues;
	std::map<std::string, bool> _boolValues;

	friend class Scene;
};

#endif // !_OBJECT_H_