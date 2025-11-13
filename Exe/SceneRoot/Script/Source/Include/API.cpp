#include "API.h"
#include "Object.h"
#include "Component.h"
#include "CameraComponent.h"
#include "Scene.h"
#include "SceneManger.h"

extern "C" {
		/* 現在のシーンの取得 */
	PIXEON_API APIResult GetCurrentScene(SceneHandle* outHandle)
	{
		if (outHandle == nullptr) return PN_ERROR_INVALID_PARAMETER;
		Scene* currentScene = nullptr;
		currentScene = SceneManger::GetInstance()->GetCurrentScene();
		if (currentScene == nullptr) return PN_ERROR_NULL_POINTER;
		*outHandle = reinterpret_cast<SceneHandle*>(currentScene);
	}
		/* ゲームオブジェクトの取得 */
	PIXEON_API APIResult GetGameObject(SceneHandle scene, const char* objectname, GameObjectHandle* outObject)
	{
		if(scene == nullptr || objectname == nullptr || outObject == nullptr)return PN_ERROR_INVALID_PARAMETER;

		Scene* targetScene = reinterpret_cast<Scene*>(scene);
		if (targetScene == nullptr)return PN_ERROR_INVALID_HANDLE;

		Object* obj = targetScene->FindObjectByName(objectname);
		if (obj == nullptr)return PN_ERROR_NOT_FOUND;
		*outObject = reinterpret_cast<GameObjectHandle*>(obj);
		return PN_SUCCESS;
	}
		/* トランスフォームの設定 */
	PIXEON_API APIResult SetGameObjectTransform(GameObjectHandle gameObject, const TransformData* inTransform)
	{
		if (gameObject == nullptr || inTransform == nullptr)return PN_ERROR_INVALID_PARAMETER;
		Object* targetObject = reinterpret_cast<Object*>(gameObject);
		if (targetObject == nullptr)return PN_ERROR_INVALID_HANDLE;
		Transform t;
		t.position = { inTransform->position.x, inTransform->position.y, inTransform->position.z };
		t.rotation = { inTransform->rotation.x, inTransform->rotation.y, inTransform->rotation.z };
		t.scale = { inTransform->scale.x, inTransform->scale.y, inTransform->scale.z };
		targetObject->SetTransform(t);
		return PN_SUCCESS;
	}
		/* コンポーネントの取得 */
	PIXEON_API APIResult GetComponent(GameObjectHandle gameObject, const char* componentName, ComponentHandle* outComponent)
	{
		if (gameObject == nullptr || componentName == nullptr || outComponent == nullptr)return PN_ERROR_INVALID_PARAMETER;
		Object* targetObject = reinterpret_cast<Object*>(gameObject);
		if (targetObject == nullptr)return PN_ERROR_INVALID_HANDLE;
		Component* comp = targetObject->GetComponent(componentName);
		if (comp == nullptr)return PN_ERROR_NOT_FOUND;
		*outComponent = reinterpret_cast<ComponentHandle*>(comp);
		return PN_SUCCESS;
	}
		/* トランスフォームの取得 */
	PIXEON_API APIResult GetGameObjectTransform(GameObjectHandle gameObject, TransformData* outTransform)
	{
		if (gameObject == nullptr || outTransform == nullptr)return PN_ERROR_INVALID_PARAMETER;
		Object* targetObject = reinterpret_cast<Object*>(gameObject);
		if (targetObject == nullptr)return PN_ERROR_INVALID_HANDLE;
		Transform t = targetObject->GetTransform();
		outTransform->position.x = t.position.x;
		outTransform->position.y = t.position.y;
		outTransform->position.z = t.position.z;
		outTransform->rotation.x = t.rotation.x;
		outTransform->rotation.y = t.rotation.y;
		outTransform->rotation.z = t.rotation.z;
		outTransform->scale.x = t.scale.x;
		outTransform->scale.y = t.scale.y;
		outTransform->scale.z = t.scale.z;
		return PN_SUCCESS;
	}
}

extern "C"
{
		/* カメラのトランスフォーム取得 */
	PIXEON_API APIResult CameraComponent_GetTrsform(ComponentHandle cameraComponent, CameraTransform* outTransform)
	{
		if (cameraComponent == nullptr || outTransform == nullptr)return PN_ERROR_INVALID_PARAMETER;
		CameraComponent* targetComp = reinterpret_cast<CameraComponent*>(cameraComponent);
		outTransform->position = { targetComp->GetPosition().x, targetComp->GetPosition().y, targetComp->GetPosition().z };
		outTransform->rotation = { targetComp->GetRotation().x, targetComp->GetRotation().y,  targetComp->GetRotation().z };
		outTransform->fixation = { targetComp->GetFixation().x, targetComp->GetFixation().y, targetComp->GetFixation().z };
		return PN_SUCCESS;
	}
	/* カメラのトランスフォーム設定 */
	PIXEON_API APIResult CameraComponent_SetTrsform(ComponentHandle cameraComponent, const CameraTransform* inTransform)
	{
		if (cameraComponent == nullptr || inTransform == nullptr)return PN_ERROR_INVALID_PARAMETER;
		CameraComponent* targetComp = reinterpret_cast<CameraComponent*>(cameraComponent);
		targetComp->SetPosition({ inTransform->position.x, inTransform->position.y, inTransform->position.z });
		targetComp->SetRotation({ inTransform->rotation.x, inTransform->rotation.y, inTransform->rotation.z });
		targetComp->SetFixation({ inTransform->fixation.x, inTransform->fixation.y, inTransform->fixation.z });
		return PN_SUCCESS;
	}
}