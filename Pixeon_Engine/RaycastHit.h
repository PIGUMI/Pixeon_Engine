#ifndef _RAYCAST_HIT_H_
#define _RAYCAST_HIT_H_

#include <DirectXMath.h>

class AbstractObject;
class BaseCollision;

struct RaycastHit {
    bool bHit = false;
    DirectX::XMFLOAT3 f3Point = { 0,0,0 };
    DirectX::XMFLOAT3 f3Normal = { 0,0,0 };
    float fDistance = 0.0f;
    AbstractObject* pHitObject = nullptr;
    BaseCollision* pHitCollision = nullptr;
};

#endif // _RAYCAST_HIT_H_