#ifndef _PHYSICS_RAYCAST_H_
#define _PHYSICS_RAYCAST_H_

#include "BulletPhysics/btBulletDynamicsCommon.h"
#include "RaycastHit.h"
#include <DirectXMath.h>
#include <vector>

class Scene;

class PhysicsRaycast {
public:
    // シングルレイキャスト（最初にヒットしたものだけ）
    static bool Raycast(
        Scene* scene,
        const DirectX::XMFLOAT3& origin,
        const DirectX::XMFLOAT3& direction,
        float maxDistance,
        RaycastHit& outHit
    );

    // レイキャストオール（すべてのヒットを取得）
    static bool RaycastAll(
        Scene* scene,
        const DirectX::XMFLOAT3& origin,
        const DirectX::XMFLOAT3& direction,
        float maxDistance,
        std::vector<RaycastHit>& outHits
    );

    // レイキャスト（トリガーを無視）
    static bool RaycastIgnoreTriggers(
        Scene* scene,
        const DirectX::XMFLOAT3& origin,
        const DirectX::XMFLOAT3& direction,
        float maxDistance,
        RaycastHit& outHit
    );

    // 球体キャスト（レイではなく球体を飛ばす）
    static bool SphereCast(
        Scene* scene,
        const DirectX::XMFLOAT3& origin,
        const DirectX::XMFLOAT3& direction,
        float radius,
        float maxDistance,
        RaycastHit& outHit
    );

private:
    static DirectX::XMFLOAT3 Normalize(const DirectX::XMFLOAT3& vec);
};

#endif // _PHYSICS_RAYCAST_H_