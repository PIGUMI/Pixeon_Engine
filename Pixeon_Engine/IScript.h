// IScript.h
#ifndef _ISCRIPT_H_
#define _ISCRIPT_H_

#include <string>
#include <vector>
#include "API.h"

class IScript
{
public:
	virtual ~IScript();
	virtual void BeginPlay();
	virtual void Update(float DeltaTime);
	virtual void EndPlay();

	virtual void CallCustom(const std::string& functionName);
	void SetParentObject(Object obj) { _parentObject = obj; }
	void SetParentScene(Scene scene) { _parentScene = scene; }

public:
	virtual void OnCollisionEnter(const APICollisionInfo* info);
	virtual void OnCollisionStay(const APICollisionInfo* info);
	virtual void OnCollisionExit(const APICollisionInfo* info);

protected:
	Object _parentObject = nullptr;
	Scene _parentScene = nullptr;
private:
	std::vector<Component> _registeredCollisions;
	void RegisterCollisionComponent(Component collision);
	void UnregisterAllCollisions();
};

#endif // _ISCRIPT_H_