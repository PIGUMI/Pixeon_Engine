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
#include "component.h"
#include "Scene.h"
#include "object.h"
#include "BaseCollision.h"

#include "CameraComponent.h"
#include "LightComponent.h"
#include "ImageRender.h"
#include "ModelRender.h"
#include "AnimationComponent.h"
#include "RigidBody.h"
#include "BoxCollision.h"
#include "Animator2DComponent.h"
#include "animator2d.h"
#include "BulletPhysics/btBulletDynamicsCommon.h"
#include "Input.h"
#include "EffectComponent.h"
#include "ScripComponent.h"

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

	CollisionEnterCallback g_BoxCollisionEnterCallback = nullptr;
	CollisionStayCallback  g_BoxCollisionStayCallback = nullptr;
	CollisionExitCallback  g_BoxCollisionExitCallback = nullptr;

	// CollisionInfoからAPICollisionInfoへ変換
	APICollisionInfo ToAPICollisionInfo(const CollisionInfo& info) {
		APICollisionInfo apiInfo;
		apiInfo.HitObject = reinterpret_cast<object>(info.HitObject);
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

// AbstractScene Functions
extern "C" {
	PIXEON_API APIResult GetCurrentScene(scene* outScene)
	{
		if (!outScene) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractScene* currentScene = SceneManger::GetInstance()->GetCurrentScene();
		if (!currentScene) {
			*outScene = nullptr;
			return PN_ERROR_NOT_FOUND;
		}
		*outScene = reinterpret_cast<scene>(currentScene);
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
	PIXEON_API APIResult SceneGetObjectCount(scene InScene, int* outCount)
	{
		if (!outCount) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractScene* scenePtr = nullptr;
		if (!ValidateHandle<AbstractScene>(InScene, &scenePtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outCount = static_cast<int>(scenePtr->GetObjects().size());
	}
	PIXEON_API APIResult FindObjectByName(scene InScene, const char* name, object* outObject)
	{
		if (!name || !outObject) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractScene* scenePtr = nullptr;
		if (!ValidateHandle<AbstractScene>(InScene, &scenePtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AbstractObject* obj = scenePtr->FindObjectByName(name);
		if (!obj) {
			*outObject = nullptr;
			return PN_ERROR_NOT_FOUND;
		}
		*outObject = reinterpret_cast<object>(obj);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult FindPrefabObjectByName(const char* name, object* outObject)
	{
		if (!name || !outObject) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* obj = EngineFrame::GetInstance()->GetPrefabByName(std::string(name));
		if (!obj) {
			*outObject = nullptr;
			return PN_ERROR_NOT_FOUND;
		}
		*outObject = reinterpret_cast<object>(obj);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult FindChildObjectByName(object parentObject, const char* name, object* outObject)
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
		*outObject = reinterpret_cast<object>(childObj);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult AddObjectToScene(scene InScene, object Object, object* CloneObject)
	{
		if (!Object) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractScene* scenePtr = nullptr;
		if (!ValidateHandle<AbstractScene>(InScene, &scenePtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(Object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AbstractObject* CloneObj = objPtr->Clone();
		scenePtr->AddObjectLocal(CloneObj);
		if (CloneObject) {
			*CloneObject = reinterpret_cast<object>(CloneObj);
		}
		return PN_SUCCESS;
	}
	PIXEON_API APIResult RemoveObjectFromScene(scene InScene, object Object)
	{
		if (!Object) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractScene* scenePtr = nullptr;
		if (!ValidateHandle<AbstractScene>(InScene, &scenePtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(Object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		scenePtr->RemoveObject(objPtr);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetMainCameraByIndex(int inCameraNumber)
	{
		AbstractScene* currentScene = SceneManger::GetInstance()->GetCurrentScene();
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
		AbstractScene* currentScene = SceneManger::GetInstance()->GetCurrentScene();
		if (!currentScene) {
			return PN_ERROR_NOT_FOUND;
		}
		*outCameraNumber = currentScene->GetMainCameraNumber();
	}
	PIXEON_API APIResult ObjectParenthood(object childObject, object parentObject)
	{
		if (!childObject || !parentObject) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* childObjPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(childObject, &childObjPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AbstractObject* parentObjPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(parentObject, &parentObjPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		childObjPtr->SetParent(parentObjPtr);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetMainCameraByPtr(component camera)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(camera, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		CameraComponent* cameraComp = dynamic_cast<CameraComponent*>(compPtr);
		if (!cameraComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AbstractScene* currentScene = SceneManger::GetInstance()->GetCurrentScene();
		if (!currentScene) {
			return PN_ERROR_NOT_FOUND;
		}
		currentScene->SetMainCamera(cameraComp);
		return PN_SUCCESS;
	}
};

extern "C" {
	PIXEON_API APIResult Raycast(
		scene InScene,
		Float3 origin,
		Float3 direction,
		float maxDistance,
		RayHit* outHit)
	{
		if (!InScene || !outHit) {
			return PN_ERROR_INVALID_PARAMETER;
		}

		AbstractScene* scene = nullptr;
		if (!ValidateHandle<AbstractScene>(InScene, &scene)) {
			return PN_ERROR_INVALID_HANDLE;
		}

		btDiscreteDynamicsWorld* physicsWorld = scene->GetPhysicsWorld();
		if (!physicsWorld) {
			return PN_ERROR_NOT_FOUND;
		}

		// 方向ベクトルを正規化
		float length = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
		if (length < 0.0001f) {
			return PN_ERROR_INVALID_PARAMETER;
		}

		Float3 normalizedDir = {
			direction.x / length,
			direction.y / length,
			direction.z / length
		};

		// レイの開始位置と終了位置を計算
		btVector3 rayFrom(origin.x, origin.y, origin.z);
		btVector3 rayTo(
			origin.x + normalizedDir.x * maxDistance,
			origin.y + normalizedDir.y * maxDistance,
			origin.z + normalizedDir.z * maxDistance
		);

		// レイキャストを実行
		btCollisionWorld::ClosestRayResultCallback rayCallback(rayFrom, rayTo);
		physicsWorld->rayTest(rayFrom, rayTo, rayCallback);

		if (rayCallback.hasHit()) {
			outHit->bHit = true;

			// ヒット位置
			outHit->point = CreateFloat3(
				rayCallback.m_hitPointWorld.getX(),
				rayCallback.m_hitPointWorld.getY(),
				rayCallback.m_hitPointWorld.getZ()
			);

			// 法線
			outHit->normal = CreateFloat3(
				rayCallback.m_hitNormalWorld.getX(),
				rayCallback.m_hitNormalWorld.getY(),
				rayCallback.m_hitNormalWorld.getZ()
			);

			// 距離
			Float3 diff = {
				outHit->point.x - origin.x,
				outHit->point.y - origin.y,
				outHit->point.z - origin.z
			};
			outHit->distance = sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

			// ヒットしたオブジェクトを取得
			const btCollisionObject* collisionObject = rayCallback.m_collisionObject;
			if (collisionObject) {
				const btRigidBody* rigidBody = btRigidBody::upcast(collisionObject);
				if (rigidBody && rigidBody->getUserPointer()) {
					RigidBody* rbComponent = static_cast<RigidBody*>(rigidBody->getUserPointer());
					if (rbComponent) {
						AbstractObject* hitObj = rbComponent->GetParent();
						outHit->hitObject = reinterpret_cast<object>(hitObj);

						// オブジェクト名を取得
						std::string objName = hitObj->GetObjectName();
						strncpy_s(outHit->hitObjectName, sizeof(outHit->hitObjectName), objName.c_str(), _TRUNCATE);
					}
				}
			}
			else {
				outHit->hitObject = nullptr;
				outHit->hitObjectName[0] = '\0';
			}

			return PN_SUCCESS;
		}

		outHit->bHit = false;
		outHit->hitObject = nullptr;
		outHit->hitObjectName[0] = '\0';
		return PN_SUCCESS;
	}

	PIXEON_API APIResult RaycastIgnoreTriggers(
		scene InScene,
		Float3 origin,
		Float3 direction,
		float maxDistance,
		RayHit* outHit)
	{
		if (!InScene || !outHit) {
			return PN_ERROR_INVALID_PARAMETER;
		}

		AbstractScene* scene = nullptr;
		if (!ValidateHandle<AbstractScene>(InScene, &scene)) {
			return PN_ERROR_INVALID_HANDLE;
		}

		btDiscreteDynamicsWorld* physicsWorld = scene->GetPhysicsWorld();
		if (!physicsWorld) {
			return PN_ERROR_NOT_FOUND;
		}

		// 方向ベクトルを正規化
		float length = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
		if (length < 0.0001f) {
			return PN_ERROR_INVALID_PARAMETER;
		}

		Float3 normalizedDir = {
			direction.x / length,
			direction.y / length,
			direction.z / length
		};

		btVector3 rayFrom(origin.x, origin.y, origin.z);
		btVector3 rayTo(
			origin.x + normalizedDir.x * maxDistance,
			origin.y + normalizedDir.y * maxDistance,
			origin.z + normalizedDir.z * maxDistance
		);

		// カスタムコールバックでトリガーを無視
		struct ClosestNotTrigger : public btCollisionWorld::ClosestRayResultCallback {
			ClosestNotTrigger(const btVector3& from, const btVector3& to)
				: btCollisionWorld::ClosestRayResultCallback(from, to) {}

			btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
				// トリガー（CF_NO_CONTACT_RESPONSE）を持つオブジェクトを無視
				if (rayResult.m_collisionObject->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE) {
					return 1.0f; // このヒットを無視
				}
				return ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
			}
		};

		ClosestNotTrigger rayCallback(rayFrom, rayTo);
		physicsWorld->rayTest(rayFrom, rayTo, rayCallback);

		if (rayCallback.hasHit()) {
			outHit->bHit = true;

			outHit->point = CreateFloat3(
				rayCallback.m_hitPointWorld.getX(),
				rayCallback.m_hitPointWorld.getY(),
				rayCallback.m_hitPointWorld.getZ()
			);

			outHit->normal = CreateFloat3(
				rayCallback.m_hitNormalWorld.getX(),
				rayCallback.m_hitNormalWorld.getY(),
				rayCallback.m_hitNormalWorld.getZ()
			);

			Float3 diff = {
				outHit->point.x - origin.x,
				outHit->point.y - origin.y,
				outHit->point.z - origin.z
			};
			outHit->distance = sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

			const btCollisionObject* collisionObject = rayCallback.m_collisionObject;
			if (collisionObject) {
				const btRigidBody* rigidBody = btRigidBody::upcast(collisionObject);
				if (rigidBody && rigidBody->getUserPointer()) {
					RigidBody* rbComponent = static_cast<RigidBody*>(rigidBody->getUserPointer());
					if (rbComponent) {
						AbstractObject* hitObj = rbComponent->GetParent();
						outHit->hitObject = reinterpret_cast<object>(hitObj);

						std::string objName = hitObj->GetObjectName();
						strncpy_s(outHit->hitObjectName, sizeof(outHit->hitObjectName), objName.c_str(), _TRUNCATE);
					}
				}
			}
			else {
				outHit->hitObject = nullptr;
				outHit->hitObjectName[0] = '\0';
			}

			return PN_SUCCESS;
		}

		outHit->bHit = false;
		outHit->hitObject = nullptr;
		outHit->hitObjectName[0] = '\0';
		return PN_SUCCESS;
	}

	PIXEON_API APIResult SphereCast(
		scene InScene,
		Float3 origin,
		Float3 direction,
		float radius,
		float maxDistance,
		RayHit* outHit)
	{
		if (!InScene || !outHit) {
			return PN_ERROR_INVALID_PARAMETER;
		}

		AbstractScene* scene = nullptr;
		if (!ValidateHandle<AbstractScene>(InScene, &scene)) {
			return PN_ERROR_INVALID_HANDLE;
		}

		btDiscreteDynamicsWorld* physicsWorld = scene->GetPhysicsWorld();
		if (!physicsWorld) {
			return PN_ERROR_NOT_FOUND;
		}

		// 方向ベクトルを正規化
		float length = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
		if (length < 0.0001f) {
			return PN_ERROR_INVALID_PARAMETER;
		}

		Float3 normalizedDir = {
			direction.x / length,
			direction.y / length,
			direction.z / length
		};

		// 球体の形状を作成
		btSphereShape sphereShape(radius);

		// 開始と終了のトランスフォーム
		btTransform transformFrom;
		transformFrom.setIdentity();
		transformFrom.setOrigin(btVector3(origin.x, origin.y, origin.z));

		btTransform transformTo;
		transformTo.setIdentity();
		transformTo.setOrigin(btVector3(
			origin.x + normalizedDir.x * maxDistance,
			origin.y + normalizedDir.y * maxDistance,
			origin.z + normalizedDir.z * maxDistance
		));

		// ConvexCastを実行
		btCollisionWorld::ClosestConvexResultCallback convexCallback(
			transformFrom.getOrigin(),
			transformTo.getOrigin()
		);

		physicsWorld->convexSweepTest(&sphereShape, transformFrom, transformTo, convexCallback);

		if (convexCallback.hasHit()) {
			outHit->bHit = true;

			outHit->point = CreateFloat3(
				convexCallback.m_hitPointWorld.getX(),
				convexCallback.m_hitPointWorld.getY(),
				convexCallback.m_hitPointWorld.getZ()
			);

			outHit->normal = CreateFloat3(
				convexCallback.m_hitNormalWorld.getX(),
				convexCallback.m_hitNormalWorld.getY(),
				convexCallback.m_hitNormalWorld.getZ()
			);

			Float3 diff = {
				outHit->point.x - origin.x,
				outHit->point.y - origin.y,
				outHit->point.z - origin.z
			};
			outHit->distance = sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

			const btCollisionObject* collisionObject = convexCallback.m_hitCollisionObject;
			if (collisionObject) {
				const btRigidBody* rigidBody = btRigidBody::upcast(collisionObject);
				if (rigidBody && rigidBody->getUserPointer()) {
					RigidBody* rbComponent = static_cast<RigidBody*>(rigidBody->getUserPointer());
					if (rbComponent) {
						AbstractObject* hitObj = rbComponent->GetParent();
						outHit->hitObject = reinterpret_cast<object>(hitObj);

						std::string objName = hitObj->GetObjectName();
						strncpy_s(outHit->hitObjectName, sizeof(outHit->hitObjectName), objName.c_str(), _TRUNCATE);
					}
				}
			}
			else {
				outHit->hitObject = nullptr;
				outHit->hitObjectName[0] = '\0';
			}

			return PN_SUCCESS;
		}

		outHit->bHit = false;
		outHit->hitObject = nullptr;
		outHit->hitObjectName[0] = '\0';
		return PN_SUCCESS;
	}

	PIXEON_API APIResult RaycastIgnoreObject(
		scene InScene,
		Float3 origin,
		Float3 direction,
		float maxDistance,
		object ignoreObject,
		RayHit* outHit)
	{
		if (!InScene || !outHit) {
			return PN_ERROR_INVALID_PARAMETER;
		}

		AbstractScene* scene = nullptr;
		if (!ValidateHandle<AbstractScene>(InScene, &scene)) {
			return PN_ERROR_INVALID_HANDLE;
		}

		btDiscreteDynamicsWorld* physicsWorld = scene->GetPhysicsWorld();
		if (!physicsWorld) {
			return PN_ERROR_NOT_FOUND;
		}

		// 方向ベクトルを正規化
		float length = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
		if (length < 0.0001f) {
			return PN_ERROR_INVALID_PARAMETER;
		}

		Float3 normalizedDir = {
			direction.x / length,
			direction.y / length,
			direction.z / length
		};

		btVector3 rayFrom(origin.x, origin.y, origin.z);
		btVector3 rayTo(
			origin.x + normalizedDir.x * maxDistance,
			origin.y + normalizedDir.y * maxDistance,
			origin.z + normalizedDir.z * maxDistance
		);

		AbstractObject* ignoreObjPtr = reinterpret_cast<AbstractObject*>(ignoreObject);

		// カスタムコールバックで特定のオブジェクトを無視
		struct ClosestNotIgnored : public btCollisionWorld::ClosestRayResultCallback {
			AbstractObject* ignoreObj;

			ClosestNotIgnored(const btVector3& from, const btVector3& to, AbstractObject* ignore)
				: btCollisionWorld::ClosestRayResultCallback(from, to), ignoreObj(ignore) {}

			btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace) override {
				const btCollisionObject* collisionObject = rayResult.m_collisionObject;
				if (collisionObject) {
					const btRigidBody* rigidBody = btRigidBody::upcast(collisionObject);
					if (rigidBody && rigidBody->getUserPointer()) {
						RigidBody* rbComponent = static_cast<RigidBody*>(rigidBody->getUserPointer());
						if (rbComponent && rbComponent->GetParent() == ignoreObj) {
							return 1.0f; // このヒットを無視
						}
					}
				}
				return ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
			}
		};

		ClosestNotIgnored rayCallback(rayFrom, rayTo, ignoreObjPtr);
		physicsWorld->rayTest(rayFrom, rayTo, rayCallback);

		if (rayCallback.hasHit()) {
			outHit->bHit = true;

			outHit->point = CreateFloat3(
				rayCallback.m_hitPointWorld.getX(),
				rayCallback.m_hitPointWorld.getY(),
				rayCallback.m_hitPointWorld.getZ()
			);

			outHit->normal = CreateFloat3(
				rayCallback.m_hitNormalWorld.getX(),
				rayCallback.m_hitNormalWorld.getY(),
				rayCallback.m_hitNormalWorld.getZ()
			);

			Float3 diff = {
				outHit->point.x - origin.x,
				outHit->point.y - origin.y,
				outHit->point.z - origin.z
			};
			outHit->distance = sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

			const btCollisionObject* collisionObject = rayCallback.m_collisionObject;
			if (collisionObject) {
				const btRigidBody* rigidBody = btRigidBody::upcast(collisionObject);
				if (rigidBody && rigidBody->getUserPointer()) {
					RigidBody* rbComponent = static_cast<RigidBody*>(rigidBody->getUserPointer());
					if (rbComponent) {
						AbstractObject* hitObj = rbComponent->GetParent();
						outHit->hitObject = reinterpret_cast<object>(hitObj);

						std::string objName = hitObj->GetObjectName();
						strncpy_s(outHit->hitObjectName, sizeof(outHit->hitObjectName), objName.c_str(), _TRUNCATE);
					}
				}
			}
			else {
				outHit->hitObject = nullptr;
				outHit->hitObjectName[0] = '\0';
			}

			return PN_SUCCESS;
		}

		outHit->bHit = false;
		outHit->hitObject = nullptr;
		outHit->hitObjectName[0] = '\0';
		return PN_SUCCESS;
	}
};

// object Functions
extern "C" {
	PIXEON_API APIResult GetObjectName(object object, char* outName, int bufferSize)
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
	PIXEON_API APIResult SetObjectName(object object, const char* name)
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
	PIXEON_API APIResult SetObjectPosition(object object, Float3 position) {
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		objPtr->SetPosition(position.x, position.y, position.z);

		for (auto rb : objPtr->GetComponents())
		{
			if (rb->GetComponentType() == ComponentManager::COMPONENT_TYPE::RIGIDBODY)
			{
				static_cast<RigidBody*>(rb)->SyncPositionToBullet(ToXMFloat3(position));
			}
		}
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetObjectPosition(object object, Float3* outPosition)
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
	PIXEON_API APIResult SetObjectRotation(object object, Float3 rotation)
	{
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		objPtr->SetRotation(rotation.x, rotation.y, rotation.z);

		for (auto rb : objPtr->GetComponents())
		{
			if (rb->GetComponentType() == ComponentManager::COMPONENT_TYPE::RIGIDBODY)
			{
				static_cast<RigidBody*>(rb)->SyncRotationToBullet(ToXMFloat3(rotation));
			}
		}
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetObjectRotation(object object, Float3* outRotation)
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
	PIXEON_API APIResult SetObjectScale(object object, Float3 scale)
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
	PIXEON_API APIResult GetObjectScale(object object, Float3* outScale)
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
	PIXEON_API APIResult GetObjectTransform(object object, transform* outTransform)
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
	PIXEON_API APIResult GetObjectWorldTransform(object object, transform* outTransform)
	{
		if (!outTransform) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		Transform transform = objPtr->GetWorldTransform();
		outTransform->position = ToFloat3(transform.position);
		outTransform->rotation = ToFloat3(transform.rotation);
		outTransform->scale = ToFloat3(transform.scale);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetObjectTransform(object object, const transform* inTransform)
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

		for (auto rb : objPtr->GetComponents())
		{
			if (rb->GetComponentType() == ComponentManager::COMPONENT_TYPE::RIGIDBODY)
			{
				static_cast<RigidBody*>(rb)->SyncTransformToBullet();
			}
		}
		return PN_SUCCESS;
	}
	PIXEON_API APIResult FindComponent(object object, const char* componentName, component* outComponent)
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
		*outComponent = reinterpret_cast<component>(comp);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult CountChildObjects(object object, int* outCount)
	{
		if (!outCount) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(object, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outCount = static_cast<int>(objPtr->GetChildren().size());
		return PN_SUCCESS;
	}
};

extern "C" {
	PIXEON_API APIResult GetVariableInt(object InObj, const char* InVarName, int* OutValue)
	{
		if (!InVarName || !OutValue) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(InObj, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*OutValue = objPtr->GetInt(std::string(InVarName));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetVariableInt(object InObj, const char* InVarName, int InValue)
	{
		if (!InVarName) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(InObj, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		objPtr->SetInt(std::string(InVarName), InValue);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetVariableFloat(object InObj, const char* InVarName, float* OutValue)
	{
		if (!InVarName || !OutValue) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(InObj, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*OutValue = objPtr->GetFloat(std::string(InVarName));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetVariableFloat(object InObj, const char* InVarName, float InValue)
	{
		if (!InVarName) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(InObj, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		objPtr->SetFloat(std::string(InVarName), InValue);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetVariableBool(object InObj, const char* InVarName, bool* OutValue)
	{
		if (!InVarName || !OutValue) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(InObj, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*OutValue = objPtr->GetBool(std::string(InVarName));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetVariableBool(object InObj, const char* InVarName, bool InValue)
	{
		if (!InVarName) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractObject* objPtr = nullptr;
		if (!ValidateHandle<AbstractObject>(InObj, &objPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		objPtr->SetBool(std::string(InVarName), InValue);
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

// component Functions
extern "C" {
	// Camera component
	PIXEON_API APIResult GetCameraTransform(component camera, CameraTransform* outTransform)
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
	PIXEON_API APIResult SetCameraTransform(component camera, const CameraTransform* inTransform)
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
	PIXEON_API APIResult GetCameraFov(component camera, float* outFov)
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
	PIXEON_API APIResult SetCameraFov(component camera, float inFov)
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
	PIXEON_API APIResult GetCameraAspect(component camera, float* outAspect)
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
	PIXEON_API APIResult SetCameraAspect(component camera, float inAspect)
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
	PIXEON_API APIResult GetCameraNearFar(component camera, float* outNear, float* outFar)
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
	PIXEON_API APIResult SetCameraNearFar(component camera, float inNear, float inFar)
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
	PIXEON_API APIResult SetChangeCameraCalculation(component camera, bool isChange)
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
	PIXEON_API APIResult GetCameraNumberAPI(component camera, int* outCameraNumber)
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
	PIXEON_API APIResult GetCameraUpVector(component camera, Float3* outUp)
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
	PIXEON_API APIResult GetCameraRightVector(component camera, Float3* outRight)
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
	PIXEON_API APIResult GetCameraForwardVector(component camera, Float3* outForward)
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

	// Light component
	PIXEON_API APIResult GetLightType(component light, int* outType)
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
	PIXEON_API APIResult SetLightType(component light, int inType)
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
	PIXEON_API APIResult GetLightColor(component light, Float3* outColor)
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
	PIXEON_API APIResult SetLightColor(component light, const Float3* inColor)
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
	PIXEON_API APIResult GetLightIntensity(component light, float* outIntensity)
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
	PIXEON_API APIResult SetLightIntensity(component light, float inIntensity)
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
	PIXEON_API APIResult GetLightRange(component light, float* outRange)
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
	PIXEON_API APIResult SetLightRange(component light, float inRange)
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
	PIXEON_API APIResult GetLightSpotInnerOuter(component light, float* outInnerDeg, float* outOuterDeg)
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
	PIXEON_API APIResult SetLightSpotInnerOuter(component light, float inInnerDeg, float inOuterDeg)
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
	PIXEON_API APIResult GetLightEnabled(component light, bool* outEnabled)
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
	PIXEON_API APIResult SetLightEnabled(component light, bool inEnabled)
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

	// ImageRender component
	PIXEON_API APIResult GetImageRenderTextureName(component imageRender, char* outName, int bufferSize)
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
	PIXEON_API APIResult SetImageRenderTextureName(component imageRender, const char* name)
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
	PIXEON_API APIResult GetImageTransform(component imageRender, transform* outTransform)
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
	PIXEON_API APIResult SetImageTransform(component imageRender, const transform* inTransform)
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
	PIXEON_API APIResult GetImageRenderColor(component imageRender, Float4* outColor)
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
	PIXEON_API APIResult SetImageRenderColor(component imageRender, const Float4* inColor)
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
	PIXEON_API APIResult GetImageRenderUVRect(component imageRender, Float4* outUVRect)
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
	PIXEON_API APIResult SetImageRenderUVRect(component imageRender, const Float4* inUVRect)
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

	// Model component
	PIXEON_API APIResult GetModelColor(component modelRender, Float4* outColor)
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
	PIXEON_API APIResult SetModelColor(component modelRender, const Float4* inColor)
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
	PIXEON_API APIResult SetMaterialTexture(component modelRender, int materialIndex, const char* texLogicalPath)
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
	PIXEON_API APIResult GetOffsetPosition(component modelRender, Float3* outPosition)
	{
		if (!outPosition) {
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
		*outPosition = ToFloat3(modelRenderComp->GetGlobalOffset());
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetOffsetPosition(component modelRender, const Float3* inPosition)
	{
		if (!inPosition) {
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
		modelRenderComp->SetGlobalOffset(ToXMFloat3(*inPosition));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetOffsetRotation(component modelRender, Float3* outRotation)
	{
		if (!outRotation) {
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
		*outRotation = ToFloat3(modelRenderComp->GetGlobalRotation());
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetOffsetRotation(component modelRender, const Float3* inRotation)
	{
		if (!inRotation) {
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
		modelRenderComp->SetGlobalRotation(ToXMFloat3(*inRotation));
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetOffsetScale(component modelRender, Float3* outScale)
	{
		if (!outScale) {
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
		*outScale = ToFloat3(modelRenderComp->GetGlobalScale());
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetOffsetScale(component modelRender, const Float3* inScale)
	{
		if (!inScale) {
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
		modelRenderComp->SetGlobalScale(ToXMFloat3(*inScale));
		return PN_SUCCESS;
	}

	PIXEON_API APIResult GetBoneName(component modelRender, int boneIndex, char* outName, int bufferSize)
	{
		if (!outName || bufferSize <= 0) {
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
		std::string boneName = modelRenderComp->GetBoneNameByIndex(boneIndex);
		int outLength = 0;
		return SafeStringCopy(boneName, outName, bufferSize, &outLength);
	}
	PIXEON_API APIResult GetBoneWorldPosition(component modelRender, int boneIndex, Float3* outPosition)
	{
		if (!outPosition) {
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
		DirectX::XMFLOAT3 pos;
		bool result = false;
		pos = modelRenderComp->GetBoneWorldPosition(boneIndex);
		if (!result) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		*outPosition = ToFloat3(pos);
		return PN_SUCCESS;
	}
	PIXEON_API APIResult GetBoneWorldRotation(component modelRender, int boneIndex, Float3* outRotation)
	{
		if (!outRotation) {
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
		DirectX::XMFLOAT3 rot;
		bool result = false;
		rot = modelRenderComp->GetBoneWorldRotationDegrees(boneIndex);
		if (!result) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		*outRotation = ToFloat3(rot);
		return PN_SUCCESS;
	}

	// Animation component
	PIXEON_API APIResult PlayAnimation(component animationComp)
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
	PIXEON_API APIResult PauseAnimation(component animationComp)
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
	PIXEON_API APIResult ResumeAnimation(component animationComp)
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
	PIXEON_API APIResult StopAnimation(component animationComp)
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
	PIXEON_API APIResult RestartAnimation(component animationComp)
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
	PIXEON_API APIResult SetAnimationClip(component animationComp, int clipIndex)
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
	PIXEON_API APIResult GetAnimationClip(component animationComp, int* outClipIndex)
	{
		if (!outClipIndex) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(animationComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		AnimationComponent* animComp = dynamic_cast<AnimationComponent*>(compPtr);
		if (!animComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outClipIndex = animComp->GetAnimationClip();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult SetAnimationPlaybackSpeed(component animationComp, float speed)
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
	PIXEON_API APIResult SetAnimationLoop(component animationComp, bool loop)
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

	// RigidBody component
	PIXEON_API APIResult RigidBodyAddForce(component rigidBodyComp, const Float3* inForce)
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
	PIXEON_API APIResult RigidBodyAddImpulse(component rigidBodyComp, const Float3* inImpulse)
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
	PIXEON_API APIResult RigidBodyGetVelocity(component rigidBodyComp, Float3* outVelocity)
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
	PIXEON_API APIResult RigidBodySetVelocity(component rigidBodyComp, const Float3* inVelocity)
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
	PIXEON_API APIResult RigidBodySetKinematic(component rigidBodyComp, bool isKinematic)
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
	PIXEON_API APIResult RigidBodyGetKinematic(component rigidBodyComp, bool* outIsKinematic)
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
	PIXEON_API APIResult RigidBodySetMass(component rigidBodyComp, float mass)
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
	PIXEON_API APIResult RigidBodyGetMass(component rigidBodyComp, float* outMass)
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
	PIXEON_API APIResult RigidBodySetUseGravity(component rigidBodyComp, bool useGravity)
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
	PIXEON_API APIResult RigidBodyGetUseGravity(component rigidBodyComp, bool* outUseGravity)
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
	PIXEON_API APIResult RigidBodySetFriction(component rigidBodyComp, float friction)
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
	PIXEON_API APIResult RigidBodyGetFriction(component rigidBodyComp, float* outFriction)
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
	PIXEON_API APIResult RigidBodySetRestitution(component rigidBodyComp, float restitution)
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
	PIXEON_API APIResult RigidBodyGetRestitution(component rigidBodyComp, float* outRestitution)
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
	PIXEON_API APIResult RigidBodySetLinearDamping(component rigidBodyComp, float damping)
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
	PIXEON_API APIResult RigidBodyGetLinearDamping(component rigidBodyComp, float* outDamping)
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
	PIXEON_API APIResult RigidBodySetAngularDamping(component rigidBodyComp, float damping)
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
	PIXEON_API APIResult RigidBodyGetAngularDamping(component rigidBodyComp, float* outDamping)
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
	PIXEON_API APIResult RigidBodySetDamping(component rigidBodyComp, float linearDamping, float angularDamping)
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
	PIXEON_API APIResult RigidBodySetRollingFriction(component rigidBodyComp, float rollingFriction)
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
	PIXEON_API APIResult RigidBodyGetRollingFriction(component rigidBodyComp, float* outRollingFriction)
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
	PIXEON_API APIResult RigidBodySetSpinningFriction(component rigidBodyComp, float spinningFriction)
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
	PIXEON_API APIResult RigidBodyGetSpinningFriction(component rigidBodyComp, float* outSpinningFriction)
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
	// BoxCollision component
	PIXEON_API APIResult BoxCollisionSetSize(component component, Float3 size)
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
	PIXEON_API APIResult BoxCollisionGetSize(component component, Float3* outSize)
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
	PIXEON_API APIResult BoxCollisionSetCenter(component component, Float3 center)
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
	PIXEON_API APIResult BoxCollisionGetCenter(component component, Float3* outCenter)
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
	PIXEON_API APIResult BoxCollisionSetIsTrigger(component component, bool isTrigger)
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
	PIXEON_API APIResult BoxCollisionGetIsTrigger(component component, bool* outIsTrigger)
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
	PIXEON_API APIResult CollisionSetCollisionEnterCallback(component component, CollisionEnterCallback callback)
	{
		BaseCollision* box;
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
	PIXEON_API APIResult CollisionSetCollisionStayCallback(component component, CollisionStayCallback callback)
	{
		BaseCollision* box;
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
	PIXEON_API APIResult CollisionSetCollisionExitCallback(component component, CollisionExitCallback callback)
	{
		BaseCollision* box;
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

	// animator2d component
// Animator2D Component
	PIXEON_API APIResult GetAnimator2D(component animatorComp, const char* animatorName, animator2d* outHandel)
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
		*outHandel = reinterpret_cast<animator2d>(animator);  // Animator2D* を animator2d (void*) にキャスト
		return PN_SUCCESS;
	}

	PIXEON_API APIResult Animator2DPlay(animator2d animator)
	{
		Animator2D* animatorPtr = reinterpret_cast<Animator2D*>(animator);  // animator2d (void*) を Animator2D* にキャスト
		if (!animatorPtr) {
			return PN_ERROR_INVALID_HANDLE;
		}
		animatorPtr->Start();
		return PN_SUCCESS;
	}

	PIXEON_API APIResult Animator2DStop(animator2d animator)
	{
		Animator2D* animatorPtr = reinterpret_cast<Animator2D*>(animator);
		if (!animatorPtr) {
			return PN_ERROR_INVALID_HANDLE;
		}
		animatorPtr->Stop();
		return PN_SUCCESS;
	}

	PIXEON_API APIResult Animator2DIsEnd(animator2d animator, bool* End)
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

	PIXEON_API APIResult FindKeyFrame(animator2d animator, const char* keyname, keyframe* outKeyframe)
	{
		if (!keyname || !outKeyframe) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		Animator2D* animatorPtr = reinterpret_cast<Animator2D*>(animator);
		if (!animatorPtr) {
			return PN_ERROR_INVALID_HANDLE;
		}
		KeyFrame* keyframePtr = animatorPtr->GetKeyFrameByName(std::string(keyname));
		if (!keyframePtr) {
			return PN_ERROR_NOT_FOUND;
		}
		*outKeyframe = reinterpret_cast<keyframe>(keyframePtr);  // KeyFrame* を keyframe (void*) にキャスト
		return PN_SUCCESS;
	}

	PIXEON_API APIResult SetVertexOffsetUp(keyframe Keyframe, float offset)
	{
		if (!Keyframe) {
			return PN_ERROR_INVALID_HANDLE;
		}
		KeyFrame* keyframePtr = reinterpret_cast<KeyFrame*>(Keyframe);  // keyframe (void*) を KeyFrame* にキャスト
		keyframePtr->vertexOffset.Up = offset;
		return PN_SUCCESS;
	}

	PIXEON_API APIResult SetVertexOffsetDown(keyframe Keyframe, float offset)
	{
		if (!Keyframe) {
			return PN_ERROR_INVALID_HANDLE;
		}
		KeyFrame* keyframePtr = reinterpret_cast<KeyFrame*>(Keyframe);
		keyframePtr->vertexOffset.Down = offset;
		return PN_SUCCESS;
	}

	PIXEON_API APIResult SetVertexOffsetLeft(keyframe Keyframe, float offset)
	{
		if (!Keyframe) {
			return PN_ERROR_INVALID_HANDLE;
		}
		KeyFrame* keyframePtr = reinterpret_cast<KeyFrame*>(Keyframe);
		keyframePtr->vertexOffset.Left = offset;
		return PN_SUCCESS;
	}

	PIXEON_API APIResult SetVertexOffsetRight(keyframe Keyframe, float offset)
	{
		if (!Keyframe) {
			return PN_ERROR_INVALID_HANDLE;
		}
		KeyFrame* keyframePtr = reinterpret_cast<KeyFrame*>(Keyframe);
		keyframePtr->vertexOffset.Right = offset;
		return PN_SUCCESS;
	}

	PIXEON_API APIResult GetVertexOffsetUp(keyframe Keyframe, float* outOffset)
	{
		if (!Keyframe || !outOffset) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		KeyFrame* keyframePtr = reinterpret_cast<KeyFrame*>(Keyframe);
		*outOffset = keyframePtr->vertexOffset.Up;
		return PN_SUCCESS;
	}

	PIXEON_API APIResult GetVertexOffsetDown(keyframe Keyframe, float* outOffset)
	{
		if (!Keyframe || !outOffset) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		KeyFrame* keyframePtr = reinterpret_cast<KeyFrame*>(Keyframe);
		*outOffset = keyframePtr->vertexOffset.Down;
		return PN_SUCCESS;
	}

	PIXEON_API APIResult GetVertexOffsetLeft(keyframe Keyframe, float* outOffset)
	{
		if (!Keyframe || !outOffset) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		KeyFrame* keyframePtr = reinterpret_cast<KeyFrame*>(Keyframe);
		*outOffset = keyframePtr->vertexOffset.Left;
		return PN_SUCCESS;
	}

	PIXEON_API APIResult GetVertexOffsetRight(keyframe Keyframe, float* outOffset)
	{
		if (!Keyframe || !outOffset) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		KeyFrame* keyframePtr = reinterpret_cast<KeyFrame*>(Keyframe);
		*outOffset = keyframePtr->vertexOffset.Right;
		return PN_SUCCESS;
	}
	PIXEON_API APIResult EffectPlay(component effectComp)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(effectComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		EffectComponent* effectComponent = dynamic_cast<EffectComponent*>(compPtr);
		if (!effectComponent) {
			return PN_ERROR_INVALID_HANDLE;
		}
		effectComponent->Play();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult EffectStop(component effectComp)
	{
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(effectComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		EffectComponent* effectComponent = dynamic_cast<EffectComponent*>(compPtr);
		if (!effectComponent) {
			return PN_ERROR_INVALID_HANDLE;
		}
		effectComponent->Stop();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult EffectIsPlaying(component effectComp, bool* outIsPlaying)
	{
		if (!outIsPlaying) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(effectComp, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		EffectComponent* effectComponent = dynamic_cast<EffectComponent*>(compPtr);
		if (!effectComponent) {
			return PN_ERROR_INVALID_HANDLE;
		}
		*outIsPlaying = effectComponent->IsPlaying();
		return PN_SUCCESS;
	}
	PIXEON_API APIResult CallScriptFunction(component Script, const char* functionName)
	{
		if (!functionName) {
			return PN_ERROR_INVALID_PARAMETER;
		}
		AbstractComponent* compPtr = nullptr;
		if (!ValidateHandle<AbstractComponent>(Script, &compPtr)) {
			return PN_ERROR_INVALID_HANDLE;
		}
		ScripComponent* scriptComp = dynamic_cast<ScripComponent*>(compPtr);
		if (!scriptComp) {
			return PN_ERROR_INVALID_HANDLE;
		}
		scriptComp->CallFunction(std::string(functionName));
		return PN_SUCCESS;
	}
};