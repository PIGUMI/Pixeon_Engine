// CollisionManager.h
// 衝突検出とコールバック管理を行うマネージャー
#ifndef _COLLISION_MANAGER_H_
#define _COLLISION_MANAGER_H_

#include "BulletPhysics/btBulletDynamicsCommon.h"
#include <DirectXMath.h>
#include <vector>
#include <unordered_map>
#include <functional>

class BaseCollision;
class BoxCollision;
class CapsuleCollision;
class RigidBody;
class AbstractObject;
struct CollisionInfo;

// Bulletの衝突コールバック用
class CollisionContactCallback : public btCollisionWorld::ContactResultCallback
{
public:
	CollisionContactCallback(BaseCollision* owner);

	btScalar addSingleResult(btManifoldPoint& cp,
		const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0,
		const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1) override;

private:
	BaseCollision* m_Owner;
	std::vector<CollisionInfo> m_Collisions;

	friend class CollisionManager;
};

class CollisionManager
{
public:
	CollisionManager() = default;
	~CollisionManager() = default;

	void Initialize(btDiscreteDynamicsWorld* dynamicsWorld);
	void Update();
	void Shutdown();

	// コンポーネント登録/削除
	void RegisterBoxCollision(BoxCollision* collision);
	void UnregisterBoxCollision(BoxCollision* collision);
	void RegisterCapsuleCollision(CapsuleCollision* collision);
	void UnregisterCapsuleCollision(CapsuleCollision* collision);

	void RegisterRigidBody(RigidBody* rigidBody);
	void UnregisterRigidBody(RigidBody* rigidBody);

	void CheckManualCollisions();
	void ProcessBulletCollisions();
	void DrawDebugInfo();

private:
	bool CheckBoxCapsuleCollision(BoxCollision* box, CapsuleCollision* capsule, CollisionInfo& info);
	bool CheckCapsuleCapsuleCollision(CapsuleCollision* capsule1, CapsuleCollision* capsule2, CollisionInfo& info);

	DirectX::XMFLOAT3 ClosestPointOnLineSegmentToAABB(
		const DirectX::XMFLOAT3& lineStart,
		const DirectX::XMFLOAT3& lineEnd,
		const DirectX::XMFLOAT3& boxMin,
		const DirectX::XMFLOAT3& boxMax);

	float ClosestPointsBetweenLineSegments(
		const DirectX::XMVECTOR& p1, const DirectX::XMVECTOR& q1,
		const DirectX::XMVECTOR& p2, const DirectX::XMVECTOR& q2,
		DirectX::XMFLOAT3& point1, DirectX::XMFLOAT3& point2);

	void ProcessCollisionEvents(BaseCollision* collision,
		const std::vector<CollisionInfo>& newCollisions);

private:
	btDiscreteDynamicsWorld* m_DynamicsWorld = nullptr;

	std::vector<BoxCollision*> m_BoxCollisions;
	std::vector<CapsuleCollision*> m_CapsuleCollisions;
	std::vector<RigidBody*> m_RigidBodies;

	// 衝突状態追跡用 - BaseCollisionを使用
	std::unordered_map<BaseCollision*, std::vector<BaseCollision*>> m_PreviousCollisions;
	std::unordered_map<BaseCollision*, std::vector<CollisionInfo>> m_CurrentCollisions;

	// Bulletコールバック用
	std::unordered_map<BaseCollision*, CollisionContactCallback*> m_ContactCallbacks;
};

#endif // _COLLISION_MANAGER_H_