#ifndef API_H
#define API_H

/*
* Pixeon Engine API
* Now Version: 1.0.0
* Copyright (c) AC30W
*/

// Define PIXEON_API for DLL export/import
#ifdef PixeonEngine_EXPORTS
#define PIXEON_API __declspec(dllexport)
#else
#define PIXEON_API __declspec(dllimport)
#endif

/* ErrorCodes */
typedef enum {
	PN_SUCCESS = 0,
	PN_ERROR_NULL_POINTER = -1,
	PN_ERROR_INVALID_HANDLE = -2,
	PN_ERROR_NOT_FOUND = -3,
	PN_ERROR_BUFFER_TOO_SMALL = -4,
	PN_ERROR_INVALID_PARAMETER = -5,
}APIResult;

// Handle Types
typedef void* SceneHandle;
typedef void* GameObjectHandle;
typedef void* ComponentHandle;

#pragma pack(push, 1)  // 1バイトアライメントで構造体パディングを制御

typedef struct {
	float x;
	float y;
} Float2;

typedef struct {
	float x;
	float y;
	float z;
} Float3;

typedef struct {
	float x;
	float y;
	float z;
	float w;
} Float4;

typedef struct {
	Float3 position;
	Float3 rotation;  // Radians
	Float3 scale;
} TransformData;

typedef struct {
	Float3 position;
	Float3 rotation;
	Float3 fixation;
} CameraTransform;

#pragma pack(pop)

/* シーンに関するAPI */
extern "C" {
	/* 現在のシーンの取得 */
	PIXEON_API APIResult GetCurrentScene(SceneHandle* outHandle);
	PIXEON_API APIResult ChangeScene(const char* sceneName);
}

/* ゲームオブジェクトに関するAPI */
extern"C" {
	PIXEON_API APIResult GetGameObject(SceneHandle scene, const char* objectname, GameObjectHandle* outObject);
	PIXEON_API APIResult GetGameObjectTransform(GameObjectHandle gameObject, TransformData* outTransform);
	PIXEON_API APIResult SetGameObjectTransform(GameObjectHandle gameObject, const TransformData* inTransform);
	PIXEON_API APIResult GetComponent(GameObjectHandle gameObject, const char* componentName, ComponentHandle* outComponent);
	PIXEON_API APIResult AddGameObject(SceneHandle scene, GameObjectHandle object);
	PIXEON_API APIResult GetPrefabObject(const char* prefabName, GameObjectHandle* outObject);
	PIXEON_API APIResult RemoveGameObject(SceneHandle scene, GameObjectHandle object);

}

/* 入力関するAPI */
extern"C"
{
	/* キーボード */
	PIXEON_API APIResult IsKeyPressed(char Key, bool* outPressed);
	PIXEON_API APIResult IsKeyTrigger(char Key, bool* outTriggered);
	PIXEON_API APIResult IsKeyRelease(char Key, bool* outReleased);
	PIXEON_API APIResult IsKeyRepeat(char Key, bool* outRepeated);
	PIXEON_API APIResult GetMouseMove(int* outX, int* outY);

}

/* コンポーネントに関するAPI */
extern"C"
{
	/* CameraComponent */
	PIXEON_API APIResult CameraComponent_GetTransform(ComponentHandle cameraComponent, CameraTransform* outTransform);
	PIXEON_API APIResult CameraComponent_SetTransform(ComponentHandle cameraComponent, const CameraTransform* inTransform);
	PIXEON_API APIResult CameraComponent_SetFov(ComponentHandle cameraComponent, float InFov);
	PIXEON_API APIResult CameraComponent_SetAspect(ComponentHandle cameraComponent, float InAspect);
	PIXEON_API APIResult CameraComponent_SetNear(ComponentHandle cameraComponent, float InNear);
	PIXEON_API APIResult CameraComponent_SetFar(ComponentHandle cameraComponent, float InFar);
	PIXEON_API APIResult CameraComponent_ChangeCalculationMode(ComponentHandle cameraComponent, bool isChange);
	/*                 */

	/* ImageRender */
	PIXEON_API APIResult ImageRender_SetTextureName(ComponentHandle imageRender, const char* textureName);
	PIXEON_API APIResult ImageRender_GetTextureName(ComponentHandle imageRender, char* outTextureName, int bufferSize);
	PIXEON_API APIResult ImageRender_SetPlacementMode(ComponentHandle imageRender, int mode);
	PIXEON_API APIResult ImageRender_GetPlacementMode(ComponentHandle imageRender, int* outMode);
	PIXEON_API APIResult ImageRender_SetOffset2D(ComponentHandle imageRender, Float2 offset);
	PIXEON_API APIResult ImageRender_GetOffset2D(ComponentHandle imageRender, Float2* offset);
	PIXEON_API APIResult ImageRender_SetSize2D(ComponentHandle imageRender, Float2 size);
	PIXEON_API APIResult ImageRender_GetSize2D(ComponentHandle imageRender, Float2* size);
	PIXEON_API APIResult ImageRender_SetOffset3D(ComponentHandle imageRender, Float3 offset);
	PIXEON_API APIResult ImageRender_GetOffset3D(ComponentHandle imageRender, Float3* offset);
	PIXEON_API APIResult ImageRender_SetSize3D(ComponentHandle imageRender, Float2 size);
	PIXEON_API APIResult ImageRender_GetSize3D(ComponentHandle imageRender, Float2* size);
	PIXEON_API APIResult ImageRender_SetUVRect(ComponentHandle imageRender, Float4 uvRect);
	PIXEON_API APIResult ImageRender_GetUVRect(ComponentHandle imageRender, Float4* outUVRect);
	/*             */

	/* LightComponent */
	PIXEON_API APIResult LightComponent_SetType(ComponentHandle lightComponent, int type);
	PIXEON_API APIResult LightComponent_GetType(ComponentHandle lightComponent, int* outType);
	PIXEON_API APIResult LightComponent_SetColor(ComponentHandle lightComponent, Float3 color);
	PIXEON_API APIResult LightComponent_GetColor(ComponentHandle lightComponent, Float3* outColor);
	PIXEON_API APIResult LightComponent_SetIntensity(ComponentHandle lightComponent, float intensity);
	PIXEON_API APIResult LightComponent_GetIntensity(ComponentHandle lightComponent, float* outIntensity);
	PIXEON_API APIResult LightComponent_SetRange(ComponentHandle lightComponent, float range);
	PIXEON_API APIResult LightComponent_GetRange(ComponentHandle lightComponent, float* outRange);
	PIXEON_API APIResult LightComponent_SetSpotInner(ComponentHandle lightComponent, float innerDeg);
	PIXEON_API APIResult LightComponent_GetSpotInner(ComponentHandle lightComponent, float* outInnerDeg);
	PIXEON_API APIResult LightComponent_SetSpotOuter(ComponentHandle lightComponent, float outerDeg);
	PIXEON_API APIResult LightComponent_GetSpotOuter(ComponentHandle lightComponent, float* outOuterDeg);
	PIXEON_API APIResult LightComponent_SetEnabled(ComponentHandle lightComponent, bool enabled);
	PIXEON_API APIResult LightComponent_IsEnabled(ComponentHandle lightComponent, bool* outEnabled);
	/*                */
}

#endif// API.h