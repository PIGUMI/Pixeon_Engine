#include "API.h"
#include "Object.h"
#include "Component.h"
#include "CameraComponent.h"
#include "ImageRender.h"
#include "Scene.h"
#include "SceneManger.h"

/* 基本API */
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

/* カメラコンポーネントに関するAPI */
extern "C"
{
		/* カメラのトランスフォーム取得 */
	PIXEON_API APIResult CameraComponent_GetTransform(ComponentHandle cameraComponent, CameraTransform* outTransform)
	{
		if (cameraComponent == nullptr || outTransform == nullptr)return PN_ERROR_INVALID_PARAMETER;
		CameraComponent* targetComp = reinterpret_cast<CameraComponent*>(cameraComponent);
		outTransform->position = { targetComp->GetPosition().x, targetComp->GetPosition().y, targetComp->GetPosition().z };
		outTransform->rotation = { targetComp->GetRotation().x, targetComp->GetRotation().y,  targetComp->GetRotation().z };
		outTransform->fixation = { targetComp->GetFixation().x, targetComp->GetFixation().y, targetComp->GetFixation().z };
		return PN_SUCCESS;
	}
		/* カメラのトランスフォーム設定 */
	PIXEON_API APIResult CameraComponent_SetTransform(ComponentHandle cameraComponent, const CameraTransform* inTransform)
	{
		if (cameraComponent == nullptr || inTransform == nullptr)return PN_ERROR_INVALID_PARAMETER;
		CameraComponent* targetComp = reinterpret_cast<CameraComponent*>(cameraComponent);
		targetComp->SetPosition({ inTransform->position.x, inTransform->position.y, inTransform->position.z });
		targetComp->SetRotation({ inTransform->rotation.x, inTransform->rotation.y, inTransform->rotation.z });
		targetComp->SetFixation({ inTransform->fixation.x, inTransform->fixation.y, inTransform->fixation.z });
		return PN_SUCCESS;
	}
		/* カメラのFOV設定 */
	PIXEON_API APIResult CameraComponent_SetFov(ComponentHandle cameraComponent, float outFov)
	{
		if (cameraComponent == nullptr)return PN_ERROR_INVALID_PARAMETER;
		CameraComponent* targetComp = reinterpret_cast<CameraComponent*>(cameraComponent);
		targetComp->SetFov(outFov);
		return PN_SUCCESS;
	}
		/* カメラのアスペクト比設定 */
	PIXEON_API APIResult CameraComponent_SetAspect(ComponentHandle cameraComponent, float InAspect)
	{
		if (cameraComponent == nullptr)return PN_ERROR_INVALID_PARAMETER;
		CameraComponent* targetComp = reinterpret_cast<CameraComponent*>(cameraComponent);
		targetComp->SetAspect(InAspect);
		return PN_SUCCESS;
	}
		/* カメラのニアクリップ設定 */
	PIXEON_API APIResult CameraComponent_SetNear(ComponentHandle cameraComponent, float InNear)
	{
		if (cameraComponent == nullptr)return PN_ERROR_INVALID_PARAMETER;
		CameraComponent* targetComp = reinterpret_cast<CameraComponent*>(cameraComponent);
		targetComp->SetNear(InNear);
		return PN_SUCCESS;
	}
		/* カメラのファークリップ設定 */
	PIXEON_API APIResult CameraComponent_SetFar(ComponentHandle cameraComponent, float InFar)
	{
		if (cameraComponent == nullptr)return PN_ERROR_INVALID_PARAMETER;
		CameraComponent* targetComp = reinterpret_cast<CameraComponent*>(cameraComponent);
		targetComp->SetFar(InFar);
		return PN_SUCCESS;
	}
		/* カメラの計算方法変更設定 */
	PIXEON_API APIResult CameraComponent_ChangeCalculationMode(ComponentHandle cameraComponent, bool isChange)
	{
		if (cameraComponent == nullptr)return PN_ERROR_INVALID_PARAMETER;
		CameraComponent* targetComp = reinterpret_cast<CameraComponent*>(cameraComponent);
		targetComp->SetIsChangeCalculation(isChange);
		return PN_SUCCESS;
	}
}

/* イメージレンダーコンポーネントに関するAPI */
extern "C" {
			/* テクスチャ名の設定 */
	PIXEON_API APIResult ImageRender_SetTextureName(ComponentHandle imageRender, const char* textureName)
	{
		if (imageRender == nullptr || textureName == nullptr)return PN_ERROR_INVALID_PARAMETER;
		ImageRender* targetComp = reinterpret_cast<ImageRender*>(imageRender);
		targetComp->SetTextureName(textureName);
		return PN_SUCCESS;
	}
			/* テクスチャ名の取得 */
	PIXEON_API APIResult ImageRender_GetTextureName(ComponentHandle imageRender, char* outTextureName, int bufferSize)
	{
		if (imageRender == nullptr || outTextureName == nullptr || bufferSize <= 0)return PN_ERROR_INVALID_PARAMETER;
		ImageRender* targetComp = reinterpret_cast<ImageRender*>(imageRender);
		std::string texName = targetComp->GetTextureName();
		if (texName.size() + 1 > static_cast<size_t>(bufferSize))return PN_ERROR_BUFFER_TOO_SMALL;
		strcpy_s(outTextureName, bufferSize, texName.c_str());
		return PN_SUCCESS;
	}
			/* 描画モードの変更 */
			/* 0:2D描画 1:ビルボード 2:3D描画 */
	PIXEON_API APIResult ImageRender_SetPlacementMode(ComponentHandle imageRender, int mode)
	{
		if (imageRender == nullptr)return PN_ERROR_INVALID_PARAMETER;
		ImageRender* targetComp = reinterpret_cast<ImageRender*>(imageRender);
		if (mode < 0 || mode > 2)return PN_ERROR_INVALID_PARAMETER;
		targetComp->SetPlacementMode(static_cast<ImageRender::PlacementMode>(mode));
		return PN_SUCCESS;
	}
			/* 描画モードの取得 */
			/* 0:2D描画 1:ビルボード 2:3D描画 */
	PIXEON_API APIResult ImageRender_GetPlacementMode(ComponentHandle imageRender, int* outMode)
	{
		if (imageRender == nullptr || outMode == nullptr)return PN_ERROR_INVALID_PARAMETER;
		ImageRender* targetComp = reinterpret_cast<ImageRender*>(imageRender);
		*outMode = static_cast<int>(targetComp->GetPlacementMode());
		return PN_SUCCESS;
	}
			/* オフセット設定 */
			/* ※2D描画時 */
	PIXEON_API APIResult ImageRender_SetOffset2D(ComponentHandle imageRender, Float2 offset)
	{
		if (imageRender == nullptr)return PN_ERROR_INVALID_PARAMETER;
		ImageRender* targetComp = reinterpret_cast<ImageRender*>(imageRender);
		DirectX::XMFLOAT2 oFfset = { offset.x, offset.y };
		targetComp->SetOffset2D(oFfset);
		return PN_SUCCESS;
	}
			/* オフセット取得 */
			/* ※2D描画時 */
	PIXEON_API APIResult ImageRender_GetOffset2D(ComponentHandle imageRender, Float2* offset)
	{
		if (imageRender == nullptr || offset == nullptr)return PN_ERROR_INVALID_PARAMETER;
		ImageRender* targetComp = reinterpret_cast<ImageRender*>(imageRender);
		DirectX::XMFLOAT2 oFfset = targetComp->GetOffset2D();
		offset->x = oFfset.x;
		offset->y = oFfset.y;
		return PN_SUCCESS;
	}
			/* サイズ取得 */
			/* ※2D描画時 */
	PIXEON_API APIResult ImageRender_SetSize2D(ComponentHandle imageRender, Float2 size)
	{
		if (imageRender == nullptr)return PN_ERROR_INVALID_PARAMETER;
		ImageRender* targetComp = reinterpret_cast<ImageRender*>(imageRender);
		DirectX::XMFLOAT2 sz = { size.x, size.y };
		targetComp->SetSize2D(sz);
		return PN_SUCCESS;
	}
			/* サイズ設定 */
			/* ※2D描画時 */
	PIXEON_API APIResult ImageRender_GetSize2D(ComponentHandle imageRender, Float2* size)
	{
		if (imageRender == nullptr || size == nullptr)return PN_ERROR_INVALID_PARAMETER;
		ImageRender* targetComp = reinterpret_cast<ImageRender*>(imageRender);
		DirectX::XMFLOAT2 sz = targetComp->GetSize2D();
		size->x = sz.x;
		size->y = sz.y;
		return PN_SUCCESS;
	}
			/* オフセット設定 */
			/* ※ビルボード・3D描画時 */
	PIXEON_API APIResult ImageRender_SetOffset3D(ComponentHandle imageRender, Float3 offset)
	{
		if (imageRender == nullptr)return PN_ERROR_INVALID_PARAMETER;
		ImageRender* targetComp = reinterpret_cast<ImageRender*>(imageRender);
		DirectX::XMFLOAT3 oFfset = { offset.x, offset.y, offset.z };
		targetComp->SetOffset3D(oFfset);
		return PN_SUCCESS;
	}
			/* オフセット取得 */
			/* ※ビルボード・3D描画時 */
	PIXEON_API APIResult ImageRender_GetOffset3D(ComponentHandle imageRender, Float3* offset)
	{
		if (imageRender == nullptr || offset == nullptr)return PN_ERROR_INVALID_PARAMETER;
		ImageRender* targetComp = reinterpret_cast<ImageRender*>(imageRender);
		DirectX::XMFLOAT3 oFfset = targetComp->GetOffset3D();
		offset->x = oFfset.x;
		offset->y = oFfset.y;
		offset->z = oFfset.z;
		return PN_SUCCESS;
	}
			/* サイズの設定 */
			/* ※ビルボード・3D描画時 */
	PIXEON_API APIResult ImageRender_SetSize3D(ComponentHandle imageRender, Float2 size)
	{
		if (imageRender == nullptr)return PN_ERROR_INVALID_PARAMETER;
		ImageRender* targetComp = reinterpret_cast<ImageRender*>(imageRender);
		DirectX::XMFLOAT2 sz = { size.x, size.y };
		targetComp->SetSizeWorld(sz);
		return PN_SUCCESS;
	}
			/* サイズの設定 */
			/* ※ビルボード・3D描画時 */
	PIXEON_API APIResult ImageRender_GetSize3D(ComponentHandle imageRender, Float2* size)
	{
		if (imageRender == nullptr || size == nullptr)return PN_ERROR_INVALID_PARAMETER;
		ImageRender* targetComp = reinterpret_cast<ImageRender*>(imageRender);
		DirectX::XMFLOAT2 sz = targetComp->GetSizeWorld();
		size->x = sz.x;
		size->y = sz.y;
		return PN_SUCCESS;
	}
			/* UV設定 */
			/* 0～1 */
			/* X:Y = 0:0 Z:W = 1:1 */
	PIXEON_API APIResult ImageRender_SetUVRect(ComponentHandle imageRender, Float4 uvRect)
	{
		if (imageRender == nullptr)return PN_ERROR_INVALID_PARAMETER;
		ImageRender* targetComp = reinterpret_cast<ImageRender*>(imageRender);
		DirectX::XMFLOAT4 uvR = { uvRect.x, uvRect.y, uvRect.z, uvRect.w };
		targetComp->SetUVRect(uvR);
		return PN_SUCCESS;
	}
			/* UV取得 */
			/* 0 ～ 1 */
	PIXEON_API APIResult ImageRender_GetUVRect(ComponentHandle imageRender, Float4* outUVRect)
	{
		if (imageRender == nullptr || outUVRect == nullptr)return PN_ERROR_INVALID_PARAMETER;
		ImageRender* targetComp = reinterpret_cast<ImageRender*>(imageRender);
		DirectX::XMFLOAT4 uvR = targetComp->GetUVRect();
		outUVRect->x = uvR.x;
		outUVRect->y = uvR.y;
		outUVRect->z = uvR.z;
		outUVRect->w = uvR.w;
		return PN_SUCCESS;
	}
}