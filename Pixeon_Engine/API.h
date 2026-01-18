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
typedef void* Animator2d;
typedef void* Keyframe;

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
	PIXEON_API APIResult FindChildObjectByName(Object parentObject, const char* name, Object* outObject);
	PIXEON_API APIResult FindPrefabObjectByName(const char* name, Object* outObject);
	PIXEON_API APIResult AddObjectToScene(SceneHandle scene, Object object);
	PIXEON_API APIResult RemoveObjectFromScene(SceneHandle scene, Object object);
	PIXEON_API APIResult SetMainCamera(int inCameraNumber);
	PIXEON_API APIResult GetMainCamera(int* outCameraNumber);
};

// Object
extern "C" {
	PIXEON_API APIResult GetObjectName(Object object, char* outName, int bufferSize);
	PIXEON_API APIResult SetObjectName(Object object, const char* name);
	PIXEON_API APIResult SetObjectPosition(Object object, Float3 position);
	PIXEON_API APIResult GetObjectPosition(Object object, Float3* outPosition);
	PIXEON_API APIResult SetObjectRotation(Object object, Float3 rotation);
	PIXEON_API APIResult GetObjectRotation(Object object, Float3* outRotation);
	PIXEON_API APIResult SetObjectScale(Object object, Float3 scale);
	PIXEON_API APIResult GetObjectScale(Object object, Float3* outScale);
	PIXEON_API APIResult GetObjectTransform(Object object, transform* outTransform);
	PIXEON_API APIResult SetObjectTransform(Object object, const transform* inTransform);
	PIXEON_API APIResult FindComponent(Object object, const char* componentName, Component* outComponent);
};

// InputKey
extern "C" {
	PIXEON_API bool KeyPressed(char keyCode);
	PIXEON_API bool KeyTriggered(char keyCode);
	PIXEON_API bool KeyReleased(char keyCode);
	PIXEON_API bool KeyRepeated(char keyCode);
	PIXEON_API int GetMouseMoveX();
	PIXEON_API int GetMouseMoveY();
	PIXEON_API APIResult FixedMouseCursor(bool enbled);
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
	PIXEON_API APIResult GetCameraUpVector(Component camera, Float3* outUp);
	PIXEON_API APIResult GetCameraRightVector(Component camera, Float3* outRight);
	PIXEON_API APIResult GetCameraForwardVector(Component camera, Float3* outForward);

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

	// RigidBody Component
	PIXEON_API APIResult RigidBodyAddForce(Component rigidBodyComp, const Float3* inForce);
	PIXEON_API APIResult RigidBodyAddImpulse(Component rigidBodyComp, const Float3* inImpulse);
	PIXEON_API APIResult RigidBodyGetVelocity(Component rigidBodyComp, Float3* outVelocity);
	PIXEON_API APIResult RigidBodySetVelocity(Component rigidBodyComp, const Float3* inVelocity);
	PIXEON_API APIResult RigidBodySetKinematic(Component rigidBodyComp, bool isKinematic);
	PIXEON_API APIResult RigidBodyGetKinematic(Component rigidBodyComp, bool* outIsKinematic);
	PIXEON_API APIResult RigidBodySetMass(Component rigidBodyComp, float mass);
	PIXEON_API APIResult RigidBodyGetMass(Component rigidBodyComp, float* outMass);
	PIXEON_API APIResult RigidBodySetUseGravity(Component rigidBodyComp, bool useGravity);
	PIXEON_API APIResult RigidBodyGetUseGravity(Component rigidBodyComp, bool* outUseGravity);
	PIXEON_API APIResult RigidBodySetFriction(Component rigidBodyComp, float friction);
	PIXEON_API APIResult RigidBodyGetFriction(Component rigidBodyComp, float* outFriction);
	PIXEON_API APIResult RigidBodySetRestitution(Component rigidBodyComp, float restitution);
	PIXEON_API APIResult RigidBodyGetRestitution(Component rigidBodyComp, float* outRestitution);
	PIXEON_API APIResult RigidBodySetLinearDamping(Component rigidBodyComp, float damping);
	PIXEON_API APIResult RigidBodyGetLinearDamping(Component rigidBodyComp, float* outDamping);
	PIXEON_API APIResult RigidBodySetAngularDamping(Component rigidBodyComp, float damping);
	PIXEON_API APIResult RigidBodyGetAngularDamping(Component rigidBodyComp, float* outDamping);
	PIXEON_API APIResult RigidBodySetDamping(Component rigidBodyComp, float linearDamping, float angularDamping);
	PIXEON_API APIResult RigidBodySetRollingFriction(Component rigidBodyComp, float rollingFriction);
	PIXEON_API APIResult RigidBodyGetRollingFriction(Component rigidBodyComp, float* outRollingFriction);
	PIXEON_API APIResult RigidBodySetSpinningFriction(Component rigidBodyComp, float spinningFriction);
	PIXEON_API APIResult RigidBodyGetSpinningFriction(Component rigidBodyComp, float* outSpinningFriction);

	// BoxCollision Component
	PIXEON_API APIResult BoxCollisionSetSize(Component component, Float3 size);
	PIXEON_API APIResult BoxCollisionGetSize(Component component, Float3* outSize);
	PIXEON_API APIResult BoxCollisionSetCenter(Component component, Float3 center);
	PIXEON_API APIResult BoxCollisionGetCenter(Component component, Float3* outCenter);
	PIXEON_API APIResult BoxCollisionSetIsTrigger(Component component, bool isTrigger);
	PIXEON_API APIResult BoxCollisionGetIsTrigger(Component component, bool* outIsTrigger);

	PIXEON_API APIResult BoxCollisionSetCollisionEnterCallback(Component component, BoxCollisionEnterCallback callback);
	PIXEON_API APIResult BoxCollisionSetCollisionStayCallback(Component component, BoxCollisionStayCallback callback);
	PIXEON_API APIResult BoxCollisionSetCollisionExitCallback(Component component, BoxCollisionExitCallback callback);

	// Animator2D Component
	PIXEON_API APIResult GetAnimator2D(Component animatorComp,const char* animatorName,Animator2d* outHandel);
	PIXEON_API APIResult Animator2DPlay(Animator2d animator);
	PIXEON_API APIResult Animator2DStop(Animator2d animator);
	PIXEON_API APIResult Animator2DIsEnd(Animator2d animator,bool* End);
	PIXEON_API APIResult FindKeyFrame(Animator2d animator, const char* keyname, Keyframe* outKeyframe);
	PIXEON_API APIResult SetVertexOffsetUp(Keyframe keyframe,float offset);
	PIXEON_API APIResult SetVertexOffsetDown(Keyframe keyframe, float offset);
	PIXEON_API APIResult SetVertexOffsetLeft(Keyframe keyframe, float offset);
	PIXEON_API APIResult SetVertexOffsetRight(Keyframe keyframe, float offset);

};

#endif// API.h