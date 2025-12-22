#include "API.h"
#include "SceneManger.h"
#include "ComponentManager.h"
#include "Component.h"
#include "Scene.h"
#include "Object.h"

#include "CameraComponent.h"
#include "BoxCollision.h"
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

    // CollisionInfo‚©‚çAPICollisionInfo‚Ö•ÏŠ·
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
		objPtr->SetTransform(transform);
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

// Component Functions
extern "C"{
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
    PIXEON_API APIResult GetCmaeraUpVector(Component camera, Float3* outUp)
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
}

