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
    currentCameraDistance = currentRadius;

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

    // 弾丸エフェクトプレハブの取得（追加）
    BulletTrailPrefab = _parentScene->FindPrefabObject(BulletTrailName);
    BulletImpactPrefab = _parentScene->FindPrefabObject(BulletImpactName);
    HitMarkerPrefab = _parentScene->FindPrefabObject(HitMarkerName);

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
    if (!BulletTrailPrefab) {
        std::string msg = "[Player] Warning: BulletTrail prefab not found: " + BulletTrailName + "\n";
        OutputDebugStringA(msg.c_str());
    }
    if (!BulletImpactPrefab) {
        std::string msg = "[Player] Warning: BulletImpact prefab not found: " + BulletImpactName + "\n";
        OutputDebugStringA(msg.c_str());
    }
    if (!HitMarkerPrefab) {
        std::string msg = "[Player] Warning: HitMarker prefab not found: " + HitMarkerName + "\n";
        OutputDebugStringA(msg.c_str());
    }

    SpawnedReticleIn = nullptr;
    SpawnedReticleOut = nullptr;
    SpawnedReticle = nullptr;
    SpawnedHitMarker = nullptr;
    hitMarkerTimer = 0.0f;
}

void Script_Player::Update(float DeltaTime) {
    IScript::Update(DeltaTime);
    PlayerMovement(DeltaTime);
    MouseInput(DeltaTime);
    ShootUpdate(DeltaTime);
    PlayerRotationUpdate(DeltaTime);
    AimUpdate(DeltaTime);
    CameraCollisionUpdate(DeltaTime);
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

    if (Input::IsKeyPressed('W')) {
        inputDirection.z -= 1.0f;
        isMoving = true;
    }
    if (Input::IsKeyPressed('S')) {
        inputDirection.z += 1.0f;
        isMoving = true;
    }
    if (Input::IsKeyPressed('A')) {
        inputDirection.x += 1.0f;
        isMoving = true;
    }
    if (Input::IsKeyPressed('D')) {
        inputDirection.x -= 1.0f;
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

        // 弾丸発射（追加）
        FireBullet();
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
        float angleDiffDeg = AngleDifferenceDeg(playerYaw, cameraYaw);

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
            playerYaw = LerpAngleDeg(playerYaw, cameraYaw, rotationSpeed);
            playerYaw = NormalizeAngleDeg(playerYaw);

            DirectX::XMFLOAT3 playerRotation = _parentObject->GetRotation();
            playerRotation.y = XMConvertToRadians(playerYaw);
            _parentObject->SetRotation(playerRotation);

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

    // この時点ではまだカメラ位置を設定しない
    // CameraCollisionUpdate()で最終的な位置を決定する
}

void Script_Player::CameraCollisionUpdate(float DeltaTime)
{
    if (!CameraObject || !playerCamera || !_parentObject) return;
    if (!enableCameraCollision) {
        // コリジョン無効時は通常のカメラ位置を設定
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
        return;
    }

    // プレイヤーの位置を取得（カメラピボット位置）
    DirectX::XMFLOAT3 playerPos = _parentObject->GetPosition();
    DirectX::XMFLOAT3 pivotPos = DirectX::XMFLOAT3(
        playerPos.x + currentCameraOffset.x,
        playerPos.y + currentCameraOffset.y,
        playerPos.z + currentCameraOffset.z
    );

    // カメラの相対回転を取得
    DirectX::XMFLOAT3 cameraRotation = CameraObject->GetRotation();

    // プレイヤーの回転を取得
    DirectX::XMFLOAT3 playerRotation = _parentObject->GetRotation();

    // カメラのワールド回転を計算（プレイヤー回転 + カメラ相対回転）
    float worldYaw = playerRotation.y + cameraRotation.y;
    float worldPitch = cameraRotation.x;

    // カメラの方向ベクトルを計算（ピボットからカメラへの方向）
    DirectX::XMFLOAT3 cameraDirection;
    cameraDirection.x = cosf(worldPitch) * sinf(worldYaw);
    cameraDirection.y = sinf(worldPitch);
    cameraDirection.z = cosf(worldPitch) * cosf(worldYaw);

    // 正規化（念のため）
    float dirLength = sqrtf(
        cameraDirection.x * cameraDirection.x +
        cameraDirection.y * cameraDirection.y +
        cameraDirection.z * cameraDirection.z
    );
    if (dirLength > 0.001f) {
        cameraDirection.x /= dirLength;
        cameraDirection.y /= dirLength;
        cameraDirection.z /= dirLength;
    }

    // レイキャストでコリジョンチェック
    RayHit hit;
    bool hasHit = Physics::Raycast(pivotPos, cameraDirection, currentRadius, hit);

    float desiredDistance = currentRadius;

    if (hasHit) {
        // 壁に当たった場合、カメラをヒット位置の手前に配置
        desiredDistance = hit.distance - cameraCollisionRadius;
        desiredDistance = Clamp(desiredDistance, cameraCollisionMinDistance, currentRadius);

        std::string debugMsg = "[Player] Camera collision detected at distance: " +
            std::to_string(hit.distance) +
            ", adjusted to: " + std::to_string(desiredDistance) + "\n";
        OutputDebugStringA(debugMsg.c_str());
    }

    // スムーズにカメラ距離を調整
    float smoothFactor = Clamp(cameraCollisionSmoothSpeed * DeltaTime, 0.0f, 1.0f);
    currentCameraDistance = Lerp(currentCameraDistance, desiredDistance, smoothFactor);

    // 最小距離の強制適用（めり込み防止）
    if (currentCameraDistance < cameraCollisionMinDistance) {
        currentCameraDistance = cameraCollisionMinDistance;
    }

    // 再度レイキャストで最終位置を確認（めり込み防止）
    DirectX::XMFLOAT3 testCameraPos;
    testCameraPos.x = pivotPos.x + cameraDirection.x * currentCameraDistance;
    testCameraPos.y = pivotPos.y + cameraDirection.y * currentCameraDistance;
    testCameraPos.z = pivotPos.z + cameraDirection.z * currentCameraDistance;

    // 最終位置から少し後ろに追加のレイキャストを行う
    DirectX::XMFLOAT3 reverseDir;
    reverseDir.x = -cameraDirection.x;
    reverseDir.y = -cameraDirection.y;
    reverseDir.z = -cameraDirection.z;

    RayHit reverseHit;
    bool hasReverseHit = Physics::Raycast(testCameraPos, reverseDir, cameraCollisionRadius * 2.0f, reverseHit);

    if (hasReverseHit && reverseHit.distance < cameraCollisionRadius) {
        // カメラが壁に近すぎる場合、さらに手前に移動
        float additionalOffset = cameraCollisionRadius - reverseHit.distance;
        currentCameraDistance -= additionalOffset;
        currentCameraDistance = Clamp(currentCameraDistance, cameraCollisionMinDistance, currentRadius);

        std::string debugMsg = "[Player] Additional collision adjustment: " + std::to_string(additionalOffset) + "\n";
        OutputDebugStringA(debugMsg.c_str());
    }

    // カメラトランスフォームを更新
    CameraTransform camTransform = playerCamera->GetTransform();
    float currentLength = sqrtf(
        camTransform.position.x * camTransform.position.x +
        camTransform.position.y * camTransform.position.y +
        camTransform.position.z * camTransform.position.z
    );

    if (currentLength > 0.001f) {
        float scale = currentCameraDistance / currentLength;
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

    if (isShooting) {
        return;
    }

    if (isAiming) {
        return;
    }

    int targetAnimationIndex = idleAnimationIndex;

    if (isRunning) {
        targetAnimationIndex = runAnimationIndex;
    }
    else if (isMoving) {
        targetAnimationIndex = runAnimationIndex;
    }

    if (currentAnimationIndex != targetAnimationIndex) {
        playerAnimation->SetClip(targetAnimationIndex);
        playerAnimation->SetLoop(true);
        playerAnimation->SetPlaybackSpeed(1.0f);
        playerAnimation->Play();
        currentAnimationIndex = targetAnimationIndex;
    }
}

void Script_Player::ReticleUpdate(float DeltaTime)
{
    if (!_parentScene) return;

    if (isAiming && !wasAiming) {
        OutputDebugStringA("[Player] Aim started - Spawning ReticleIn and Reticle\n");

        if (ReticleInPrefab) {
            SpawnedReticleIn = _parentScene->AddObject(ReticleInPrefab);
            if (SpawnedReticleIn) {
                OutputDebugStringA("[Player] ReticleIn spawned successfully\n");
            }
            else {
                OutputDebugStringA("[Player] Error: Failed to spawn ReticleIn\n");
            }
        }

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
    else if (!isAiming && wasAiming) {
        OutputDebugStringA("[Player] Aim ended - Removing Reticle and spawning ReticleOut\n");

        if (SpawnedReticle) {
            _parentScene->RemoveObject(SpawnedReticle);
            SpawnedReticle = nullptr;
            OutputDebugStringA("[Player] Reticle removed from scene\n");
        }

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

void Script_Player::FireBullet()
{
    if (!_parentObject || !playerCamera || !CameraObject) {
        OutputDebugStringA("[Player] Error: Cannot fire bullet - missing components\n");
        return;
    }

    // カメラの方向ベクトルを取得
    DirectX::XMFLOAT3 cameraForward = playerCamera->GetForwardVector();
    DirectX::XMFLOAT3 cameraRight = playerCamera->GetRightVector();
    DirectX::XMFLOAT3 cameraUp = playerCamera->GetUpVector();

    // カメラの前方向を反転（レイキャスト用）
    DirectX::XMFLOAT3 shootDirection;
    shootDirection.x = -cameraForward.x;
    shootDirection.y = -cameraForward.y;
    shootDirection.z = -cameraForward.z;

    // カメラオブジェクトのワールド位置を取得
    Float3 Pos = CameraObject->GetWorldTransform().position;
    DirectX::XMFLOAT3 cameraWorldPos = DirectX::XMFLOAT3(Pos.x, Pos.y, Pos.z);

    // muzzleOffsetを適用してレイキャストの開始位置を計算
    DirectX::XMFLOAT3 raycastStartPos;
    raycastStartPos.x = cameraWorldPos.x + cameraRight.x * muzzleOffsetX + cameraUp.x * muzzleOffsetY - cameraForward.x * muzzleOffsetZ;
    raycastStartPos.y = cameraWorldPos.y + cameraRight.y * muzzleOffsetX + cameraUp.y * muzzleOffsetY - cameraForward.y * muzzleOffsetZ;
    raycastStartPos.z = cameraWorldPos.z + cameraRight.z * muzzleOffsetX + cameraUp.z * muzzleOffsetY - cameraForward.z * muzzleOffsetZ;

    // デバッグ: レイキャストの開始位置を出力
    std::string debugStartPos = "[Player] Raycast start position: (" +
        std::to_string(raycastStartPos.x) + ", " +
        std::to_string(raycastStartPos.y) + ", " +
        std::to_string(raycastStartPos.z) + ")\n";
    OutputDebugStringA(debugStartPos.c_str());

    std::string debugOffset = "[Player] MuzzleOffset: X=" + std::to_string(muzzleOffsetX) +
        ", Y=" + std::to_string(muzzleOffsetY) +
        ", Z=" + std::to_string(muzzleOffsetZ) + "\n";
    OutputDebugStringA(debugOffset.c_str());

    // 弾の拡散を計算
    float currentSpread = isAiming ? bulletSpread : hipFireSpread;

    // ランダムな拡散を追加
    float spreadX = ((float)rand() / RAND_MAX - 0.5f) * currentSpread * 2.0f;
    float spreadY = ((float)rand() / RAND_MAX - 0.5f) * currentSpread * 2.0f;

    // 弾の方向ベクトルを計算（カメラの前方向 + 拡散）
    DirectX::XMFLOAT3 bulletDirection;
    bulletDirection.x = shootDirection.x + cameraRight.x * spreadX + cameraUp.x * spreadY;
    bulletDirection.y = shootDirection.y + cameraRight.y * spreadX + cameraUp.y * spreadY;
    bulletDirection.z = shootDirection.z + cameraRight.z * spreadX + cameraUp.z * spreadY;

    // 正規化
    float dirLength = sqrtf(
        bulletDirection.x * bulletDirection.x +
        bulletDirection.y * bulletDirection.y +
        bulletDirection.z * bulletDirection.z
    );
    if (dirLength > 0.001f) {
        bulletDirection.x /= dirLength;
        bulletDirection.y /= dirLength;
        bulletDirection.z /= dirLength;
    }

    // デバッグ: レイキャストの方向を出力
    std::string debugDir = "[Player] Raycast direction: (" +
        std::to_string(bulletDirection.x) + ", " +
        std::to_string(bulletDirection.y) + ", " +
        std::to_string(bulletDirection.z) + ")\n";
    OutputDebugStringA(debugDir.c_str());

    // レイキャストで弾の着弾判定（muzzleOffset適用後の位置から開始）
    RayHit hit;
    bool hasHit = _parentScene->RaycastIgnoreObject(
        raycastStartPos,
        bulletDirection,
        bulletMaxRange,
        _parentObject,
        &hit
    );

    // 銃口位置はレイキャスト開始位置と同じ（エフェクト表示用）
    DirectX::XMFLOAT3 muzzlePos = raycastStartPos;

    if (hasHit) {
        // ヒット地点を計算
        DirectX::XMFLOAT3 hitPoint = DirectX::XMFLOAT3(
            hit.point.x,
            hit.point.y,
            hit.point.z
        );

        std::string debugMsg = "[Player] Bullet hit: " + std::string(hit.hitObjectName) +
            " at distance: " + std::to_string(hit.distance) + "\n";
        OutputDebugStringA(debugMsg.c_str());

        // 弾道エフェクトは銃口から着弾点まで表示
        CreateBulletTrail(muzzlePos, hitPoint);

        DirectX::XMFLOAT3 hitNormal = DirectX::XMFLOAT3(
            hit.normal.x,
            hit.normal.y,
            hit.normal.z
        );
        CreateImpactEffect(hitPoint, hitNormal);

        ShowHitMarker(hitPoint, hitNormal);

        if (hit.hitObject) {
            // TODO: ここでヒットしたオブジェクトにダメージを与える処理を追加
            // 例: hit.hitObject->ApplyDamage(bulletDamage);
        }
    }
    else {
        // 何にも当たらなかった場合は最大射程地点まで弾道を表示
        DirectX::XMFLOAT3 endPoint;
        endPoint.x = raycastStartPos.x + bulletDirection.x * bulletMaxRange;
        endPoint.y = raycastStartPos.y + bulletDirection.y * bulletMaxRange;
        endPoint.z = raycastStartPos.z + bulletDirection.z * bulletMaxRange;

        // 弾道エフェクトは銃口から終点まで表示
        CreateBulletTrail(muzzlePos, endPoint);

        OutputDebugStringA("[Player] Bullet missed - no hit detected\n");
    }
}

void Script_Player::CreateBulletTrail(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end)
{
    if (!BulletTrailPrefab || !_parentScene) return;

    // 弾道エフェクトを生成
    Object* trail = _parentScene->AddObject(BulletTrailPrefab);
    if (trail) {
        // 開始位置に配置
        trail->SetPosition(start);

        // 方向を計算
        DirectX::XMFLOAT3 direction;
        direction.x = end.x - start.x;
        direction.y = end.y - start.y;
        direction.z = end.z - start.z;

        float distance = sqrtf(
            direction.x * direction.x +
            direction.y * direction.y +
            direction.z * direction.z
        );

        // エフェクトのスケールを調整（距離に応じて）
        trail->SetScale(DirectX::XMFLOAT3(0.05f, 0.05f, distance));

        // エフェクトの向きを設定
        float yaw = atan2f(direction.x, direction.z);
        float pitch = atan2f(-direction.y, sqrtf(direction.x * direction.x + direction.z * direction.z));
        trail->SetRotation(DirectX::XMFLOAT3(pitch, yaw, 0.0f));

        OutputDebugStringA("[Player] Bullet trail created\n");
    }
}

void Script_Player::CreateImpactEffect(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& normal)
{
    if (!BulletImpactPrefab || !_parentScene) return;

    // 着弾エフェクトを生成
    Object* impact = _parentScene->AddObject(BulletImpactPrefab);
    if (impact) {
        // 着弾位置に配置
        impact->SetPosition(position);

        // 法線方向を向くように回転を設定
        float yaw = atan2f(normal.x, normal.z);
        float pitch = atan2f(-normal.y, sqrtf(normal.x * normal.x + normal.z * normal.z));
        impact->SetRotation(DirectX::XMFLOAT3(pitch, yaw, 0.0f));

        // エフェクトコンポーネントを取得して再生
        Effect* effectComp = impact->GetComponent<Effect>("Effect");
        if (effectComp) {
            effectComp->Play();
        }

        OutputDebugStringA("[Player] Impact effect created\n");
    }
}

void Script_Player::ShowHitMarker(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& normal)
{
    if (!HitMarkerPrefab || !_parentScene) return;

    // 新しいヒットマーカーを生成
    Object* hitMarker = _parentScene->AddObject(HitMarkerPrefab);
    if (hitMarker) {
        // ヒット位置に配置
        hitMarker->SetPosition(position);

        // 法線方向を向くように回転を設定
        float yaw = atan2f(normal.x, normal.z);
        float pitch = atan2f(-normal.y, sqrtf(normal.x * normal.x + normal.z * normal.z));
        hitMarker->SetRotation(DirectX::XMFLOAT3(pitch, yaw, 0.0f));

        // エフェクトコンポーネントを取得して再生（存在する場合）
        Effect* effectComp = hitMarker->GetComponent<Effect>("Effect");
        if (effectComp) {
            effectComp->Play();
        }

        OutputDebugStringA("[Player] Hit marker displayed at hit position\n");
    }
}