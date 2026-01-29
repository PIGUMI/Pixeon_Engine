#include "Pixeon2_API.h"

// Wrapper 4 Object

std::string object::GetName() {
	char nameBuffer[256];
	if (GetObjectName(Handle, nameBuffer, 256) == APIResult::PN_SUCCESS) {
		return std::string(nameBuffer);
	}
	return std::string();
}

void object::SetName(const std::string& name)
{
	SetObjectName(Handle, name.c_str());
}

transform object::GetTransform() {
	transform outTransform;
	if (GetObjectTransform(Handle, &outTransform) == APIResult::PN_SUCCESS) {
		return outTransform;
	}
	return transform{};
}

void object::SetTransform(const transform& inTransform)
{
	SetObjectTransform(Handle, &inTransform);
}

DirectX::XMFLOAT3 object::GetPosition()
{
	Float3 position;
	if (GetObjectPosition(Handle, &position) == APIResult::PN_SUCCESS) {
		return DirectX::XMFLOAT3(position.x, position.y, position.z);
	}
	return DirectX::XMFLOAT3{};
}

void object::SetPosition(const DirectX::XMFLOAT3& position)
{
	Float3 pos{ position.x, position.y, position.z };
	SetObjectPosition(Handle, pos);
}

DirectX::XMFLOAT3 object::GetRotation()
{
	Float3 rotation;
	if (GetObjectRotation(Handle, &rotation) == APIResult::PN_SUCCESS) {
		return DirectX::XMFLOAT3(rotation.x, rotation.y, rotation.z);
	}
	return DirectX::XMFLOAT3{};
}

void object::SetRotation(const DirectX::XMFLOAT3& rotation)
{
	Float3 rot{ rotation.x, rotation.y, rotation.z };
	SetObjectRotation(Handle, rot);
}

DirectX::XMFLOAT3 object::GetScale()
{
	Float3 scale;
	if (GetObjectScale(Handle, &scale) == APIResult::PN_SUCCESS) {
		return DirectX::XMFLOAT3(scale.x, scale.y, scale.z);
	}
	return DirectX::XMFLOAT3{};
}

void object::SetScale(const DirectX::XMFLOAT3& scale)
{
	Float3 scl{ scale.x, scale.y, scale.z };
	SetObjectScale(Handle, scl);
}

object* object::FindChildObject(const std::string& name)
{
	Object childHandle = nullptr;
	if (FindChildObjectByName(Handle, name.c_str(), &childHandle) == APIResult::PN_SUCCESS) {
		return new object(childHandle);
	}
	return nullptr;
}

// Wrapper 4 Scene

int scene::GetObjectCount()
{
	int count = 0;
	SceneGetObjectCount(Handle, &count);
	return count;
}

object* scene::FindObject(const std::string& name)
{
	Object objHandle = nullptr;
	if (FindObjectByName(Handle, name.c_str(), &objHandle) == APIResult::PN_SUCCESS) {
		return new object(objHandle);
	}
	return nullptr;
}

void scene::AddObject(object* obj)
{
	AddObjectToScene(Handle, obj->Handle);
}

void scene::RemoveObject(object* obj)
{
	RemoveObjectFromScene(Handle, obj->Handle);
}

