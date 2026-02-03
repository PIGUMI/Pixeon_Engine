// IScript.h
#ifndef _ISCRIPT_H_
#define _ISCRIPT_H_

#include <string>
#include <vector>
#include "API.h"
#include "Scriptproperty.h"

class ScripComponent;

class IScript : public ScriptPropertyBase
{
public:
	virtual ~IScript();
	virtual void BeginPlay();
	virtual void Update(float DeltaTime);
	virtual void EndPlay();

	virtual void CallCustom(const std::string& functionName);

	void SetParentObject(Object obj) { _parentObject = obj; }
	void SetParentScene(Scene scene) { _parentScene = scene; }
	void SetOwnerComponent(ScripComponent* owner) { _ownerComponent = owner; }

	ScripComponent* GetOwnerComponent() const { return _ownerComponent; }

	void ClearParent();

public:
	virtual void OnCollisionEnter(const APICollisionInfo* info);
	virtual void OnCollisionStay(const APICollisionInfo* info);
	virtual void OnCollisionExit(const APICollisionInfo* info);

	std::vector<PropertyMetadata> GetProperties() override { return {}; }
	std::string SerializeProperty(const std::string& name) override { return ""; }
	void DeserializeProperty(const std::string& name, const std::string& value) override {}

protected:
	Object _parentObject = nullptr;
	Scene _parentScene = nullptr;
	ScripComponent* _ownerComponent = nullptr;

private:
	std::vector<Component> _registeredCollisions;
	void RegisterCollisionComponent(Component collision);
	void UnregisterAllCollisions();
};

#endif // _ISCRIPT_H_