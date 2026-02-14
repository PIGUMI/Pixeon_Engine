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
    void ReticleUpdate(float DeltaTime);  // 追加
public:
    Object* CameraObject = nullptr;
    Camera* playerCamera = nullptr;
    Rigidbody* playerRigidbody = nullptr;
    Animation* playerAnimation = nullptr;

    Object* ReticleInPrefab = nullptr;   // エイムインアニメーションのプレハブ
    Object* ReticleOutPrefab = nullptr;  // エイムアウトアニメーションのプレハブ
    Object* ReticlePrefab = nullptr;     // エイム中のレティクルプレハブ

    Object* SpawnedReticleIn = nullptr;   // 召喚されたReticleInインスタンス
    Object* SpawnedReticleOut = nullptr;  // 召喚されたReticleOutインスタンス
    Object* SpawnedReticle = nullptr;     // 召喚されたReticleインスタンス

    // エイム関連
    bool isAiming = false;
    bool wasAiming = false;         // 前フレームのエイム状態
    bool isShooting = false;        // 射撃中フラグ
    bool canShoot = true;           // 射撃可能フラグ
    float shootAnimationTimer = 0.0f; // 射撃アニメーションタイマー
    float currentRadius = 1.8f;
    DirectX::XMFLOAT3 currentCameraOffset;
    DirectX::XMFLOAT3 defaultCameraOffset;

    // エイム時のカメラ設定
    float defaultRadius = 1.8f;      // 通常時のRadius
    float aimRadius = 0.9f;          // エイム時のRadius
    DirectX::XMFLOAT3 aimCameraOffset = DirectX::XMFLOAT3(0.5f, 1.4f, 0.0f); // エイム時のオフセット
    float aimTransitionSpeed = 10.0f; // エイムの遷移速度

    // プレイヤー回転関連
    float cameraYaw = 0.0f;          // カメラの水平回転角度
    float playerYaw = 0.0f;          // プレイヤーの水平回転角度
    float playerRotationSpeed = 8.0f; // プレイヤーの回転速度
    bool isMoving = false;           // 移動中フラグ
    bool isRunning = false;

    // 移動関連
    float walkSpeed = 3.0f;          // 歩行速度
    float runSpeed = 6.0f;           // 走行速度
    float aimWalkSpeed = 2.0f;       // エイム時の歩行速度
    DirectX::XMFLOAT3 moveDirection; // 移動方向

    // カメラ設定
    float cameraVerticalAngleDegMin = -70.0f; // カメラ上下角度の最小値（度）
    float cameraVerticalAngleDegMax = 35.0f;  // カメラ上下角度の最大値（度）
    float cameraSensitivity = 10.0f;          // カメラ感度（度/秒）
    float aimCameraSensitivity = 5.0f;        // エイム時カメラ感度（度/秒）

    // プレイヤー回転設定
    float normalRotationThreshold = 5.0f;     // 通常時の回転開始閾値（度）
    float aimRotationThreshold = 0.5f;        // エイム時の回転開始閾値（度）
    float aimRotationSpeedMultiplier = 2.0f;  // エイム時の回転速度倍率

    // アニメーション設定
    int idleAnimationIndex = 0;      // 待機アニメーション番号
    int runAnimationIndex = 1;       // 走行アニメーション番号
    int aimAnimationIndex = 2;       // エイム射撃アニメーション番号
    int currentAnimationIndex = -1;  // 現在再生中のアニメーション番号
    float shootAnimationSpeed = 1.5f; // 射撃アニメーション速度

    std::string ReticleInName = "ReticleIn";   // エイムインアニメーションの名前
    std::string ReticleOutName = "ReticleOut"; // エイムアウトアニメーションの名前
    std::string ReticleName = "Reticle";       // エイム中のレティクルオブジェクトの名前

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
    ACTION(INT, idleAnimationIndex) \
    ACTION(INT, runAnimationIndex) \
    ACTION(INT, aimAnimationIndex) \
    ACTION(STRING, ReticleInName) \
    ACTION(STRING, ReticleOutName) \
    ACTION(STRING, ReticleName)

    DECLARE_SCRIPT_PROPERTIES()
#undef PROPERTY_LIST
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Player();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}