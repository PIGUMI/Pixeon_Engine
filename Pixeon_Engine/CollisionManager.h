// CollisionManager.h
// 衝突検出とコールバック管理を行うマネージャー
#ifndef _COLLISION_MANAGER_H_
#define _COLLISION_MANAGER_H_

#include "BulletPhysics/btBulletDynamicsCommon.h"
#include <vector>
#include <unordered_map>
#include <functional>

class BoxCollision;
class RigidBody;
class Object;
struct CollisionInfo;

// Bulletの衝突コールバック処理用
class CollisionContactCallback : public btCollisionWorld::ContactResultCallback
{
public:
	CollisionContactCallback(BoxCollision* owner);

	btScalar addSingleResult(btManifoldPoint& cp,
		const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0,
		const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1) override;

private:
	BoxCollision* m_Owner;
	std::vector<CollisionInfo> m_Collisions;

	friend class CollisionManager;
};

class CollisionManager
{
public:
	// Singletonパターンを削除し、通常のクラスに変更
	CollisionManager() = default;
	~CollisionManager() = default;

	void Initialize(btDiscreteDynamicsWorld* dynamicsWorld);
	void Update();
	void Shutdown();

	// コンポーネント登録/削除
	void RegisterBoxCollision(BoxCollision* collision);
	void UnregisterBoxCollision(BoxCollision* collision);

	void RegisterRigidBody(RigidBody* rigidBody);
	void UnregisterRigidBody(RigidBody* rigidBody);

	// 手動衝突検出（RigidBodyなしの場合）
	void CheckManualCollisions();

	// Bulletの衝突検出結果を処理
	void ProcessBulletCollisions();

	// デバッグ描画
	void DrawDebugInfo();

private:
	btDiscreteDynamicsWorld* m_DynamicsWorld = nullptr;

	std::vector<BoxCollision*> m_BoxCollisions;
	std::vector<RigidBody*> m_RigidBodies;

	// 衝突状態追跡用
	std::unordered_map<BoxCollision*, std::vector<BoxCollision*>> m_PreviousCollisions;
	std::unordered_map<BoxCollision*, std::vector<CollisionInfo>> m_CurrentCollisions;

	// Bulletコールバック用
	std::unordered_map<BoxCollision*, CollisionContactCallback*> m_ContactCallbacks;

	void ProcessCollisionEvents(BoxCollision* collision,
		const std::vector<CollisionInfo>& newCollisions);
};

#endif // _COLLISION_MANAGER_H_