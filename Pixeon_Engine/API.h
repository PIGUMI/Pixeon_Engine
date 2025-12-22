#ifndef API_H
#define API_H

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
typedef void* Object;
typedef void* Component;

#pragma pack(push, 1)

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
	Float3 rotation;
	Float3 scale;
} transform;

typedef struct {
	Float3 position;
	Float3 rotation;
	Float3 fixation;
} CameraTransform;

typedef struct {
	Object HitObject;
	Float3 HitPoint;
	Float3 HitNormal;
	char HitObjectName[64];
	float Distance;
} APICollisionInfo;

#pragma pack(pop)

typedef void (*BoxCollisionEnterCallback)(Component box, const APICollisionInfo* info);
typedef void (*BoxCollisionStayCallback)(Component box, const APICollisionInfo* info);
typedef void (*BoxCollisionExitCallback)(Component box, const APICollisionInfo* info);

// Utility Functions
extern "C" {
	PIXEON_API Float2 CreateFloat2(float x, float y);
	PIXEON_API Float3 CreateFloat3(float x, float y, float z);
	PIXEON_API Float4 CreateFloat4(float x, float y, float z, float w);
	PIXEON_API transform CreateTransform(Float3 pos, Float3 rot, Float3 scale);
	PIXEON_API transform IdentityTransform();
};

// Scene
extern "C" {
	PIXEON_API APIResult GetCurrentScene(SceneHandle* outScene);
	PIXEON_API APIResult ChangeScene(const char* sceneName);
	PIXEON_API APIResult SceneGetObjectCount(SceneHandle scene, int* outCount);
	PIXEON_API APIResult FindObjectByName(SceneHandle scene, const char* name, Object* outObject);
	PIXEON_API APIResult AddObjectToScene(SceneHandle scene, Object object);
	PIXEON_API APIResult RemoveObjectFromScene(SceneHandle scene, Object object);
	PIXEON_API APIResult SetMainCamera(int inCameraNumber);
	PIXEON_API APIResult GetMainCamera(int* outCameraNumber);
};

// Object
extern "C" {
	PIXEON_API APIResult GetObjectName(Object object, char* outName, int bufferSize);
	PIXEON_API APIResult SetObjectName(Object object, const char* name);
	PIXEON_API APIResult GetObjectTransform(Object object, transform* outTransform);
	PIXEON_API APIResult SetObjectTransform(Object object, const transform* inTransform);
	PIXEON_API APIResult FindComponent(Object object, const char* componentName, Component* outComponent);
};

// Component
extern "C" {
	// Camera
	PIXEON_API APIResult GetCameraTransform(Component camera, CameraTransform* outTransform);
	PIXEON_API APIResult SetCameraTransform(Component camera, const CameraTransform* inTransform);
	PIXEON_API APIResult GetCameraFov(Component camera, float* outFov);
	PIXEON_API APIResult SetCameraFov(Component camera, float inFov);
	PIXEON_API APIResult GetCameraAspect(Component camera, float* outAspect);
	PIXEON_API APIResult SetCameraAspect(Component camera, float inAspect);
	PIXEON_API APIResult GetCameraNearFar(Component camera, float* outNear, float* outFar);
	PIXEON_API APIResult SetCameraNearFar(Component camera, float inNear, float inFar);
	PIXEON_API APIResult SetChangeCameraCalculation(Component camera, bool isChange);
	PIXEON_API APIResult GetCameraNumber(Component camera, int* outCameraNumber);
	PIXEON_API APIResult GetCmaeraUpVector(Component camera, Float3* outUp);

	// light Component
	PIXEON_API APIResult GetLightType(Component light, int* outType);
	PIXEON_API APIResult SetLightType(Component light, int inType);
	PIXEON_API APIResult GetLightColor(Component light, Float3* outColor);
	PIXEON_API APIResult SetLightColor(Component light, const Float3* inColor);
	PIXEON_API APIResult GetLightIntensity(Component light, float* outIntensity);
	PIXEON_API APIResult SetLightIntensity(Component light, float inIntensity);
	PIXEON_API APIResult GetLightRange(Component light, float* outRange);
	PIXEON_API APIResult SetLightRange(Component light, float inRange);
	PIXEON_API APIResult GetLightSpotInnerOuter(Component light, float* outInnerDeg, float* outOuterDeg);
	PIXEON_API APIResult SetLightSpotInnerOuter(Component light, float inInnerDeg, float inOuterDeg);
	PIXEON_API APIResult GetLightEnabled(Component light, bool* outEnabled);
	PIXEON_API APIResult SetLightEnabled(Component light, bool inEnabled);

	// ImageRender Component
	PIXEON_API APIResult GetImageRenderTextureName(Component imageRender, char* outName, int bufferSize);
	PIXEON_API APIResult SetImageRenderTextureName(Component imageRender, const char* name);
	PIXEON_API APIResult GetImageTransform(Component imageRender, transform* outTransform);
	PIXEON_API APIResult SetImageTransform(Component imageRender, const transform* inTransform);
	PIXEON_API APIResult GetImageRenderColor(Component imageRender, Float4* outColor);
	PIXEON_API APIResult SetImageRenderColor(Component imageRender, const Float4* inColor);
	PIXEON_API APIResult GetImageRenderUVRect(Component imageRender, Float4* outUVRect);
	PIXEON_API APIResult SetImageRenderUVRect(Component imageRender, const Float4* inUVRect);

	// ModelRender Component
	PIXEON_API APIResult GetModelColor(Component modelRender, Float4* outColor);
	PIXEON_API APIResult SetModelColor(Component modelRender, const Float4* inColor);
	PIXEON_API APIResult SetMaterialTexture(Component modelRender, int materialIndex, const char* texLogicalPath);

	// Animation Component
	PIXEON_API APIResult PlayAnimation(Component animationComp);
	PIXEON_API APIResult PauseAnimation(Component animationComp);
	PIXEON_API APIResult ResumeAnimation(Component animationComp);
	PIXEON_API APIResult StopAnimation(Component animationComp);
	PIXEON_API APIResult RestartAnimation(Component animationComp);
	PIXEON_API APIResult SetAnimationClip(Component animationComp, int clipIndex);
	PIXEON_API APIResult SetAnimationPlaybackSpeed(Component animationComp, float speed);
	PIXEON_API APIResult SetAnimationLoop(Component animationComp, bool loop);
};

#endif// API.h