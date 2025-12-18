#include "API.h"
#include "Object.h"
#include "Scene.h"
#include "SceneManger.h"
#include "EngineFrame.h"
#include "Input.h"
#include "Component.h"
#include "CameraComponent.h"
#include "ImageRender.h"
#include "LightComponent.h"
#include "ModelRender.h"

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
		return PN_SUCCESS;
	}
	/* シーン切り替え */
	PIXEON_API APIResult ChangeScene(const char* sceneName)
	{
		if (sceneName == nullptr)return PN_ERROR_INVALID_PARAMETER;
		SceneManger::GetInstance()->ChangeScene(sceneName);
		return PN_SUCCESS;
	}
	/* ゲームオブジェクトの取得 */
	PIXEON_API APIResult GetGameObject(SceneHandle scene, const char* objectname, GameObjectHandle* outObject)
	{
		if (scene == nullptr || objectname == nullptr || outObject == nullptr)return PN_ERROR_INVALID_PARAMETER;

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
	/* ゲームオブジェクトのシーンへの追加 */
	PIXEON_API APIResult AddGameObject(SceneHandle scene, GameObjectHandle object)
	{
		if (scene == nullptr || object == nullptr)return PN_ERROR_INVALID_PARAMETER;
		Scene* targetScene = reinterpret_cast<Scene*>(scene);
		if (targetScene == nullptr)return PN_ERROR_INVALID_HANDLE;
		Object* targetObject = reinterpret_cast<Object*>(object);
		if (targetObject == nullptr)return PN_ERROR_INVALID_HANDLE;
		bool result = targetScene->AddObject(targetObject);
		if (!result)return PN_ERROR_INVALID_PARAMETER;
		return PN_SUCCESS;
	}
	/* Prefabからオブジェクトの取得 */
	PIXEON_API APIResult GetPrefabObject(const char* prefabName, GameObjectHandle* outObject)
	{
		if (prefabName == nullptr || outObject == nullptr)return PN_ERROR_INVALID_PARAMETER;
		Object* prefabObject = EngineFrame::GetInstance()->GetPrefabByName(prefabName);
		if (prefabObject == nullptr)return PN_ERROR_NOT_FOUND;
		*outObject = reinterpret_cast<GameObjectHandle*>(prefabObject);
		return PN_SUCCESS;
	}
	/* ゲームオブジェクトのシーンからの削除 */
	PIXEON_API APIResult RemoveGameObject(SceneHandle scene, GameObjectHandle object)
	{
		if (scene == nullptr || object == nullptr)return PN_ERROR_INVALID_PARAMETER;
		Scene* targetScene = reinterpret_cast<Scene*>(scene);
		if (targetScene == nullptr)return PN_ERROR_INVALID_HANDLE;
		Object* targetObject = reinterpret_cast<Object*>(object);
		if (targetObject == nullptr)return PN_ERROR_INVALID_HANDLE;
		targetScene->RemoveObject(targetObject);
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

/* 入力処理 */
extern "C" {
	/* キーが押されているか */
	PIXEON_API APIResult IsKeyPressed(char Key, bool* outPressed)
	{
		if (outPressed == nullptr)return PN_ERROR_INVALID_PARAMETER;
		*outPressed = IsKeyPress(Key);
		return PN_SUCCESS;
	}
	/* キーがトリガーされたか */
	PIXEON_API APIResult IsKeyTrigger(char Key, bool* outTriggered)
	{
		if (outTriggered == nullptr)return PN_ERROR_INVALID_PARAMETER;
		*outTriggered = IsKeyTrigger(Key);
		return PN_SUCCESS;
	}
	/* キーがリリースされたか */
	PIXEON_API APIResult IsKeyRelease(char Key, bool* outReleased)
	{
		if (outReleased == nullptr)return PN_ERROR_INVALID_PARAMETER;
		*outReleased = IsKeyRelease(Key);
		return PN_SUCCESS;
	}
	/* キーがリピートされたか */
	PIXEON_API APIResult IsKeyRepeat(char Key, bool* outRepeated)
	{
		if (outRepeated == nullptr)return PN_ERROR_INVALID_PARAMETER;
		*outRepeated = IsKeyRepeat(Key);
		return PN_SUCCESS;
	}
	/* マウスの移動量取得 */
	PIXEON_API APIResult GetMouseMove(int* outX, int* outY)
	{
		if (outX == nullptr || outY == nullptr)return PN_ERROR_INVALID_PARAMETER;
		*outX = MouseMoveX();
		*outY = MouseMoveY();
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
	/* 0:2D描画 1:ビルボード 2:3D描画 3:UI */
	PIXEON_API APIResult ImageRender_SetPlacementMode(ComponentHandle imageRender, int mode)
	{
		if (imageRender == nullptr)return PN_ERROR_INVALID_PARAMETER;
		ImageRender* targetComp = reinterpret_cast<ImageRender*>(imageRender);
		if (mode < 0 || mode > 2)return PN_ERROR_INVALID_PARAMETER;
		targetComp->SetPlacementMode(static_cast<ImageRender::PlacementMode>(mode));
		return PN_SUCCESS;
	}
	/* 描画モードの取得 */
	/* 0:2D描画 1:ビルボード 2:3D描画 3:UI */
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

/* ライトコンポーネントに関するAPI */
extern "C" {
	/* ライトコンポーネントのモード設定 */
	/* 0:Directional 1:Point 2:Spot */
	PIXEON_API APIResult LightComponent_SetType(ComponentHandle lightComponent, int type)
	{
		if (lightComponent == nullptr)return PN_ERROR_INVALID_PARAMETER;
		LightComponent* targetComp = reinterpret_cast<LightComponent*>(lightComponent);
		if (type < 0 || type > 2)return PN_ERROR_INVALID_PARAMETER;
		targetComp->SetType(static_cast<LightComponent::LightType>(type));
		return PN_SUCCESS;
	}
	/* ライトコンポーネントのモード取得 */
	/* 0:Directional 1:Point 2:Spot */
	PIXEON_API APIResult LightComponent_GetType(ComponentHandle lightComponent, int* outType)
	{
		if (lightComponent == nullptr || outType == nullptr)return PN_ERROR_INVALID_PARAMETER;
		LightComponent* targetComp = reinterpret_cast<LightComponent*>(lightComponent);
		*outType = static_cast<int>(targetComp->GetType());
		return PN_SUCCESS;
	}
	/* ライト色設定 */
	PIXEON_API APIResult LightComponent_SetColor(ComponentHandle lightComponent, Float3 color)
	{
		if (lightComponent == nullptr)return PN_ERROR_INVALID_PARAMETER;
		LightComponent* targetComp = reinterpret_cast<LightComponent*>(lightComponent);
		DirectX::XMFLOAT3 c = { color.x, color.y, color.z };
		targetComp->SetColor(c);
		return PN_SUCCESS;
	}
	/* ライト色取得 */
	PIXEON_API APIResult LightComponent_GetColor(ComponentHandle lightComponent, Float3* outColor)
	{
		if (lightComponent == nullptr || outColor == nullptr)return PN_ERROR_INVALID_PARAMETER;
		LightComponent* targetComp = reinterpret_cast<LightComponent*>(lightComponent);
		DirectX::XMFLOAT3 color = targetComp->GetColor();
		outColor->x = color.x;
		outColor->y = color.y;
		outColor->z = color.z;
		return PN_SUCCESS;
	}
	/* ライト強度取得 */
	PIXEON_API APIResult LightComponent_SetIntensity(ComponentHandle lightComponent, float intensity)
	{
		if (lightComponent == nullptr)return PN_ERROR_INVALID_PARAMETER;
		LightComponent* targetComp = reinterpret_cast<LightComponent*>(lightComponent);
		targetComp->SetIntensity(intensity);
		return PN_SUCCESS;
	}
	/* ライト強度取得 */
	PIXEON_API APIResult LightComponent_GetIntensity(ComponentHandle lightComponent, float* outIntensity)
	{
		if (lightComponent == nullptr || outIntensity == nullptr)return PN_ERROR_INVALID_PARAMETER;
		LightComponent* targetComp = reinterpret_cast<LightComponent*>(lightComponent);
		*outIntensity = targetComp->GetIntensity();
		return PN_SUCCESS;
	}
	/* ライト距離設定 */
	PIXEON_API APIResult LightComponent_SetRange(ComponentHandle lightComponent, float range)
	{
		if (lightComponent == nullptr)return PN_ERROR_INVALID_PARAMETER;
		LightComponent* targetComp = reinterpret_cast<LightComponent*>(lightComponent);
		targetComp->SetRange(range);
		return PN_SUCCESS;
	}
	/* ライト距離取得 */
	PIXEON_API APIResult LightComponent_GetRange(ComponentHandle lightComponent, float* outRange)
	{
		if (lightComponent == nullptr || outRange == nullptr)return PN_ERROR_INVALID_PARAMETER;
		LightComponent* targetComp = reinterpret_cast<LightComponent*>(lightComponent);
		*outRange = targetComp->GetRange();
		return PN_SUCCESS;
	}
	/* スポットライト外側角度設定 */
	PIXEON_API APIResult LightComponent_SetSpotInner(ComponentHandle lightComponent, float innerDeg)
	{
		if (lightComponent == nullptr)return PN_ERROR_INVALID_PARAMETER;
		LightComponent* targetComp = reinterpret_cast<LightComponent*>(lightComponent);
		targetComp->SetSpotInner(innerDeg);
		return PN_SUCCESS;
	}
	/* スポットライト外側角度取得 */
	PIXEON_API APIResult LightComponent_GetSpotInner(ComponentHandle lightComponent, float* outInnerDeg)
	{
		if (lightComponent == nullptr || outInnerDeg == nullptr)return PN_ERROR_INVALID_PARAMETER;
		LightComponent* targetComp = reinterpret_cast<LightComponent*>(lightComponent);
		*outInnerDeg = targetComp->GetSpotInner();
		return PN_SUCCESS;
	}
	/* スポットライト外側角度設定 */
	PIXEON_API APIResult LightComponent_SetSpotOuter(ComponentHandle lightComponent, float outerDeg)
	{
		if (lightComponent == nullptr)return PN_ERROR_INVALID_PARAMETER;
		LightComponent* targetComp = reinterpret_cast<LightComponent*>(lightComponent);
		targetComp->SetSpotOuter(outerDeg);
		return PN_SUCCESS;
	}
	/* スポットライト外側角度取得 */
	PIXEON_API APIResult LightComponent_GetSpotOuter(ComponentHandle lightComponent, float* outOuterDeg)
	{
		if (lightComponent == nullptr || outOuterDeg == nullptr)return PN_ERROR_INVALID_PARAMETER;
		LightComponent* targetComp = reinterpret_cast<LightComponent*>(lightComponent);
		*outOuterDeg = targetComp->GetSpotOuter();
		return PN_SUCCESS;
	}
	/* スポットライト外側角度設定 */
	PIXEON_API APIResult LightComponent_SetEnabled(ComponentHandle lightComponent, bool enabled)
	{
		if (lightComponent == nullptr)return PN_ERROR_INVALID_PARAMETER;
		LightComponent* targetComp = reinterpret_cast<LightComponent*>(lightComponent);
		targetComp->SetEnabled(enabled);
		return PN_SUCCESS;
	}
	/* スポットライト外側角度取得 */
	PIXEON_API APIResult LightComponent_IsEnabled(ComponentHandle lightComponent, bool* outEnabled)
	{
		if (lightComponent == nullptr || outEnabled == nullptr)return PN_ERROR_INVALID_PARAMETER;
		LightComponent* targetComp = reinterpret_cast<LightComponent*>(lightComponent);
		*outEnabled = targetComp->IsEnabled();
		return PN_SUCCESS;
	}
};

/* モデルレンダラーに関するAPI */
extern "C" {
	/* モデル名の設定 */
	PIXEON_API APIResult ModelComponent_SetModelName(ComponentHandle modelComponent, const char* modelName)
	{
		if (modelComponent == nullptr || modelName == nullptr)return PN_ERROR_INVALID_PARAMETER;
		ModelRenderComponent* targetComp = reinterpret_cast<ModelRenderComponent*>(modelComponent);
		targetComp->SetModel(modelName);
		return PN_SUCCESS;
	}
};