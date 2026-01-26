#pragma once
#include "Include/IScript.h"

class Script_Weapon : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;

private:
    void Animation();

    void Fire();
    Float3 CalculateForwardVector(Float3 rotation);
    Float3 CalculateSpreadDirection(Float3 baseDirection, float angleX, float angleY);
    float RandomRange(float min, float max);
    void ApplyDamageAndKnockback(Object target, Float3 direction, float damage);
    void CreateBulletTrailEffect(Float3 start, Float3 end);
    void CreateImpactEffect(Float3 position, Float3 normal);
    void CreateMuzzleFlash();

private:
    // トランスフォーム
    transform _idleTransform;
    transform _ReadyTransform;

	Float3 _idlePosition;
	Float3 _idleRotation;
	Float3 _readyPosition;
	Float3 _readyRotation;

    float _transitionProgress;
    bool _isAiming;

    // リコイル
    transform _recoilOffset;
    float _recoilProgress;
    float _recoilRecoverySpeed;

    // クールタイム
    float _coolTime;
    float _fireRate;

    // ショットガンパラメータ
    int _pelletCount;
    float _spreadAngle;
    float _maxRange;
    float _damagePerPellet;
    float _knockbackForce;

    // エフェクト
    Object _ExplosionEffect;
    Object _Body;
    Object _MuzzleFlash;
    Object _BulletTrailPrefab;
    Object _ImpactEffectPrefab;

    // カメラ・シーン
    Component _cameraComponent;
	Component _ModelComponent;
    Object _playerObject;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Weapon();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}