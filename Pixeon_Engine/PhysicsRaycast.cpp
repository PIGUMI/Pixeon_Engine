#include "PhysicsRaycast.h"
#include "Scene.h"
#include "RigidBody.h"
#include "BaseCollision.h"
#include "Object.h"

DirectX::XMFLOAT3 PhysicsRaycast::Normalize(const DirectX::XMFLOAT3& vec) {
    float length = sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
    if (length > 0.0f) {
        return DirectX::XMFLOAT3(vec.x / length, vec.y / length, vec.z / length);
    }
    return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
}

bool PhysicsRaycast::Raycast(
    Scene* scene,
    const DirectX::XMFLOAT3& origin,
    const DirectX::XMFLOAT3& direction,
    float maxDistance,
    RaycastHit& outHit)
{
    if (!scene) return false;

    btDiscreteDynamicsWorld* physicsWorld = scene->GetPhysicsWorld();
    if (!physicsWorld) return false;

    // 方向ベクトルを正規化
    DirectX::XMFLOAT3 normalizedDir = Normalize(direction);

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
        outHit.bHit = true;

        // ヒット位置
        outHit.f3Point = DirectX::XMFLOAT3(
            rayCallback.m_hitPointWorld.getX(),
            rayCallback.m_hitPointWorld.getY(),
            rayCallback.m_hitPointWorld.getZ()
        );

        // 法線
        outHit.f3Normal = DirectX::XMFLOAT3(
            rayCallback.m_hitNormalWorld.getX(),
            rayCallback.m_hitNormalWorld.getY(),
            rayCallback.m_hitNormalWorld.getZ()
        );

        // 距離
        DirectX::XMFLOAT3 diff = {
            outHit.f3Point.x - origin.x,
            outHit.f3Point.y - origin.y,
            outHit.f3Point.z - origin.z
        };
        outHit.fDistance = sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

        // ヒットしたオブジェクトを取得
        const btCollisionObject* collisionObject = rayCallback.m_collisionObject;
        if (collisionObject) {
            const btRigidBody* rigidBody = btRigidBody::upcast(collisionObject);
            if (rigidBody && rigidBody->getUserPointer()) {
                RigidBody* rbComponent = static_cast<RigidBody*>(rigidBody->getUserPointer());
                if (rbComponent) {
                    outHit.pHitObject = rbComponent->GetParent();
                    // コリジョンコンポーネントも取得可能であれば設定
                    // outHit.pHitCollision = ... ;
                }
            }
        }

        return true;
    }

    outHit.bHit = false;
    return false;
}

bool PhysicsRaycast::RaycastAll(
    Scene* scene,
    const DirectX::XMFLOAT3& origin,
    const DirectX::XMFLOAT3& direction,
    float maxDistance,
    std::vector<RaycastHit>& outHits)
{
    if (!scene) return false;

    btDiscreteDynamicsWorld* physicsWorld = scene->GetPhysicsWorld();
    if (!physicsWorld) return false;

    DirectX::XMFLOAT3 normalizedDir = Normalize(direction);

    btVector3 rayFrom(origin.x, origin.y, origin.z);
    btVector3 rayTo(
        origin.x + normalizedDir.x * maxDistance,
        origin.y + normalizedDir.y * maxDistance,
        origin.z + normalizedDir.z * maxDistance
    );

    // AllHitsRayResultCallbackを使用してすべてのヒットを取得
    btCollisionWorld::AllHitsRayResultCallback rayCallback(rayFrom, rayTo);
    physicsWorld->rayTest(rayFrom, rayTo, rayCallback);

    outHits.clear();

    if (rayCallback.hasHit()) {
        int numHits = rayCallback.m_collisionObjects.size();

        for (int i = 0; i < numHits; ++i) {
            RaycastHit hit;
            hit.bHit = true;

            // ヒット位置
            hit.f3Point = DirectX::XMFLOAT3(
                rayCallback.m_hitPointWorld[i].getX(),
                rayCallback.m_hitPointWorld[i].getY(),
                rayCallback.m_hitPointWorld[i].getZ()
            );

            // 法線
            hit.f3Normal = DirectX::XMFLOAT3(
                rayCallback.m_hitNormalWorld[i].getX(),
                rayCallback.m_hitNormalWorld[i].getY(),
                rayCallback.m_hitNormalWorld[i].getZ()
            );

            // 距離
            DirectX::XMFLOAT3 diff = {
                hit.f3Point.x - origin.x,
                hit.f3Point.y - origin.y,
                hit.f3Point.z - origin.z
            };
            hit.fDistance = sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

            // ヒットしたオブジェクトを取得
            const btCollisionObject* collisionObject = rayCallback.m_collisionObjects[i];
            if (collisionObject) {
                const btRigidBody* rigidBody = btRigidBody::upcast(collisionObject);
                if (rigidBody && rigidBody->getUserPointer()) {
                    RigidBody* rbComponent = static_cast<RigidBody*>(rigidBody->getUserPointer());
                    if (rbComponent) {
                        hit.pHitObject = rbComponent->GetParent();
                    }
                }
            }

            outHits.push_back(hit);
        }

        return true;
    }

    return false;
}

bool PhysicsRaycast::RaycastIgnoreTriggers(
    Scene* scene,
    const DirectX::XMFLOAT3& origin,
    const DirectX::XMFLOAT3& direction,
    float maxDistance,
    RaycastHit& outHit)
{
    if (!scene) return false;

    btDiscreteDynamicsWorld* physicsWorld = scene->GetPhysicsWorld();
    if (!physicsWorld) return false;

    DirectX::XMFLOAT3 normalizedDir = Normalize(direction);

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
        outHit.bHit = true;

        outHit.f3Point = DirectX::XMFLOAT3(
            rayCallback.m_hitPointWorld.getX(),
            rayCallback.m_hitPointWorld.getY(),
            rayCallback.m_hitPointWorld.getZ()
        );

        outHit.f3Normal = DirectX::XMFLOAT3(
            rayCallback.m_hitNormalWorld.getX(),
            rayCallback.m_hitNormalWorld.getY(),
            rayCallback.m_hitNormalWorld.getZ()
        );

        DirectX::XMFLOAT3 diff = {
            outHit.f3Point.x - origin.x,
            outHit.f3Point.y - origin.y,
            outHit.f3Point.z - origin.z
        };
        outHit.fDistance = sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

        const btCollisionObject* collisionObject = rayCallback.m_collisionObject;
        if (collisionObject) {
            const btRigidBody* rigidBody = btRigidBody::upcast(collisionObject);
            if (rigidBody && rigidBody->getUserPointer()) {
                RigidBody* rbComponent = static_cast<RigidBody*>(rigidBody->getUserPointer());
                if (rbComponent) {
                    outHit.pHitObject = rbComponent->GetParent();
                }
            }
        }

        return true;
    }

    outHit.bHit = false;
    return false;
}

bool PhysicsRaycast::SphereCast(
    Scene* scene,
    const DirectX::XMFLOAT3& origin,
    const DirectX::XMFLOAT3& direction,
    float radius,
    float maxDistance,
    RaycastHit& outHit)
{
    if (!scene) return false;

    btDiscreteDynamicsWorld* physicsWorld = scene->GetPhysicsWorld();
    if (!physicsWorld) return false;

    DirectX::XMFLOAT3 normalizedDir = Normalize(direction);

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
        outHit.bHit = true;

        outHit.f3Point = DirectX::XMFLOAT3(
            convexCallback.m_hitPointWorld.getX(),
            convexCallback.m_hitPointWorld.getY(),
            convexCallback.m_hitPointWorld.getZ()
        );

        outHit.f3Normal = DirectX::XMFLOAT3(
            convexCallback.m_hitNormalWorld.getX(),
            convexCallback.m_hitNormalWorld.getY(),
            convexCallback.m_hitNormalWorld.getZ()
        );

        DirectX::XMFLOAT3 diff = {
            outHit.f3Point.x - origin.x,
            outHit.f3Point.y - origin.y,
            outHit.f3Point.z - origin.z
        };
        outHit.fDistance = sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

        const btCollisionObject* collisionObject = convexCallback.m_hitCollisionObject;
        if (collisionObject) {
            const btRigidBody* rigidBody = btRigidBody::upcast(collisionObject);
            if (rigidBody && rigidBody->getUserPointer()) {
                RigidBody* rbComponent = static_cast<RigidBody*>(rigidBody->getUserPointer());
                if (rbComponent) {
                    outHit.pHitObject = rbComponent->GetParent();
                }
            }
        }

        return true;
    }

    outHit.bHit = false;
    return false;
}