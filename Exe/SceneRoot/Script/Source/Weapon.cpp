#include "Weapon.h"
#include <Windows.h>
#include <DirectXMath.h>
#include <cmath>
#include <cstdlib>
#include <ctime>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void Script_Weapon::BeginPlay() {
    IScript::BeginPlay();

    GetObjectTransform(_parentObject, &_idleTransform);

    // 構えトランスフォーム設定
    {
        _ReadyTransform.position.x = -0.3f;
        _ReadyTransform.position.y = 0.9f;
        _ReadyTransform.position.z = 0.0f;
        _ReadyTransform.rotation.x = DirectX::XMConvertToRadians(16.0f);
        _ReadyTransform.rotation.y = DirectX::XMConvertToRadians(90.0f);
        _ReadyTransform.rotation.z = DirectX::XMConvertToRadians(9.5f);
        _ReadyTransform.scale.x = _ReadyTransform.scale.y = _ReadyTransform.scale.z = 0.5f;

        _transitionProgress = 0.0f;
        _isAiming = false;
    }

    // 射撃パラメータ初期化
    {
        _coolTime = 0.0f;
        _fireRate = 0.8f;

        _pelletCount = 4;
        _spreadAngle = 5.0f;
        _maxRange = 25.0f;
        _damagePerPellet = 12.0f;
        _knockbackForce = 10.0f;
    }

    // 乱数初期化
    srand(static_cast<unsigned int>(time(nullptr)));

    // エフェクトプレハブを取得
    FindPrefabObjectByName("Expl", &_ExplosionEffect);
    _BulletTrailPrefab = nullptr;
    _ImpactEffectPrefab = nullptr;
    _MuzzleFlash = nullptr;

    // Playerオブジェクトを取得
    if (_parentScene != nullptr)
    {
        if (FindObjectByName(_parentScene, "Player", &_playerObject) != PN_SUCCESS)
        {
            _playerObject = nullptr;
            MessageBox(nullptr, "Playerオブジェクトが見つかりません。", "Error", MB_OK);
        }
        if (FindChildObjectByName(_playerObject, "Body", &_Body) != PN_SUCCESS)
        {
            _Body = nullptr;
            MessageBox(nullptr, "WeaponのBodyオブジェクトが見つかりません。", "Error", MB_OK);
        }
        if (_Body != nullptr)
        {
            FindComponent(_Body, "CameraComponent", &_cameraComponent);
        }
    }
}

void Script_Weapon::Update(float DeltaTime) {
    IScript::Update(DeltaTime);

    if (_coolTime > 0.0f)
    {
        _coolTime -= DeltaTime;
    }

    // エイム処理
    float transitionSpeed = 10.0f * DeltaTime;
    if (KeyPressed(VK_RBUTTON))
    {
        _isAiming = true;
        _transitionProgress += transitionSpeed;
        if (_transitionProgress > 1.0f) _transitionProgress = 1.0f;
    }
    else
    {
        _isAiming = false;
        _transitionProgress -= transitionSpeed;
        if (_transitionProgress < 0.0f) _transitionProgress = 0.0f;
    }

    if (KeyTriggered(VK_LBUTTON) && _coolTime <= 0.0f)
    {
        Fire();
        _coolTime = _fireRate;
    }

    Animation();
}

void Script_Weapon::EndPlay() {
    IScript::EndPlay();
}

void Script_Weapon::Animation() {
    transform current_Transform;
    float t = _transitionProgress;

    current_Transform.position.x = _idleTransform.position.x + (_ReadyTransform.position.x - _idleTransform.position.x) * t;
    current_Transform.position.y = _idleTransform.position.y + (_ReadyTransform.position.y - _idleTransform.position.y) * t;
    current_Transform.position.z = _idleTransform.position.z + (_ReadyTransform.position.z - _idleTransform.position.z) * t;

    current_Transform.rotation.x = _idleTransform.rotation.x + (_ReadyTransform.rotation.x - _idleTransform.rotation.x) * t;
    current_Transform.rotation.y = _idleTransform.rotation.y + (_ReadyTransform.rotation.y - _idleTransform.rotation.y) * t;
    current_Transform.rotation.z = _idleTransform.rotation.z + (_ReadyTransform.rotation.z - _idleTransform.rotation.z) * t;

    current_Transform.scale.x = _idleTransform.scale.x + (_ReadyTransform.scale.x - _idleTransform.scale.x) * t;
    current_Transform.scale.y = _idleTransform.scale.y + (_ReadyTransform.scale.y - _idleTransform.scale.y) * t;
    current_Transform.scale.z = _idleTransform.scale.z + (_ReadyTransform.scale.z - _idleTransform.scale.z) * t;

    current_Transform.position.x += _recoilOffset.position.x;
    current_Transform.position.y += _recoilOffset.position.y;
    current_Transform.position.z += _recoilOffset.position.z;

    current_Transform.rotation.x += _recoilOffset.rotation.x;
    current_Transform.rotation.y += _recoilOffset.rotation.y;
    current_Transform.rotation.z += _recoilOffset.rotation.z;

    SetObjectTransform(_parentObject, &current_Transform);
}

void Script_Weapon::Fire() {
    if (!_cameraComponent || !_Body)
        return;

    transform bodyWorldTransform;
    GetObjectWorldTransform(_Body, &bodyWorldTransform);

    Float3 origin = bodyWorldTransform.position;
    Float3 forward = CalculateForwardVector(bodyWorldTransform.rotation);

    forward.x = -forward.x;
    forward.z = -forward.z;
    origin.y += 1.5f;

    CreateMuzzleFlash();

    for (int i = 0; i < _pelletCount; i++)
    {
        float randomX = RandomRange(-_spreadAngle, _spreadAngle);
        float randomY = RandomRange(-_spreadAngle, _spreadAngle);

        Float3 spreadDirection = CalculateSpreadDirection(forward, randomX, randomY);

        RayHit hit;
        APIResult result;

        if (_playerObject != nullptr)
        {
            result = RaycastIgnoreObject(
                _parentScene,
                origin,
                spreadDirection,
                _maxRange,
                _playerObject,
                &hit
            );
        }
        else
        {
            result = Raycast(
                _parentScene,
                origin,
                spreadDirection,
                _maxRange,
                &hit
            );
        }

        if (result == PN_SUCCESS && hit.bHit)
        {
            float distanceFactor = 1.0f - (hit.distance / _maxRange);
            distanceFactor = (distanceFactor < 0.3f) ? 0.3f : distanceFactor;
            float adjustedDamage = _damagePerPellet * distanceFactor;

            ApplyDamageAndKnockback(hit.hitObject, spreadDirection, adjustedDamage);

            CreateBulletTrailEffect(origin, hit.point);
            CreateImpactEffect(hit.point, hit.normal);
        }
        else
        {
            Float3 endPoint = CreateFloat3(
                origin.x + spreadDirection.x * _maxRange,
                origin.y + spreadDirection.y * _maxRange,
                origin.z + spreadDirection.z * _maxRange
            );
            CreateBulletTrailEffect(origin, endPoint);
        }
    }
}

Float3 Script_Weapon::CalculateForwardVector(Float3 rotation) {
    float pitch = rotation.x;
    float yaw = rotation.y;

    Float3 forward;
    forward.x = sinf(yaw) * cosf(pitch);
    forward.y = -sinf(pitch);
    forward.z = cosf(yaw) * cosf(pitch);

    float length = sqrtf(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    if (length > 0.0001f)
    {
        forward.x /= length;
        forward.y /= length;
        forward.z /= length;
    }

    return forward;
}

Float3 Script_Weapon::CalculateSpreadDirection(Float3 baseDirection, float angleX, float angleY) {
    float radX = angleX * static_cast<float>(M_PI) / 180.0f;
    float radY = angleY * static_cast<float>(M_PI) / 180.0f;

    float cosY = cosf(radY);
    float sinY = sinf(radY);
    Float3 rotatedY = CreateFloat3(
        baseDirection.x * cosY - baseDirection.z * sinY,
        baseDirection.y,
        baseDirection.x * sinY + baseDirection.z * cosY
    );

    float cosX = cosf(radX);
    float sinX = sinf(radX);
    Float3 result = CreateFloat3(
        rotatedY.x,
        rotatedY.y * cosX - rotatedY.z * sinX,
        rotatedY.y * sinX + rotatedY.z * cosX
    );

    float length = sqrtf(result.x * result.x + result.y * result.y + result.z * result.z);
    if (length > 0.0001f)
    {
        result.x /= length;
        result.y /= length;
        result.z /= length;
    }

    return result;
}

float Script_Weapon::RandomRange(float min, float max) {
    float random = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return min + random * (max - min);
}

void Script_Weapon::ApplyDamageAndKnockback(Object target, Float3 direction, float damage) {
    Component rigidBody;
    if (FindComponent(target, "RigidBody", &rigidBody) == PN_SUCCESS)
    {
        Float3 impulse = CreateFloat3(
            direction.x * _knockbackForce,
            direction.y * _knockbackForce,
            direction.z * _knockbackForce
        );
        RigidBodyAddImpulse(rigidBody, &impulse);
    }
}

void Script_Weapon::CreateBulletTrailEffect(Float3 start, Float3 end) {
    if (!_BulletTrailPrefab)
        return;

    Float3 midPoint = CreateFloat3(
        (start.x + end.x) / 2.0f,
        (start.y + end.y) / 2.0f,
        (start.z + end.z) / 2.0f
    );

    Object clonedTrail = nullptr;
    APIResult result = AddObjectToScene(_parentScene, _BulletTrailPrefab, &clonedTrail);

    if (result == PN_SUCCESS && clonedTrail != nullptr)
    {
        SetObjectPosition(clonedTrail, midPoint);
    }
}

void Script_Weapon::CreateImpactEffect(Float3 position, Float3 normal) {
    if (!_ExplosionEffect)
        return;

    // クローンされたオブジェクトを取得
    Object clonedEffect = nullptr;
    APIResult result = AddObjectToScene(_parentScene, _ExplosionEffect, &clonedEffect);

    if (result == PN_SUCCESS && clonedEffect != nullptr)
    {
        SetObjectPosition(clonedEffect, position);
    }
}

void Script_Weapon::CreateMuzzleFlash() {
    if (!_MuzzleFlash || !_Body)
        return;

    transform bodyTransform;
    GetObjectWorldTransform(_Body, &bodyTransform);

    Float3 forward = CalculateForwardVector(bodyTransform.rotation);

    Float3 muzzlePosition = CreateFloat3(
        bodyTransform.position.x + forward.x * 0.5f,
        bodyTransform.position.y + forward.y * 0.5f,
        bodyTransform.position.z + forward.z * 0.5f
    );

    Object clonedFlash = nullptr;
    APIResult result = AddObjectToScene(_parentScene, _MuzzleFlash, &clonedFlash);

    if (result == PN_SUCCESS && clonedFlash != nullptr)
    {
        SetObjectPosition(clonedFlash, muzzlePosition);
    }
}