#ifndef API_H
#define API_H

#ifdef PixeonEngine_EXPORTS
#define PIXEON_API __declspec(dllexport)
#else
#define PIXEON_API __declspec(dllimport)
#endif

typedef enum {
	PN_SUCCESS = 0,
	PN_ERROR_NULL_POINTER = -1,
	PN_ERROR_INVALID_HANDLE = -2,
	PN_ERROR_NOT_FOUND = -3,
	PN_ERROR_BUFFER_TOO_SMALL = -4,
	PN_ERROR_INVALID_PARAMETER = -5,
}APIResult;

typedef void* scene;
typedef void* object;
typedef void* component;
typedef void* animator2d;
typedef void* keyframe;

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
	object HitObject;
	Float3 HitPoint;
	Float3 HitNormal;
	char HitObjectName[64];
	float Distance;
} APICollisionInfo;

typedef struct {
	bool bHit;
	Float3 point;
	Float3 normal;
	float distance;
	object hitObject;
	char hitObjectName[64];
} RayHit;

#pragma pack(pop)

// API.h
typedef void (*CollisionEnterCallback)(component collision, const APICollisionInfo* info);
typedef void (*CollisionStayCallback)(component collision, const APICollisionInfo* info);
typedef void (*CollisionExitCallback)(component collision, const APICollisionInfo* info);

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
	PIXEON_API APIResult GetCurrentScene(scene* outScene);
	PIXEON_API APIResult ChangeScene(const char* sceneName);
	PIXEON_API APIResult SceneGetObjectCount(scene Scene, int* outCount);
	PIXEON_API APIResult FindObjectByName(scene Scene, const char* name, object* outObject);
	PIXEON_API APIResult FindChildObjectByName(object parentObject, const char* name, object* outObject);
	PIXEON_API APIResult FindPrefabObjectByName(const char* name, object* outObject);
	PIXEON_API APIResult AddObjectToScene(scene Scene, object Object, object* CloneObject = nullptr);
	PIXEON_API APIResult RemoveObjectFromScene(scene Scene, object Object);
	PIXEON_API APIResult SetMainCameraByIndex(int inCameraNumber);
	PIXEON_API APIResult SetMainCameraByPtr(component camera);
	PIXEON_API APIResult GetMainCamera(int* outCameraNumber);
	PIXEON_API APIResult ObjectParenthood(object childObject, object parentObject);
};

extern "C" {
	PIXEON_API APIResult Raycast(
		scene InScene,
		Float3 origin,
		Float3 direction,
		float maxDistance,
		RayHit* outHit
	);

	PIXEON_API APIResult RaycastIgnoreTriggers(
		scene InScene,
		Float3 origin,
		Float3 direction,
		float maxDistance,
		RayHit* outHit
	);

	PIXEON_API APIResult SphereCast(
		scene InScene,
		Float3 origin,
		Float3 direction,
		float radius,
		float maxDistance,
		RayHit* outHit
	);

	PIXEON_API APIResult RaycastIgnoreObject(
		scene InScene,
		Float3 origin,
		Float3 direction,
		float maxDistance,
		object ignoreObject,
		RayHit* outHit
	);
};

extern "C" {
	PIXEON_API APIResult GetObjectName(object Object, char* outName, int bufferSize);
	PIXEON_API APIResult SetObjectName(object Object, const char* name);
	PIXEON_API APIResult SetObjectPosition(object Object, Float3 position);
	PIXEON_API APIResult GetObjectPosition(object Object, Float3* outPosition);
	PIXEON_API APIResult SetObjectRotation(object Object, Float3 rotation);
	PIXEON_API APIResult GetObjectRotation(object Object, Float3* outRotation);
	PIXEON_API APIResult SetObjectScale(object Object, Float3 scale);
	PIXEON_API APIResult GetObjectScale(object Object, Float3* outScale);
	PIXEON_API APIResult GetObjectTransform(object Object, transform* outTransform);
	PIXEON_API APIResult GetObjectWorldTransform(object Object, transform* outTransform);
	PIXEON_API APIResult SetObjectTransform(object Object, const transform* inTransform);
	PIXEON_API APIResult CountChildObjects(object Object, int* outCount);
	PIXEON_API APIResult FindComponent(object Object, const char* componentName, component* outComponent);
};

extern "C" {
	PIXEON_API APIResult GetVariableInt(object InObj, const char* InVarName, int* OutValue);
	PIXEON_API APIResult SetVariableInt(object InObj, const char* InVarName, int InValue);
	PIXEON_API APIResult GetVariableFloat(object InObj, const char* InVarName, float* OutValue);
	PIXEON_API APIResult SetVariableFloat(object InObj, const char* InVarName, float InValue);
	PIXEON_API APIResult GetVariableBool(object InObj, const char* InVarName, bool* OutValue);
	PIXEON_API APIResult SetVariableBool(object InObj, const char* InVarName, bool InValue);
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
	PIXEON_API APIResult GetCameraTransform(component camera, CameraTransform* outTransform);
	PIXEON_API APIResult SetCameraTransform(component camera, const CameraTransform* inTransform);
	PIXEON_API APIResult GetCameraFov(component camera, float* outFov);
	PIXEON_API APIResult SetCameraFov(component camera, float inFov);
	PIXEON_API APIResult GetCameraAspect(component camera, float* outAspect);
	PIXEON_API APIResult SetCameraAspect(component camera, float inAspect);
	PIXEON_API APIResult GetCameraNearFar(component camera, float* outNear, float* outFar);
	PIXEON_API APIResult SetCameraNearFar(component camera, float inNear, float inFar);
	PIXEON_API APIResult SetChangeCameraCalculation(component camera, bool isChange);
	PIXEON_API APIResult GetCameraNumberAPI(component camera, int* outCameraNumber);
	PIXEON_API APIResult GetCameraUpVector(component camera, Float3* outUp);
	PIXEON_API APIResult GetCameraRightVector(component camera, Float3* outRight);
	PIXEON_API APIResult GetCameraForwardVector(component camera, Float3* outForward);

	// light Component
	PIXEON_API APIResult GetLightType(component light, int* outType);
	PIXEON_API APIResult SetLightType(component light, int inType);
	PIXEON_API APIResult GetLightColor(component light, Float3* outColor);
	PIXEON_API APIResult SetLightColor(component light, const Float3* inColor);
	PIXEON_API APIResult GetLightIntensity(component light, float* outIntensity);
	PIXEON_API APIResult SetLightIntensity(component light, float inIntensity);
	PIXEON_API APIResult GetLightRange(component light, float* outRange);
	PIXEON_API APIResult SetLightRange(component light, float inRange);
	PIXEON_API APIResult GetLightSpotInnerOuter(component light, float* outInnerDeg, float* outOuterDeg);
	PIXEON_API APIResult SetLightSpotInnerOuter(component light, float inInnerDeg, float inOuterDeg);
	PIXEON_API APIResult GetLightEnabled(component light, bool* outEnabled);
	PIXEON_API APIResult SetLightEnabled(component light, bool inEnabled);

	// ImageRender Component
	PIXEON_API APIResult GetImageRenderTextureName(component imageRender, char* outName, int bufferSize);
	PIXEON_API APIResult SetImageRenderTextureName(component imageRender, const char* name);
	PIXEON_API APIResult GetImageTransform(component imageRender, transform* outTransform);
	PIXEON_API APIResult SetImageTransform(component imageRender, const transform* inTransform);
	PIXEON_API APIResult GetImageRenderColor(component imageRender, Float4* outColor);
	PIXEON_API APIResult SetImageRenderColor(component imageRender, const Float4* inColor);
	PIXEON_API APIResult GetImageRenderUVRect(component imageRender, Float4* outUVRect);
	PIXEON_API APIResult SetImageRenderUVRect(component imageRender, const Float4* inUVRect);

	// ModelRender Component
	PIXEON_API APIResult GetModelColor(component modelRender, Float4* outColor);
	PIXEON_API APIResult SetModelColor(component modelRender, const Float4* inColor);
	PIXEON_API APIResult SetMaterialTexture(component modelRender, int materialIndex, const char* texLogicalPath);
	PIXEON_API APIResult GetOffsetPosition(component modelRender, Float3* outPosition);
	PIXEON_API APIResult SetOffsetPosition(component modelRender, const Float3* inPosition);
	PIXEON_API APIResult GetOffsetRotation(component modelRender, Float3* outRotation);
	PIXEON_API APIResult SetOffsetRotation(component modelRender, const Float3* inRotation);
	PIXEON_API APIResult GetOffsetScale(component modelRender, Float3* outScale);
	PIXEON_API APIResult SetOffsetScale(component modelRender, const Float3* inScale);
	PIXEON_API APIResult GetBoneName(component modelRender, int boneIndex, char* outName, int bufferSize);
	PIXEON_API APIResult GetBoneWorldPosition(component modelRender, int boneIndex, Float3* outPosition);
	PIXEON_API APIResult GetBoneWorldRotation(component modelRender, int boneIndex, Float3* outRotation);

	// Animation Component
	PIXEON_API APIResult PlayAnimation(component animationComp);
	PIXEON_API APIResult PauseAnimation(component animationComp);
	PIXEON_API APIResult ResumeAnimation(component animationComp);
	PIXEON_API APIResult StopAnimation(component animationComp);
	PIXEON_API APIResult RestartAnimation(component animationComp);
	PIXEON_API APIResult SetAnimationClip(component animationComp, int clipIndex);
	PIXEON_API APIResult GetAnimationClip(component animationComp, int* outClipIndex);
	PIXEON_API APIResult SetAnimationPlaybackSpeed(component animationComp, float speed);
	PIXEON_API APIResult SetAnimationLoop(component animationComp, bool loop);

	// RigidBody Component
	PIXEON_API APIResult RigidBodyAddForce(component rigidBodyComp, const Float3* inForce);
	PIXEON_API APIResult RigidBodyAddImpulse(component rigidBodyComp, const Float3* inImpulse);
	PIXEON_API APIResult RigidBodyGetVelocity(component rigidBodyComp, Float3* outVelocity);
	PIXEON_API APIResult RigidBodySetVelocity(component rigidBodyComp, const Float3* inVelocity);
	PIXEON_API APIResult RigidBodySetKinematic(component rigidBodyComp, bool isKinematic);
	PIXEON_API APIResult RigidBodyGetKinematic(component rigidBodyComp, bool* outIsKinematic);
	PIXEON_API APIResult RigidBodySetMass(component rigidBodyComp, float mass);
	PIXEON_API APIResult RigidBodyGetMass(component rigidBodyComp, float* outMass);
	PIXEON_API APIResult RigidBodySetUseGravity(component rigidBodyComp, bool useGravity);
	PIXEON_API APIResult RigidBodyGetUseGravity(component rigidBodyComp, bool* outUseGravity);
	PIXEON_API APIResult RigidBodySetFriction(component rigidBodyComp, float friction);
	PIXEON_API APIResult RigidBodyGetFriction(component rigidBodyComp, float* outFriction);
	PIXEON_API APIResult RigidBodySetRestitution(component rigidBodyComp, float restitution);
	PIXEON_API APIResult RigidBodyGetRestitution(component rigidBodyComp, float* outRestitution);
	PIXEON_API APIResult RigidBodySetLinearDamping(component rigidBodyComp, float damping);
	PIXEON_API APIResult RigidBodyGetLinearDamping(component rigidBodyComp, float* outDamping);
	PIXEON_API APIResult RigidBodySetAngularDamping(component rigidBodyComp, float damping);
	PIXEON_API APIResult RigidBodyGetAngularDamping(component rigidBodyComp, float* outDamping);
	PIXEON_API APIResult RigidBodySetDamping(component rigidBodyComp, float linearDamping, float angularDamping);
	PIXEON_API APIResult RigidBodySetRollingFriction(component rigidBodyComp, float rollingFriction);
	PIXEON_API APIResult RigidBodyGetRollingFriction(component rigidBodyComp, float* outRollingFriction);
	PIXEON_API APIResult RigidBodySetSpinningFriction(component rigidBodyComp, float spinningFriction);
	PIXEON_API APIResult RigidBodyGetSpinningFriction(component rigidBodyComp, float* outSpinningFriction);

	// BoxCollision Component
	PIXEON_API APIResult BoxCollisionSetSize(component Component, Float3 size);
	PIXEON_API APIResult BoxCollisionGetSize(component Component, Float3* outSize);
	PIXEON_API APIResult BoxCollisionSetCenter(component Component, Float3 center);
	PIXEON_API APIResult BoxCollisionGetCenter(component Component, Float3* outCenter);
	PIXEON_API APIResult BoxCollisionSetIsTrigger(component Component, bool isTrigger);
	PIXEON_API APIResult BoxCollisionGetIsTrigger(component Component, bool* outIsTrigger);

	PIXEON_API APIResult CollisionSetCollisionEnterCallback(component Component, CollisionEnterCallback callback);
	PIXEON_API APIResult CollisionSetCollisionStayCallback(component Component, CollisionStayCallback callback);
	PIXEON_API APIResult CollisionSetCollisionExitCallback(component Component, CollisionExitCallback callback);

	// Animator2D Component
	PIXEON_API APIResult GetAnimator2D(component animatorComp, const char* animatorName, animator2d* outHandel);
	PIXEON_API APIResult Animator2DPlay(animator2d animator);
	PIXEON_API APIResult Animator2DStop(animator2d animator);
	PIXEON_API APIResult Animator2DIsEnd(animator2d animator, bool* End);
	PIXEON_API APIResult FindKeyFrame(animator2d animator, const char* keyname, keyframe* outKeyframe);
	PIXEON_API APIResult SetVertexOffsetUp(keyframe Keyframe, float offset);
	PIXEON_API APIResult SetVertexOffsetDown(keyframe Keyframe, float offset);
	PIXEON_API APIResult SetVertexOffsetLeft(keyframe Keyframe, float offset);
	PIXEON_API APIResult SetVertexOffsetRight(keyframe Keyframe, float offset);
	PIXEON_API APIResult GetVertexOffsetUp(keyframe Keyframe, float* outOffset);
	PIXEON_API APIResult GetVertexOffsetDown(keyframe Keyframe, float* outOffset);
	PIXEON_API APIResult GetVertexOffsetLeft(keyframe Keyframe, float* outOffset);
	PIXEON_API APIResult GetVertexOffsetRight(keyframe Keyframe, float* outOffset);

	PIXEON_API APIResult EffectPlay(component effectComp);
	PIXEON_API APIResult EffectStop(component effectComp);
	PIXEON_API APIResult EffectIsPlaying(component effectComp, bool* outIsPlaying);

	PIXEON_API APIResult CallScriptFunction(component Script, const char* functionName);
};

#endif// API.h