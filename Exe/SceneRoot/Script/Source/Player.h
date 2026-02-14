#pragma once
#include "Include/IScript.h"

class Script_Player : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;
private:
    void MouseInput(float DeltaTime);
    void AimUpdate(float DeltaTime);
    void PlayerRotationUpdate(float DeltaTime);
    void PlayerMovement(float DeltaTime);
    void AnimationUpdate(float DeltaTime);
    void ShootUpdate(float DeltaTime);
    void ReticleUpdate(float DeltaTime);
    void CameraCollisionUpdate(float DeltaTime);
    void FireBullet();
    void CreateBulletTrail(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end);
    void CreateImpactEffect(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& normal);
    void ShowHitMarker(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& normal);

public:
    Object* CameraObject = nullptr;
    Camera* playerCamera = nullptr;
    Rigidbody* playerRigidbody = nullptr;
    Animation* playerAnimation = nullptr;

    Object* ReticleInPrefab = nullptr;
    Object* ReticleOutPrefab = nullptr;
    Object* ReticlePrefab = nullptr;

    Object* SpawnedReticleIn = nullptr;
    Object* SpawnedReticleOut = nullptr;
    Object* SpawnedReticle = nullptr;

    Object* BulletTrailPrefab = nullptr;
    Object* BulletImpactPrefab = nullptr;
    Object* HitMarkerPrefab = nullptr;
    Object* SpawnedHitMarker = nullptr;
    float hitMarkerTimer = 0.0f;
    float hitMarkerDuration = 0.2f;

    bool isAiming = false;
    bool wasAiming = false;
    bool isShooting = false;
    bool canShoot = true;
    float shootAnimationTimer = 0.0f;
    float currentRadius = 1.8f;
    DirectX::XMFLOAT3 currentCameraOffset;
    DirectX::XMFLOAT3 defaultCameraOffset;

    float cameraCollisionRadius = 0.3f;
    float cameraCollisionMinDistance = 0.5f;
    float cameraCollisionSmoothSpeed = 10.0f;
    float currentCameraDistance = 1.8f;
    bool enableCameraCollision = true;

    float defaultRadius = 1.8f;
    float aimRadius = 0.9f;
    DirectX::XMFLOAT3 aimCameraOffset = DirectX::XMFLOAT3(0.5f, 1.4f, 0.0f);
    float aimTransitionSpeed = 10.0f;

    float cameraYaw = 0.0f;
    float playerYaw = 0.0f;
    float playerRotationSpeed = 8.0f;
    bool isMoving = false;
    bool isRunning = false;

    float walkSpeed = 3.0f;
    float runSpeed = 6.0f;
    float aimWalkSpeed = 2.0f;
    DirectX::XMFLOAT3 moveDirection;

    float cameraVerticalAngleDegMin = -70.0f;
    float cameraVerticalAngleDegMax = 35.0f;
    float cameraSensitivity = 10.0f;
    float aimCameraSensitivity = 5.0f;

    float normalRotationThreshold = 5.0f;
    float aimRotationThreshold = 0.5f;
    float aimRotationSpeedMultiplier = 2.0f;

    int idleAnimationIndex = 0;
    int runAnimationIndex = 1;
    int aimAnimationIndex = 2;
    int currentAnimationIndex = -1;
    float shootAnimationSpeed = 1.5f;

    float bulletMaxRange = 100.0f;
    float bulletDamage = 25.0f;
    float bulletSpread = 0.01f;
    float hipFireSpread = 0.05f;

    float muzzleOffsetX = 0.3f;
    float muzzleOffsetY = 1.5f;
    float muzzleOffsetZ = 0.5f;
    std::string ReticleInName = "ReticleIn";
    std::string ReticleOutName = "ReticleOut";
    std::string ReticleName = "Reticle";
    std::string BulletTrailName = "BulletTrail";
    std::string BulletImpactName = "BulletImpact";
    std::string HitMarkerName = "HitMarker"; 

public:
#define PROPERTY_LIST(ACTION) \
    ACTION(FLOAT, walkSpeed) \
    ACTION(FLOAT, runSpeed) \
    ACTION(FLOAT, aimWalkSpeed) \
    ACTION(FLOAT, playerRotationSpeed) \
    ACTION(FLOAT, cameraSensitivity) \
    ACTION(FLOAT, aimCameraSensitivity) \
    ACTION(FLOAT, cameraVerticalAngleDegMin) \
    ACTION(FLOAT, cameraVerticalAngleDegMax) \
    ACTION(FLOAT, defaultRadius) \
    ACTION(FLOAT, aimRadius) \
    ACTION(FLOAT, aimTransitionSpeed) \
    ACTION(FLOAT, normalRotationThreshold) \
    ACTION(FLOAT, aimRotationThreshold) \
    ACTION(FLOAT, aimRotationSpeedMultiplier) \
    ACTION(FLOAT, shootAnimationSpeed) \
    ACTION(FLOAT, cameraCollisionRadius) \
    ACTION(FLOAT, cameraCollisionMinDistance) \
    ACTION(FLOAT, cameraCollisionSmoothSpeed) \
    ACTION(FLOAT, bulletMaxRange) \
    ACTION(FLOAT, bulletDamage) \
    ACTION(FLOAT, bulletSpread) \
    ACTION(FLOAT, hipFireSpread) \
    ACTION(FLOAT, hitMarkerDuration) \
    ACTION(FLOAT, muzzleOffsetX) \
    ACTION(FLOAT, muzzleOffsetY) \
    ACTION(FLOAT, muzzleOffsetZ) \
    ACTION(INT, idleAnimationIndex) \
    ACTION(INT, runAnimationIndex) \
    ACTION(INT, aimAnimationIndex) \
    ACTION(BOOL, enableCameraCollision) \
    ACTION(STRING, ReticleInName) \
    ACTION(STRING, ReticleOutName) \
    ACTION(STRING, ReticleName) \
    ACTION(STRING, BulletTrailName) \
    ACTION(STRING, BulletImpactName) \
    ACTION(STRING, HitMarkerName)

    DECLARE_SCRIPT_PROPERTIES()
#undef PROPERTY_LIST
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Player();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}