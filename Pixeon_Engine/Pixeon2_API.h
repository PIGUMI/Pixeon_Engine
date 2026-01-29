#ifndef PIXEON2_API
#define PIXEON2_API
#include "API.h"
#include <string>
#include <DirectXMath.h>



// Wrapper 4 Object
class object {
public:
	object(Object handle) : Handle(handle) {}
	~object() = default;
public:
	std::string GetName();
	void SetName(const std::string& name);

	transform GetTransform();
	void SetTransform(const transform& inTransform);

	DirectX::XMFLOAT3 GetPosition();
	void SetPosition(const DirectX::XMFLOAT3& position);

	DirectX::XMFLOAT3 GetRotation();
	void SetRotation(const DirectX::XMFLOAT3& rotation);

	DirectX::XMFLOAT3 GetScale();
	void SetScale(const DirectX::XMFLOAT3& scale);

	object* FindChildObject(const std::string& name);
public:
	Object Handle;
};

// Wrapper 4 Scene
class scene {
public:
	scene(Scene handle) : Handle(handle) {}
	~scene() = default;
	
	int GetObjectCount();
	object* FindObject(const std::string& name);
	void AddObject(object* obj);
	void RemoveObject(object* obj);
public:
	Scene Handle;
};

#endif // !PIXEON2_API
