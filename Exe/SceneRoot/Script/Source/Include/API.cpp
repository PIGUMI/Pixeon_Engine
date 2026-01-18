/*
* ファイル名　API
* 説      明  API関数群
*	          PixeonEngineの機能を外部から利用するためのインターフェースを提供します。
*/

#include "API.h"
#include "MainFrame.h"
#include "SceneManger.h"
#include "EngineFrame.h"
#include "ComponentManager.h"
#include "Component.h"
#include "Scene.h"
#include "Object.h"

#include "CameraComponent.h"
#include "LightComponent.h"
#include "ImageRender.h"
#include "ModelRender.h"
#include "AnimationComponent.h"
#include "RigidBody.h"
#include "BoxCollision.h"
#include "Animator2DComponent.h"
#include "Animator2D.h"

#include "Input.h"

#include <string>
#include <cstring>
#include <DirectXMath.h>

// Utility Functions
extern "C" {
	PIXEON_API Float2 CreateFloat2(float x, float y) {
		Float2 result = { x, y };
		return result;
	}

	PIXEON_API Float3 CreateFloat3(float x, float y, float z) {
		Float3 result = { x, y, z };
		return result;
	}

	PIXEON_API Float4 CreateFloat4(float x, float y, float z, float w) {
		Float4 result = { x, y, z, w };
		return result;
	}

	PIXEON_API transform CreateTransform(Float3 pos, Float3 rot, Float3 scale) {
		transform result = { pos, rot, scale };
		return result;
	}

	PIXEON_API transform IdentityTransform() {
		transform result = {
			{ 0.0f, 0.0f, 0.0f },  // position
			{ 0.0f, 0.0f, 0.0f },  // rotation
			{ 1.0f, 1.0f, 1.0f }   // scale
		};
		return result;
	}
};

// Helpers
namespace {
	// Safe string copy with length return
	APIResult SafeStringCopy(const std::string& source, char* buffer, int bufferSize, int* outLength) {
		if (!buffer || bufferSize <= 0) {
			if (outLength) *outLength = 0;
			return PN_ERROR_INVALID_PARAMETER;
		}

		int sourceLength = static_cast<int>(source.length());
		if (outLength) *outLength = sourceLength;

		if (bufferSize <= sourceLength) {
			return PN_ERROR_BUFFER_TOO_SMALL;
		}

		std::strncpy(buffer, source.c_str(), bufferSize - 1);
		buffer[bufferSize - 1] = '\0';
		return PN_SUCCESS;
	}

	Float4 ToFloat4(const DirectX::XMFLOAT4& xmfloat) {
		return CreateFloat4(xmfloat.x, xmfloat.y, xmfloat.z, xmfloat.w);
	}

	DirectX::XMFLOAT4 ToXMFloat4(const Float4& float4) {
		return DirectX::XMFLOAT4(float4.x, float4.y, float4.z, float4.w);
	}

	// Convert DirectX::XMFLOAT3 to Float3
	Float3 ToFloat3(const DirectX::XMFLOAT3& xmfloat) {
		return CreateFloat3(xmfloat.x, xmfloat.y, xmfloat.z);
	}

	// Convert Float3 to DirectX::XMFLOAT3
	DirectX::XMFLOAT3 ToXMFloat3(const Float3& float3) {
		return DirectX::XMFLOAT3(float3.x, float3.y, float3.z);
	}

	// Convert DirectX::XMFLOAT2 to Float2
	Float2 ToFloat2(const DirectX::XMFLOAT2& xmfloat) {
		return CreateFloat2(xmfloat.x, xmfloat.y);
	}

	// Convert Float2 to DirectX::XMFLOAT2
	DirectX::XMFLOAT2 ToXMFloat2(const Float2& float2) {
		return DirectX::XMFLOAT2(float2.x, float2.y);
	}

	BoxCollisionEnterCallback g_BoxCollisionEnterCallback = nullptr;
	BoxCollisionStayCallback  g_BoxCollisionStayCallback = nullptr;
	BoxCollisionExitCallback  g_BoxCollisionExitCallback = nullptr;

	// CollisionInfoからAPICollisionInfoへ変換
	APICollisionInfo ToAPICollisionInfo(const CollisionInfo& info) {
		APICollisionInfo apiInfo;
		apiInfo.HitObject = reinterpret_cast<Object>(info.HitObject);
		apiInfo.HitPoint = CreateFloat3(info.HitPoint.x, info.HitPoint.y, info.HitPoint.z);
		apiInfo.HitNormal = CreateFloat3(info.HitNormal.x, info.HitNormal.y, info.HitNormal.z);
		strncpy(apiInfo.HitObjectName, info.HitObjectName.c_str(), sizeof(apiInfo.HitObjectName));
		apiInfo.HitObjectName[sizeof(apiInfo.HitObjectName) - 1] = '\0';
		apiInfo.Distance = info.Distance;
		return apiInfo;
	}

	// Validate handle
	template<typename T>
	bool ValidateHandle(void* handle, T** outPtr) {
		if (!handle) return false;
		*outPtr = reinterpret_cast<T*>(handle);
		return true;
	}
}

// Scene Functions
extern "C" {
	PIXEON_API APIResult GetCurrentScene(SceneHandle* outScene)
	{
		if (!outScene) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		Scene* currentScene = SceneManger::GetInstance()->GetCurrentScene();
		if (!currentScene) {
			*outScene = nullptr;
			return PN_ERROR_NOT_FOUND;
		}
		*outScene = reinterpret_cast<SceneHandle>(currentScene);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult ChangeScene(const char* sceneName)
	{
		if (!sceneName) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		SceneManger::GetInstance()->ChangeScene(std::string(sceneName));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SceneGetObjectCount(SceneHandle scene, int* outCount)
	{
		if (!outCount) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		Scene* scenePtr = nullptr;
		if (!ValidateHandle<Scene>(scene, &scenePtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outCount = static_cast<int>(scenePtr->GetObjects().size());
	}
	PIXEON_API APIResult FindObjectByName(SceneHandle scene, const char* name, Object* outObject)
	{
		if (!name || !outObject) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		Scene* scenePtr = nullptr;
		if (!ValidateHandle<Scene>(scene, &scenePtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AbstractObject* obj = scenePtr->FindObjectByName(name);
		if (!obj) {
			*outObject = nullptr;
			return PN_ERROR_NOT_FOUND;
		}
		*outObject = reinterpret_cast<Object>(obj);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult FindPrefabObjectByName(const char* name, Object* outObject)
	{
		if (!name || !outObject) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* obj = EngineFrame::GetInstance()->GetPrefabByName(std::string(name));
		if (!obj) {
			*outObject = nullptr;
			return PN_ERROR_NOT_FOUND;
		}
		*outObject = reinterpret_cast<Object>(obj);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult FindChildObjectByName(Object parentObject, const char* name, Object* outObject)
	{
		if (!name || !outObject) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* parentObjPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(parentObject, &parentObjPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AbstractObject* childObj = parentObjPtr->FindChildByName(name);
		if (!childObj) {
			*outObject = nullptr;
			return PN_ERROR_NOT_FOUND;
		}
		*outObject = reinterpret_cast<Object>(childObj);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult AddObjectToScene(SceneHandle scene, Object object)
	{
		if (!object) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		Scene* scenePtr = nullptr;
		if (!ValidateHandle<Scene>(scene, &scenePtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		scenePtr->AddObject(objPtr);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RemoveObjectFromScene(SceneHandle scene, Object object)
	{
		if (!object) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		Scene* scenePtr = nullptr;
		if (!ValidateHandle<Scene>(scene, &scenePtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		scenePtr->RemoveObject(objPtr);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetMainCamera(int inCameraNumber)
	{
		Scene* currentScene = SceneManger::GetInstance()->GetCurrentScene();
		if (!currentScene) {
			return PN_ERROR_NOT_FOUND;
		}
		currentScene->SetMainCameraNumber(inCameraNumber);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetMainCamera(int* outCameraNumber)
	{
		if (!outCameraNumber) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		Scene* currentScene = SceneManger::GetInstance()->GetCurrentScene();
		if (!currentScene) {
			return PN_ERROR_NOT_FOUND;
		}
		*outCameraNumber = currentScene->GetMainCameraNumber();
	}
};

// Object Functions
extern "C" {
	PIXEON_API APIResult GetObjectName(Object object, char* outName, int bufferSize)
	{
		if (!outName || bufferSize <= 0) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		std::string name = objPtr->GetObjectName();
		int outLength = 0;
		return SafeStringCopy(name, outName, bufferSize, &outLength);
	}
	PIXEON_API APIResult SetObjectName(Object object, const char* name)
	{
		if (!name) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		objPtr->SetObjectName(std::string(name));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetObjectPosition(Object object, Float3 position){

		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		objPtr->SetPosition(position.x, position.y, position.z);

		for (auto rb : objPtr->GetComponents())
		{
			if(rb->GetComponentType() == ComponentManager::COMPONENT_TYPE::RIGIDBODY)
			{
				static_cast<RigidBody*>(rb)->SyncPositionToBullet(ToXMFloat3(position));
			}
		}
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetObjectPosition(Object object, Float3* outPosition)
	{
		if (!outPosition) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		Transform transform = objPtr->GetTransform();
		*outPosition = ToFloat3(transform.position);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetObjectRotation(Object object, Float3 rotation)
	{
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		objPtr->SetRotation(rotation.x, rotation.y, rotation.z);

		for(auto rb : objPtr->GetComponents())
		{
			if(rb->GetComponentType() == ComponentManager::COMPONENT_TYPE::RIGIDBODY)
			{
				static_cast<RigidBody*>(rb)->SyncRotationToBullet(ToXMFloat3(rotation));
			}
		}
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetObjectRotation(Object object, Float3* outRotation)
	{
		if (!outRotation) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		Transform transform = objPtr->GetTransform();
		*outRotation = ToFloat3(transform.rotation);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetObjectScale(Object object, Float3 scale)
	{
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		objPtr->SetScale(scale.x, scale.y, scale.z);

		std::vector<RigidBody*> components = objPtr->GetComponentsByType<RigidBody>();
		for (RigidBody* rb : components) {
			rb->SetTransformDirty(true);
		}
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetObjectScale(Object object, Float3* outScale)
	{
		if (!outScale) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		Transform transform = objPtr->GetTransform();
		*outScale = ToFloat3(transform.scale);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetObjectTransform(Object object, transform* outTransform)
	{
		if (!outTransform) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		Transform transform = objPtr->GetTransform();
		outTransform->position = ToFloat3(transform.position);
		outTransform->rotation = ToFloat3(transform.rotation);
		outTransform->scale = ToFloat3(transform.scale);

		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetObjectTransform(Object object, const transform* inTransform)
	{
		if (!inTransform) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		Transform transform;
		transform.position = ToXMFloat3(inTransform->position);
		transform.rotation = ToXMFloat3(inTransform->rotation);
		transform.scale = ToXMFloat3(inTransform->scale);

		objPtr->SetTransform(transform);

		for(auto rb : objPtr->GetComponents())
		{
			if(rb->GetComponentType() == ComponentManager::COMPONENT_TYPE::RIGIDBODY)
			{
				static_cast<RigidBody*>(rb)->SyncTransformToBullet();
			}
		}
		return PN_SUCCESS;
	}
	PIXEON_API APIResult FindComponent(Object object, const char* componentName, Component* outComponent)
	{
		if (!componentName || !outComponent) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AbstractComponent* comp = objPtr->GetComponent(std::string(componentName));
		if (!comp) {
			*outComponent = nullptr;
			return PN_ERROR_NOT_FOUND;
		}
		*outComponent = reinterpret_cast<Component>(comp);
		return PN_SUCCESS;
	}
};

// Input Functions
extern "C" {
	PIXEON_API bool KeyPressed(char keyCode)
	{
		return IsKeyPress(keyCode);
	}
	PIXEON_API bool KeyTriggered(char keyCode)
	{
		return IsKeyTrigger(keyCode);
	}
	PIXEON_API bool KeyReleased(char keyCode)
	{
		return IsKeyRelease(keyCode);
	}
	PIXEON_API bool KeyRepeated(char keyCode)
	{
		return IsKeyRepeat(keyCode);
	}
	PIXEON_API int GetMouseMoveX()
	{
		int Move;
		Move = MouseMoveX();
		return Move;
	}
	PIXEON_API int GetMouseMoveY()
	{
		int Move;
		Move = MouseMoveY();
		return Move;
	}
	PIXEON_API APIResult FixedMouseCursor(bool enbled)
	{
		MainFrame::GetInstance()->fixedMouseCursor(enbled);
		return PN_SUCCESS;
	}
};

// Component Functions
extern "C" {
	// Camera Component
	PIXEON_API APIResult GetCameraTransform(Component camera, CameraTransform* outTransform)
	{
		if (!outTransform) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(camera, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		CameraComponent* cameraComp = dynamic_cast<CameraComponent*>(compPtr);
		if (!cameraComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		outTransform->position = ToFloat3(cameraComp->GetPosition());
		outTransform->rotation = ToFloat3(cameraComp->GetRotation());
		outTransform->fixation = ToFloat3(cameraComp->GetFixation());
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetCameraTransform(Component camera, const CameraTransform* inTransform)
	{
		if (!inTransform) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(camera, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		CameraComponent* cameraComp = dynamic_cast<CameraComponent*>(compPtr);
		if (!cameraComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		cameraComp->SetPosition(ToXMFloat3(inTransform->position));
		cameraComp->SetRotation(ToXMFloat3(inTransform->rotation));
		cameraComp->SetFixation(ToXMFloat3(inTransform->fixation));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetCameraFov(Component camera, float* outFov)
	{
		if (!outFov) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(camera, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		CameraComponent* cameraComp = dynamic_cast<CameraComponent*>(compPtr);
		if (!cameraComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outFov = cameraComp->GetFov();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetCameraFov(Component camera, float inFov)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(camera, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		CameraComponent* cameraComp = dynamic_cast<CameraComponent*>(compPtr);
		if (!cameraComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		cameraComp->SetFov(inFov);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetCameraAspect(Component camera, float* outAspect)
	{
		if (!outAspect) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(camera, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		CameraComponent* cameraComp = dynamic_cast<CameraComponent*>(compPtr);
		if (!cameraComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outAspect = cameraComp->GetAspect();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetCameraAspect(Component camera, float inAspect)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(camera, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		CameraComponent* cameraComp = dynamic_cast<CameraComponent*>(compPtr);
		if (!cameraComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		cameraComp->SetAspect(inAspect);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetCameraNearFar(Component camera, float* outNear, float* outFar)
	{
		if (!outNear || !outFar) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(camera, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		CameraComponent* cameraComp = dynamic_cast<CameraComponent*>(compPtr);
		if (!cameraComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outNear = cameraComp->GetNear();
		*outFar = cameraComp->GetFar();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetCameraNearFar(Component camera, float inNear, float inFar)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(camera, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		CameraComponent* cameraComp = dynamic_cast<CameraComponent*>(compPtr);
		if (!cameraComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		cameraComp->SetNear(inNear);
		cameraComp->SetFar(inFar);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetChangeCameraCalculation(Component camera, bool isChange)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(camera, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		CameraComponent* cameraComp = dynamic_cast<CameraComponent*>(compPtr);
		if (!cameraComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		cameraComp->SetIsChangeCalculation(isChange);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetCameraNumber(Component camera, int* outCameraNumber)
	{
		if (!outCameraNumber) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(camera, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		CameraComponent* cameraComp = dynamic_cast<CameraComponent*>(compPtr);
		if (!cameraComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outCameraNumber = cameraComp->GetCameraNumber();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetCameraUpVector(Component camera, Float3* outUp)
	{
		if (!outUp) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(camera, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		CameraComponent* cameraComp = dynamic_cast<CameraComponent*>(compPtr);
		if (!cameraComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outUp = ToFloat3(cameraComp->GetUpVector());
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetCameraRightVector(Component camera, Float3* outRight)
	{
		if (!outRight) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(camera, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		CameraComponent* cameraComp = dynamic_cast<CameraComponent*>(compPtr);
		if (!cameraComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outRight = ToFloat3(cameraComp->GetRightVector());
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetCameraForwardVector(Component camera, Float3* outForward)
	{
		if (!outForward) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(camera, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		CameraComponent* cameraComp = dynamic_cast<CameraComponent*>(compPtr);
		if (!cameraComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outForward = ToFloat3(cameraComp->GetForwardVector());
		return PN_SUCCESS;
	}

	// Light Component
	PIXEON_API APIResult GetLightType(Component light, int* outType)
	{
		if (!outType) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(light, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		LightComponent* lightComp = dynamic_cast<LightComponent*>(compPtr);
		if (!lightComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outType = static_cast<int>(lightComp->GetType());
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetLightType(Component light, int inType)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(light, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		LightComponent* lightComp = dynamic_cast<LightComponent*>(compPtr);
		if (!lightComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		lightComp->SetType(static_cast<LightComponent::LightType>(inType));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetLightColor(Component light, Float3* outColor)
	{
		if (!outColor) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(light, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		LightComponent* lightComp = dynamic_cast<LightComponent*>(compPtr);
		if (!lightComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outColor = ToFloat3(lightComp->GetColor());
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetLightColor(Component light, const Float3* inColor)
	{
		if (!inColor) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(light, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		LightComponent* lightComp = dynamic_cast<LightComponent*>(compPtr);
		if (!lightComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		lightComp->SetColor(ToXMFloat3(*inColor));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetLightIntensity(Component light, float* outIntensity)
	{
		if (!outIntensity) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(light, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		LightComponent* lightComp = dynamic_cast<LightComponent*>(compPtr);
		if (!lightComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outIntensity = lightComp->GetIntensity();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetLightIntensity(Component light, float inIntensity)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(light, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		LightComponent* lightComp = dynamic_cast<LightComponent*>(compPtr);
		if (!lightComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		lightComp->SetIntensity(inIntensity);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetLightRange(Component light, float* outRange)
	{
		if (!outRange) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(light, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		LightComponent* lightComp = dynamic_cast<LightComponent*>(compPtr);
		if (!lightComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outRange = lightComp->GetRange();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetLightRange(Component light, float inRange)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(light, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		LightComponent* lightComp = dynamic_cast<LightComponent*>(compPtr);
		if (!lightComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		lightComp->SetRange(inRange);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetLightSpotInnerOuter(Component light, float* outInnerDeg, float* outOuterDeg)
	{
		if (!outInnerDeg || !outOuterDeg) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(light, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		LightComponent* lightComp = dynamic_cast<LightComponent*>(compPtr);
		if (!lightComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outInnerDeg = lightComp->GetSpotInner();
		*outOuterDeg = lightComp->GetSpotOuter();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetLightSpotInnerOuter(Component light, float inInnerDeg, float inOuterDeg)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(light, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		LightComponent* lightComp = dynamic_cast<LightComponent*>(compPtr);
		if (!lightComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		lightComp->SetSpotInner(inInnerDeg);
		lightComp->SetSpotOuter(inOuterDeg);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetLightEnabled(Component light, bool* outEnabled)
	{
		if (!outEnabled) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(light, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		LightComponent* lightComp = dynamic_cast<LightComponent*>(compPtr);
		if (!lightComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outEnabled = lightComp->IsEnabled();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetLightEnabled(Component light, bool inEnabled)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(light, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		LightComponent* lightComp = dynamic_cast<LightComponent*>(compPtr);
		if (!lightComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		lightComp->SetEnabled(inEnabled);
		return PN_SUCCESS;
	}

	// ImageRender Component
	PIXEON_API APIResult GetImageRenderTextureName(Component imageRender, char* outName, int bufferSize)
	{
		if (!outName || bufferSize <= 0) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(imageRender, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		ImageRender* imgRenderComp = dynamic_cast<ImageRender*>(compPtr);
		if (!imgRenderComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		std::string textureName = imgRenderComp->GetTextureName();
		int outLength = 0;
		return SafeStringCopy(textureName, outName, bufferSize, &outLength);
	}
	PIXEON_API APIResult SetImageRenderTextureName(Component imageRender, const char* name)
	{
		if (!name) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(imageRender, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		ImageRender* imgRenderComp = dynamic_cast<ImageRender*>(compPtr);
		if (!imgRenderComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		imgRenderComp->SetTextureName(std::string(name));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetImageTransform(Component imageRender, transform* outTransform)
	{
		if (!outTransform) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(imageRender, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		ImageRender* imgRenderComp = dynamic_cast<ImageRender*>(compPtr);
		if (!imgRenderComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		switch (imgRenderComp->GetMode())
		{
		case ImageRender::PlacementMode::Screen2D:
			outTransform->position = ToFloat3({ imgRenderComp->GetOffset2D().x,imgRenderComp->GetOffset2D().y,0.0f });
			outTransform->rotation = CreateFloat3(0.0f, 0.0f, 0.0f);
			outTransform->scale = ToFloat3({ imgRenderComp->GetSize2D().x,imgRenderComp->GetSize2D().y,1.0f });
			break;
		case ImageRender::PlacementMode::Billboard:
			outTransform->position = ToFloat3(imgRenderComp->GetOffset3D());
			outTransform->rotation = CreateFloat3(0.0f, 0.0f, 0.0f);
			outTransform->scale = ToFloat3({ imgRenderComp->GetSizeWorld().x,imgRenderComp->GetSizeWorld().y,1.0f });
			break;
		case ImageRender::PlacementMode::World3D:
			outTransform->position = ToFloat3(imgRenderComp->GetOffset3D());
			outTransform->rotation = CreateFloat3(0.0f, 0.0f, 0.0f);
			outTransform->scale = ToFloat3({ imgRenderComp->GetSizeWorld().x,imgRenderComp->GetSizeWorld().y,1.0f });
			break;
		case ImageRender::PlacementMode::UI:
			outTransform->position = ToFloat3({ imgRenderComp->GetOffset2D().x,imgRenderComp->GetOffset2D().y,0.0f });
			outTransform->rotation = CreateFloat3(0.0f, 0.0f, 0.0f);
			outTransform->scale = ToFloat3({ imgRenderComp->GetSizeWorld().x,imgRenderComp->GetSizeWorld().y,1.0f });
			break;
		default:
			break;
		}
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetImageTransform(Component imageRender, const transform* inTransform)
	{
		if (!inTransform) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(imageRender, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		ImageRender* imgRenderComp = dynamic_cast<ImageRender*>(compPtr);
		if (!imgRenderComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		switch (imgRenderComp->GetMode())
		{
		case ImageRender::PlacementMode::Screen2D:
			imgRenderComp->SetOffset2D({ inTransform->position.x, inTransform->position.y });
			imgRenderComp->SetSize2D({ inTransform->scale.x, inTransform->scale.y });
			break;
		case ImageRender::PlacementMode::Billboard:
			imgRenderComp->SetOffset3D({ inTransform->position.x, inTransform->position.y, inTransform->position.z });
			imgRenderComp->SetSizeWorld({ inTransform->scale.x, inTransform->scale.y });
			break;
		case ImageRender::PlacementMode::World3D:
			imgRenderComp->SetOffset3D({ inTransform->position.x, inTransform->position.y, inTransform->position.z });
			imgRenderComp->SetSizeWorld({ inTransform->scale.x, inTransform->scale.y });
			break;
		case ImageRender::PlacementMode::UI:
			imgRenderComp->SetOffset2D({ inTransform->position.x, inTransform->position.y });
			imgRenderComp->SetSizeWorld({ inTransform->scale.x, inTransform->scale.y });
			break;
		default:
			break;
		}
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetImageRenderColor(Component imageRender, Float4* outColor)
	{
		if (!outColor) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(imageRender, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		ImageRender* imgRenderComp = dynamic_cast<ImageRender*>(compPtr);
		if (!imgRenderComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outColor = ToFloat4(imgRenderComp->GetColor());
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetImageRenderColor(Component imageRender, const Float4* inColor)
	{
		if (!inColor) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(imageRender, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		ImageRender* imgRenderComp = dynamic_cast<ImageRender*>(compPtr);
		if (!imgRenderComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		imgRenderComp->SetColor(ToXMFloat4(*inColor));
	}
	PIXEON_API APIResult GetImageRenderUVRect(Component imageRender, Float4* outUVRect)
	{
		if (!outUVRect) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(imageRender, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		ImageRender* imgRenderComp = dynamic_cast<ImageRender*>(compPtr);
		if (!imgRenderComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outUVRect = ToFloat4(imgRenderComp->GetUVRect());
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetImageRenderUVRect(Component imageRender, const Float4* inUVRect)
	{
		if (!inUVRect) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(imageRender, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		ImageRender* imgRenderComp = dynamic_cast<ImageRender*>(compPtr);
		if (!imgRenderComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		imgRenderComp->SetUVRect(ToXMFloat4(*inUVRect));
		return PN_SUCCESS;
	}

	// Model Component
	PIXEON_API APIResult GetModelColor(Component modelRender, Float4* outColor)
	{
		if (!outColor) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(modelRender, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		ModelRenderComponent* modelRenderComp = dynamic_cast<ModelRenderComponent*>(compPtr);
		if (!modelRenderComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outColor = ToFloat4(modelRenderComp->GetColor());
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetModelColor(Component modelRender, const Float4* inColor)
	{
		if (!inColor) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(modelRender, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		ModelRenderComponent* modelRenderComp = dynamic_cast<ModelRenderComponent*>(compPtr);
		if (!modelRenderComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		modelRenderComp->SetColor(ToXMFloat4(*inColor));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetMaterialTexture(Component modelRender, int materialIndex, const char* texLogicalPath)
	{
		if (!texLogicalPath) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(modelRender, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		ModelRenderComponent* modelRenderComp = dynamic_cast<ModelRenderComponent*>(compPtr);
		if (!modelRenderComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		modelRenderComp->SetMaterialTexture(materialIndex, std::string(texLogicalPath));
		return PN_SUCCESS;
	}

	// Animation Component
	PIXEON_API APIResult PlayAnimation(Component animationComp)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(animationComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AnimationComponent* animComp = dynamic_cast<AnimationComponent*>(compPtr);
		if (!animComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		animComp->Play();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult PauseAnimation(Component animationComp)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(animationComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AnimationComponent* animComp = dynamic_cast<AnimationComponent*>(compPtr);
		if (!animComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		animComp->Pause();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult ResumeAnimation(Component animationComp)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(animationComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AnimationComponent* animComp = dynamic_cast<AnimationComponent*>(compPtr);
		if (!animComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		animComp->Resume();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult StopAnimation(Component animationComp)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(animationComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AnimationComponent* animComp = dynamic_cast<AnimationComponent*>(compPtr);
		if (!animComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		animComp->Stop();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RestartAnimation(Component animationComp)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(animationComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AnimationComponent* animComp = dynamic_cast<AnimationComponent*>(compPtr);
		if (!animComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		animComp->Restart();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetAnimationClip(Component animationComp, int clipIndex)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(animationComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AnimationComponent* animComp = dynamic_cast<AnimationComponent*>(compPtr);
		if (!animComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		animComp->SetAnimationClip(clipIndex);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetAnimationPlaybackSpeed(Component animationComp, float speed)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(animationComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AnimationComponent* animComp = dynamic_cast<AnimationComponent*>(compPtr);
		if (!animComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		animComp->SetPlaybackSpeed(speed);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetAnimationLoop(Component animationComp, bool loop)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(animationComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AnimationComponent* animComp = dynamic_cast<AnimationComponent*>(compPtr);
		if (!animComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		animComp->SetLoop(loop);
		return PN_SUCCESS;
	}

	// RigidBody Component
	PIXEON_API APIResult RigidBodyAddForce(Component rigidBodyComp, const Float3* inForce)
	{
		if (!inForce) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		rbComp->AddForce(ToXMFloat3(*inForce));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodyAddImpulse(Component rigidBodyComp, const Float3* inImpulse)
	{
		if (!inImpulse) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		rbComp->AddImpulse(ToXMFloat3(*inImpulse));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodyGetVelocity(Component rigidBodyComp, Float3* outVelocity)
	{
		if (!outVelocity) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outVelocity = ToFloat3(rbComp->GetVelocity());
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodySetVelocity(Component rigidBodyComp, const Float3* inVelocity)
	{
		if (!inVelocity) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		rbComp->SetVelocity(ToXMFloat3(*inVelocity));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodySetKinematic(Component rigidBodyComp, bool isKinematic)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		rbComp->SetKinematic(isKinematic);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodyGetKinematic(Component rigidBodyComp, bool* outIsKinematic)
	{
		if (!outIsKinematic) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outIsKinematic = rbComp->IsKinematic();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodySetMass(Component rigidBodyComp, float mass)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		rbComp->SetMass(mass);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodyGetMass(Component rigidBodyComp, float* outMass)
	{
		if (!outMass) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outMass = rbComp->GetMass();
	}
	PIXEON_API APIResult RigidBodySetUseGravity(Component rigidBodyComp, bool useGravity)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		rbComp->SetGravityEnabled(useGravity);
	}
	PIXEON_API APIResult RigidBodyGetUseGravity(Component rigidBodyComp, bool* outUseGravity)
	{
		if (!outUseGravity) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outUseGravity = rbComp->IsGravityEnabled();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodySetFriction(Component rigidBodyComp, float friction)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		rbComp->SetFriction(friction);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodyGetFriction(Component rigidBodyComp, float* outFriction)
	{
		if (!outFriction) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outFriction = rbComp->GetFriction();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodySetRestitution(Component rigidBodyComp, float restitution)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		rbComp->SetRestitution(restitution);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodyGetRestitution(Component rigidBodyComp, float* outRestitution)
	{
		if (!outRestitution) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outRestitution = rbComp->GetRestitution();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodySetLinearDamping(Component rigidBodyComp, float damping)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		rbComp->SetLinearDamping(damping);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodyGetLinearDamping(Component rigidBodyComp, float* outDamping)
	{
		if (!outDamping) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outDamping = rbComp->GetLinearDamping();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodySetAngularDamping(Component rigidBodyComp, float damping)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		rbComp->SetAngularDamping(damping);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodyGetAngularDamping(Component rigidBodyComp, float* outDamping)
	{
		if (!outDamping) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outDamping = rbComp->GetAngularDamping();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodySetDamping(Component rigidBodyComp, float linearDamping, float angularDamping)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		rbComp->SetDamping(linearDamping, angularDamping);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodySetRollingFriction(Component rigidBodyComp, float rollingFriction)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		rbComp->SetRollingFriction(rollingFriction);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodyGetRollingFriction(Component rigidBodyComp, float* outRollingFriction)
	{
		if (!outRollingFriction) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outRollingFriction = rbComp->GetRollingFriction();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodySetSpinningFriction(Component rigidBodyComp, float spinningFriction)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		rbComp->SetSpinningFriction(spinningFriction);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RigidBodyGetSpinningFriction(Component rigidBodyComp, float* outSpinningFriction)
	{
		if (!outSpinningFriction) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(rigidBodyComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		RigidBody* rbComp = dynamic_cast<RigidBody*>(compPtr);
		if (!rbComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outSpinningFriction = rbComp->GetSpinningFriction();
		return PN_SUCCESS;
	}
	// BoxCollision Component
	PIXEON_API APIResult BoxCollisionSetSize(Component component, Float3 size)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(component, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		BoxCollision* boxComp = dynamic_cast<BoxCollision*>(compPtr);
		if (!boxComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		boxComp->SetSize(ToXMFloat3(size));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult BoxCollisionGetSize(Component component, Float3* outSize)
	{
		if (!outSize) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(component, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		BoxCollision* boxComp = dynamic_cast<BoxCollision*>(compPtr);
		if (!boxComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outSize = ToFloat3(boxComp->GetSize());
		return PN_SUCCESS;
	}
	PIXEON_API APIResult BoxCollisionSetCenter(Component component, Float3 center)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(component, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		BoxCollision* boxComp = dynamic_cast<BoxCollision*>(compPtr);
		if (!boxComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		boxComp->SetCenter(ToXMFloat3(center));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult BoxCollisionGetCenter(Component component, Float3* outCenter)
	{
		if (!outCenter) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(component, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		BoxCollision* boxComp = dynamic_cast<BoxCollision*>(compPtr);
		if (!boxComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outCenter = ToFloat3(boxComp->GetCenter());
		return PN_SUCCESS;
	}
	PIXEON_API APIResult BoxCollisionSetIsTrigger(Component component, bool isTrigger)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(component, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		BoxCollision* boxComp = dynamic_cast<BoxCollision*>(compPtr);
		if (!boxComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		boxComp->SetTrigger(isTrigger);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult BoxCollisionGetIsTrigger(Component component, bool* outIsTrigger)
	{
		if (!outIsTrigger) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(component, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		BoxCollision* boxComp = dynamic_cast<BoxCollision*>(compPtr);
		if (!boxComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outIsTrigger = boxComp->IsTrigger();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult BoxCollisionSetCollisionEnterCallback(Component component, BoxCollisionEnterCallback callback)
	{
		BoxCollision* box;
		if (!ValidateHandle(component, &box))return PN_ERROR_INVALID_HANDLE;
		box->SetOnCollisionEnter([component, callback](const CollisionInfo& info)
			{
				if (callback) {
					APICollisionInfo apiInfo = ToAPICollisionInfo(info);
					callback(component, &apiInfo);
				}
			}
		);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult BoxCollisionSetCollisionStayCallback(Component component, BoxCollisionStayCallback callback)
	{
		BoxCollision* box;
		if (!ValidateHandle(component, &box))return PN_ERROR_INVALID_HANDLE;
		box->SetOnCollisionStay([component, callback](const CollisionInfo& info)
			{
				if (callback) {
					APICollisionInfo apiInfo = ToAPICollisionInfo(info);
					callback(component, &apiInfo);
				}
			}
		);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult BoxCollisionSetCollisionExitCallback(Component component, BoxCollisionExitCallback callback)
	{
		BoxCollision* box;
		if (!ValidateHandle(component, &box))return PN_ERROR_INVALID_HANDLE;
		box->SetOnCollisionExit([component, callback](const CollisionInfo& info)
			{
				if (callback) {
					APICollisionInfo apiInfo = ToAPICollisionInfo(info);
					callback(component, &apiInfo);
				}
			}
		);
		return PN_SUCCESS;
	}

	// Animator2D Component
	PIXEON_API APIResult GetAnimator2D(Component animatorComp, const char* animatorName, Animator2d* outHandel)
	{
		if (!animatorName || !outHandel) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(animatorComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		Animator2DComponent* animator2DComp = dynamic_cast<Animator2DComponent*>(compPtr);
		if (!animator2DComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		Animator2D* animator = animator2DComp->GetAnimator2D(std::string(animatorName));
		if (!animator) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		*outHandel = reinterpret_cast<Animator2d>(animator);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult Animator2DPlay(Animator2d animator)
	{
		Animator2D* animatorPtr = reinterpret_cast<Animator2D*>(animator);
		if (!animatorPtr) {
			return PN_ERROR_INVALID_HANDLE;
		}
		animatorPtr->Start();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult Animator2DStop(Animator2d animator)
	{
		Animator2D* animatorPtr = reinterpret_cast<Animator2D*>(animator);
		if (!animatorPtr) {
			return PN_ERROR_INVALID_HANDLE;
		}
		animatorPtr->Stop();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult Animator2DIsEnd(Animator2d animator, bool* End)
	{
		if (!End) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		Animator2D* animatorPtr = reinterpret_cast<Animator2D*>(animator);
		if (!animatorPtr) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*End = animatorPtr->bEnded_;
		return PN_SUCCESS;
	}
};