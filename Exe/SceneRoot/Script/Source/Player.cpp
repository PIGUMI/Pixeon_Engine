#include "Player.h"
#include <Windows.h>
#include <DirectXMath.h>

template<typename T>
inline T Clamp(T value, T min, T max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

template<typename T>
inline T Lerp(T a, T b, float t) {
    return a + (b - a) * t;
}

inline DirectX::XMFLOAT3 LerpFloat3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t) {
    return DirectX::XMFLOAT3(
        Lerp(a.x, b.x, t),
        Lerp(a.y, b.y, t),
        Lerp(a.z, b.z, t)
    );
}

inline float NormalizeAngleDeg(float angle) {
    while (angle > 180.0f) angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
    return angle;
}

inline float AngleDifferenceDeg(float from, float to) {
    float diff = NormalizeAngleDeg(to - from);
    return diff;
}

inline float LerpAngleDeg(float from, float to, float t) {
    float diff = AngleDifferenceDeg(from, to);
    return NormalizeAngleDeg(from + diff * t);
}

using namespace DirectX;

void Script_Player::BeginPlay() {
    IScript::BeginPlay();

    /* カメラ設定 */
    CameraObject = _parentObject->FindChildObject("Camera");
    if (!CameraObject) {
        return;
    }
    playerCamera = CameraObject->GetComponent<Camera>("CameraComponent");
    _parentScene->SetMainCamera(playerCamera);
    Input::FixMouseCursor(true);

    /* Rigidbody取得 */
    playerRigidbody = _parentObject->GetComponent<Rigidbody>("RigidBody");
    if (playerRigidbody) {
        playerRigidbody->SetUseGravity(true);
        playerRigidbody->SetAngularDamping(1.0f);
    }

    /* Animation取得 */
    playerAnimation = _parentObject->GetComponent<Animation>("Animation");
    if (playerAnimation) {
        playerAnimation->SetClip(idleAnimationIndex);
        playerAnimation->SetLoop(true);
        playerAnimation->Play();
        currentAnimationIndex = idleAnimationIndex;
    }

    defaultCameraOffset = CameraObject->GetPosition();
    currentCameraOffset = defaultCameraOffset;

    CameraTransform camTransform = playerCamera->GetTransform();
    defaultRadius = sqrtf(
        camTransform.position.x * camTransform.position.x +
        camTransform.position.y * camTransform.position.y +
        camTransform.position.z * camTransform.position.z
    );
    currentRadius = defaultRadius;

    playerYaw = XMConvertToDegrees(_parentObject->GetRotation().y);
    cameraYaw = playerYaw;

    moveDirection = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    wasAiming = false;
    canShoot = true;
    shootAnimationTimer = 0.0f;

    // プレハブの取得
    ReticleInPrefab = _parentScene->FindPrefabObject(ReticleInName);
    ReticleOutPrefab = _parentScene->FindPrefabObject(ReticleOutName);
    ReticlePrefab = _parentScene->FindPrefabObject(ReticleName);

    if (!ReticleInPrefab) {
        std::string msg = "[Player] Warning: ReticleIn prefab not found: " + ReticleInName + "\n";
        OutputDebugStringA(msg.c_str());
    }
    if (!ReticleOutPrefab) {
        std::string msg = "[Player] Warning: ReticleOut prefab not found: " + ReticleOutName + "\n";
        OutputDebugStringA(msg.c_str());
    }
    if (!ReticlePrefab) {
        std::string msg = "[Player] Warning: Reticle prefab not found: " + ReticleName + "\n";
        OutputDebugStringA(msg.c_str());
    }

    SpawnedReticleIn = nullptr;
    SpawnedReticleOut = nullptr;
    SpawnedReticle = nullptr;
}

void Script_Player::Update(float DeltaTime) {
    IScript::Update(DeltaTime);
    PlayerMovement(DeltaTime);
    MouseInput(DeltaTime);
    ShootUpdate(DeltaTime);
    PlayerRotationUpdate(DeltaTime);
    AimUpdate(DeltaTime);
    AnimationUpdate(DeltaTime);
    ReticleUpdate(DeltaTime);

    wasAiming = isAiming;
}

void Script_Player::EndPlay() {
    IScript::EndPlay();

    if (playerCamera) {
        delete playerCamera;
        playerCamera = nullptr;
    }
    if (CameraObject) {
        delete CameraObject;
        CameraObject = nullptr;
    }
    if (playerRigidbody) {
        delete playerRigidbody;
        playerRigidbody = nullptr;
    }
    if (playerAnimation) {
        delete playerAnimation;
        playerAnimation = nullptr;
    }
}

void Script_Player::PlayerMovement(float DeltaTime)
{
    if (!playerRigidbody || !playerCamera) return;
    DirectX::XMFLOAT3 inputDirection(0.0f, 0.0f, 0.0f);
    isMoving = false;
    isRunning = false;

    // WASD入力を取得
    if (Input::IsKeyPressed('W')) {
        inputDirection.z -= 1.0f; // 前方
        isMoving = true;
    }
    if (Input::IsKeyPressed('S')) {
        inputDirection.z += 1.0f; // 後方
        isMoving = true;
    }
    if (Input::IsKeyPressed('A')) {
        inputDirection.x += 1.0f; // 左
        isMoving = true;
    }
    if (Input::IsKeyPressed('D')) {
        inputDirection.x -= 1.0f; // 右
        isMoving = true;
    }

    if (isMoving) {
        float inputLength = sqrtf(inputDirection.x * inputDirection.x + inputDirection.z * inputDirection.z);
        if (inputLength > 0.001f) {
            inputDirection.x /= inputLength;
            inputDirection.z /= inputLength;
        }

        DirectX::XMFLOAT3 cameraForward = playerCamera->GetForwardVector();
        DirectX::XMFLOAT3 cameraRight = playerCamera->GetRightVector();

        cameraForward.y = 0.0f;
        cameraRight.y = 0.0f;

        float forwardLength = sqrtf(cameraForward.x * cameraForward.x + cameraForward.z * cameraForward.z);
        if (forwardLength > 0.001f) {
            cameraForward.x /= forwardLength;
            cameraForward.z /= forwardLength;
        }

        float rightLength = sqrtf(cameraRight.x * cameraRight.x + cameraRight.z * cameraRight.z);
        if (rightLength > 0.001f) {
            cameraRight.x /= rightLength;
            cameraRight.z /= rightLength;
        }

        DirectX::XMFLOAT3 worldDirection;
        worldDirection.x = cameraRight.x * inputDirection.x + cameraForward.x * inputDirection.z;
        worldDirection.y = 0.0f;
        worldDirection.z = cameraRight.z * inputDirection.x + cameraForward.z * inputDirection.z;

        float currentSpeed = walkSpeed;
        if (Input::IsKeyPressed(VK_SHIFT) && !isAiming) {
            currentSpeed = runSpeed;
            isRunning = true;
        }
        if (isAiming) {
            currentSpeed = aimWalkSpeed;
        }

        DirectX::XMFLOAT3 targetVelocity;
        targetVelocity.x = worldDirection.x * currentSpeed;
        targetVelocity.z = worldDirection.z * currentSpeed;

        DirectX::XMFLOAT3 currentVelocity = playerRigidbody->GetVelocity();
        targetVelocity.y = currentVelocity.y;

        playerRigidbody->SetVelocity(targetVelocity);
        moveDirection = worldDirection;
    }
    else {
        DirectX::XMFLOAT3 currentVelocity = playerRigidbody->GetVelocity();
        currentVelocity.x = 0.0f;
        currentVelocity.z = 0.0f;
        playerRigidbody->SetVelocity(currentVelocity);
    }
}

void Script_Player::MouseInput(float DeltaTime)
{
    if (!CameraObject || !playerCamera) return;

    int mouseX = Input::GetMouseMoveX();
    int mouseY = Input::GetMouseMoveY();

    float rotationSensitivity = isAiming ? aimCameraSensitivity : cameraSensitivity;

    cameraYaw += mouseX * rotationSensitivity * DeltaTime;
    cameraYaw = NormalizeAngleDeg(cameraYaw);

    DirectX::XMFLOAT3 cameraRotation = CameraObject->GetRotation();
    float cameraVerticalAngleDeg = XMConvertToDegrees(cameraRotation.x);

    cameraVerticalAngleDeg += mouseY * rotationSensitivity * DeltaTime;
    cameraVerticalAngleDeg = Clamp(cameraVerticalAngleDeg, cameraVerticalAngleDegMin, cameraVerticalAngleDegMax);

    cameraRotation.x = XMConvertToRadians(cameraVerticalAngleDeg);

    float relativeAngleDeg = AngleDifferenceDeg(playerYaw, cameraYaw);
    cameraRotation.y = XMConvertToRadians(relativeAngleDeg);

    CameraObject->SetRotation(cameraRotation);

    if (Input::IsKeyPressed(VK_RBUTTON)) {
        isAiming = true;
    }
    else {
        isAiming = false;
    }
}

void Script_Player::ShootUpdate(float DeltaTime)
{
    if (isShooting) {
        shootAnimationTimer += DeltaTime;

        const float shootAnimationDuration = 0.7f;

        if (shootAnimationTimer >= shootAnimationDuration) {
            isShooting = false;
            canShoot = true;
            shootAnimationTimer = 0.0f;

            if (isAiming && playerAnimation) {
                playerAnimation->SetClip(aimAnimationIndex);
                playerAnimation->SetLoop(false);
                playerAnimation->Play();
            }
        }
    }

    // エイムを開始した瞬間（前フレームはエイムしていない、現在フレームはエイム中）
    static int aimFrameCount = 0;
    if (isAiming && !wasAiming) {
        if (playerAnimation) {
            playerAnimation->SetClip(aimAnimationIndex);
            playerAnimation->SetLoop(false);
            playerAnimation->SetPlaybackSpeed(1.0f);
            playerAnimation->Play();
            currentAnimationIndex = aimAnimationIndex;
            aimFrameCount = 0;
            canShoot = true;
        }
    }

    if (isAiming && !isShooting && currentAnimationIndex == aimAnimationIndex) {
        if (!wasAiming) {
            aimFrameCount = 0;
        }
        aimFrameCount++;

        if (aimFrameCount >= 1 && playerAnimation) {
            playerAnimation->Pause();
        }
    }
    else if (!isAiming || isShooting) {
        aimFrameCount = 0;
    }

    if (isAiming && Input::IsKeyTriggered(VK_LBUTTON) && canShoot) {
        isShooting = true;
        canShoot = false;
        shootAnimationTimer = 0.0f;
        aimFrameCount = 0;

        if (playerAnimation) {
            playerAnimation->SetClip(aimAnimationIndex);
            playerAnimation->SetLoop(false);
            playerAnimation->SetPlaybackSpeed(shootAnimationSpeed);
            playerAnimation->Restart();
            playerAnimation->Play();
            currentAnimationIndex = aimAnimationIndex;
        }

        // ここで弾を発射する処理を追加
        // TODO: 発射処理の実装
    }

    if (!isAiming) {
        if (isShooting) {
            isShooting = false;
            shootAnimationTimer = 0.0f;
        }
        canShoot = true;
        aimFrameCount = 0;
    }
}

void Script_Player::PlayerRotationUpdate(float DeltaTime)
{
    if (!_parentObject || !CameraObject) return;

    bool shouldRotate = isAiming || isMoving;

    if (shouldRotate) {
        // 角度差を計算（度数法）
        float angleDiffDeg = AngleDifferenceDeg(playerYaw, cameraYaw);

        // エイム時と通常時で異なる設定を使用
        float rotationSpeed;
        float rotationStartThresholdDeg;

        if (isAiming) {
            rotationSpeed = playerRotationSpeed * aimRotationSpeedMultiplier * DeltaTime;
            rotationStartThresholdDeg = aimRotationThreshold;
        }
        else {
            rotationSpeed = playerRotationSpeed * DeltaTime;
            rotationStartThresholdDeg = normalRotationThreshold;
        }

        if (abs(angleDiffDeg) > rotationStartThresholdDeg) {
            // プレイヤーを回転（度数法）
            playerYaw = LerpAngleDeg(playerYaw, cameraYaw, rotationSpeed);
            playerYaw = NormalizeAngleDeg(playerYaw);

            // プレイヤーの回転を適用（ラジアンに変換）
            DirectX::XMFLOAT3 playerRotation = _parentObject->GetRotation();
            playerRotation.y = XMConvertToRadians(playerYaw);
            _parentObject->SetRotation(playerRotation);

            // カメラの相対角度を再計算
            float newRelativeAngleDeg = AngleDifferenceDeg(playerYaw, cameraYaw);
            DirectX::XMFLOAT3 cameraRotation = CameraObject->GetRotation();
            cameraRotation.y = XMConvertToRadians(newRelativeAngleDeg);
            CameraObject->SetRotation(cameraRotation);
        }
    }
}

void Script_Player::AimUpdate(float DeltaTime)
{
    if (!CameraObject || !playerCamera) return;

    float targetRadius = isAiming ? aimRadius : defaultRadius;
    DirectX::XMFLOAT3 targetOffset = isAiming ? aimCameraOffset : defaultCameraOffset;

    float t = Clamp(aimTransitionSpeed * DeltaTime, 0.0f, 1.0f);

    currentRadius = Lerp(currentRadius, targetRadius, t);
    currentCameraOffset = LerpFloat3(currentCameraOffset, targetOffset, t);

    CameraTransform camTransform = playerCamera->GetTransform();

    float currentLength = sqrtf(
        camTransform.position.x * camTransform.position.x +
        camTransform.position.y * camTransform.position.y +
        camTransform.position.z * camTransform.position.z
    );

    if (currentLength > 0.001f) {
        float scale = currentRadius / currentLength;
        camTransform.position.x *= scale;
        camTransform.position.y *= scale;
        camTransform.position.z *= scale;
    }

    playerCamera->SetTransform(camTransform);
    CameraObject->SetPosition(currentCameraOffset);
}

void Script_Player::AnimationUpdate(float DeltaTime)
{
    if (!playerAnimation) return;

    // 射撃中はアニメーションを変更しない
    if (isShooting) {
        return;
    }

    // エイム中（構えている状態）もアニメーションを変更しない
    if (isAiming) {
        return;
    }

    // アニメーションの優先順位: 走行 > 待機
    int targetAnimationIndex = idleAnimationIndex;

    if (isRunning) {
        // 走行中は走行アニメーション
        targetAnimationIndex = runAnimationIndex;
    }
    else if (isMoving) {
        // 歩行中も走行アニメーション（速度は違うが同じアニメーション）
        targetAnimationIndex = runAnimationIndex;
    }

    // アニメーションが変更された場合のみ切り替え
    if (currentAnimationIndex != targetAnimationIndex) {
        playerAnimation->SetClip(targetAnimationIndex);
        playerAnimation->SetLoop(true); // 待機・走行はループ
        playerAnimation->SetPlaybackSpeed(1.0f); // 通常速度
        playerAnimation->Play();
        currentAnimationIndex = targetAnimationIndex;
    }
}

void Script_Player::ReticleUpdate(float DeltaTime)
{
    if (!_parentScene) return;

    // エイム開始時
    if (isAiming && !wasAiming) {
        OutputDebugStringA("[Player] Aim started - Spawning ReticleIn and Reticle\n");

        // ReticleInを召喚
        if (ReticleInPrefab) {
            SpawnedReticleIn = _parentScene->AddObject(ReticleInPrefab);
            if (SpawnedReticleIn) {
                OutputDebugStringA("[Player] ReticleIn spawned successfully\n");
            }
            else {
                OutputDebugStringA("[Player] Error: Failed to spawn ReticleIn\n");
            }
        }

        // Reticleを召喚
        if (ReticlePrefab) {
            SpawnedReticle = _parentScene->AddObject(ReticlePrefab);
            if (SpawnedReticle) {
                OutputDebugStringA("[Player] Reticle spawned successfully\n");
            }
            else {
                OutputDebugStringA("[Player] Error: Failed to spawn Reticle\n");
            }
        }
    }
    // エイム解除時
    else if (!isAiming && wasAiming) {
        OutputDebugStringA("[Player] Aim ended - Removing Reticle and spawning ReticleOut\n");

        // Reticleをシーンから削除
        if (SpawnedReticle) {
            _parentScene->RemoveObject(SpawnedReticle);
            SpawnedReticle = nullptr;
            OutputDebugStringA("[Player] Reticle removed from scene\n");
        }

        // ReticleOutを召喚
        if (ReticleOutPrefab) {
			SpawnedReticleOut = _parentScene->AddObject(ReticleOutPrefab);
            if (SpawnedReticleOut) {
                OutputDebugStringA("[Player] ReticleOut spawned successfully\n");
            }
            else {
                OutputDebugStringA("[Player] Error: Failed to spawn ReticleOut\n");
            }
        }
    }
}