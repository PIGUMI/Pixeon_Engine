#include "Enemy.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void Script_Enemy::BeginPlay() {
    IScript::BeginPlay();

    // プレイヤーオブジェクトを検索
    playerObject = _parentScene->FindObject("Player");

    if (!playerObject) {
        // プレイヤーが見つからない場合、タグで検索を試みる
        // ここでは名前で検索していますが、必要に応じて変更してください
    }

    // 初期状態を巡回に設定
    currentState = EnemyState::Patrol;
    patrolTimer = 0.0f;
}

void Script_Enemy::Update(float DeltaTime) {
    IScript::Update(DeltaTime);

    if (!playerObject) {
        return;
    }

    // 現在の状態に応じた処理
    switch (currentState) {
    case EnemyState::Patrol:
        UpdatePatrol(DeltaTime);
        break;
    case EnemyState::Chase:
        UpdateChase(DeltaTime);
        break;
    case EnemyState::Search:
        UpdateSearch(DeltaTime);
        break;
    case EnemyState::Attack:
        UpdateAttack(DeltaTime);
        break;
    }
}

void Script_Enemy::EndPlay() {
    IScript::EndPlay();
}

// 視界判定用レイキャスト
bool Script_Enemy::CheckVisionRaycast(const DirectX::XMFLOAT3& direction, float distance, RayHit& outHit) {
    DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
    myPos.y += 1.0f; // 目の高さに調整

    return _parentScene->Raycast(myPos, direction, distance, &outHit);
}

// プレイヤーが視界内にいるか確認（3本のレイキャスト）
bool Script_Enemy::IsPlayerInSight(float& outDistance) {
    DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
    DirectX::XMFLOAT3 myRot = _parentObject->GetRotation();

    // 前方向ベクトルを計算
    float yaw = myRot.y * (M_PI / 180.0f);
    DirectX::XMFLOAT3 forward = {
        sinf(yaw),
        0.0f,
        cosf(yaw)
    };

    // 現在の状態に応じた視界範囲を設定
    float currentRange = (currentState == EnemyState::Chase) ? chaseRange : detectionRange;

    // 3本のレイキャストを45度ずつずらして発射
    float angles[] = { -visionAngle, 0.0f, visionAngle };

    for (float angle : angles) {
        DirectX::XMFLOAT3 rayDirection = RotateVectorY(forward, angle);
        RayHit hit;

        if (CheckVisionRaycast(rayDirection, currentRange, hit)) {
            // ヒットしたオブジェクトがプレイヤーか確認
            if (hit.hitObject == playerObject->GetHandle()) {
                DirectX::XMFLOAT3 playerPos = playerObject->GetPosition();
                DirectX::XMFLOAT3 diff = {
                    playerPos.x - myPos.x,
                    playerPos.y - myPos.y,
                    playerPos.z - myPos.z
                };
                outDistance = VectorLength(diff);
                return true;
            }
        }
    }

    return false;
}

// 障害物回避（4方向のレイキャスト）
DirectX::XMFLOAT3 Script_Enemy::AvoidObstacles() {
    DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
    DirectX::XMFLOAT3 myRot = _parentObject->GetRotation();

    float yaw = myRot.y * (M_PI / 180.0f);
    DirectX::XMFLOAT3 forward = {
        sinf(yaw),
        0.0f,
        cosf(yaw)
    };

    DirectX::XMFLOAT3 avoidanceVector = { 0.0f, 0.0f, 0.0f };

    // 4方向（前、右、後、左）
    float checkAngles[] = { 0.0f, 90.0f, 180.0f, -90.0f };
    float weights[] = { 2.0f, 1.0f, 0.5f, 1.0f }; // 前方を重視

    for (int i = 0; i < 4; i++) {
        DirectX::XMFLOAT3 checkDir = RotateVectorY(forward, checkAngles[i]);
        RayHit hit;

        myPos.y += 0.5f; // 腰の高さでチェック
        if (_parentScene->RaycastIgnoreTriggers(myPos, checkDir, obstacleAvoidDistance, &hit)) {
            if (hit.bHit && hit.distance < obstacleAvoidDistance) {
                // 障害物があった場合、反対方向に回避ベクトルを追加
                float avoidStrength = (1.0f - hit.distance / obstacleAvoidDistance) * weights[i];
                avoidanceVector.x -= checkDir.x * avoidStrength;
                avoidanceVector.z -= checkDir.z * avoidStrength;
            }
        }
        myPos.y -= 0.5f;
    }

    return avoidanceVector;
}

// 目標地点への移動
void Script_Enemy::MoveTowards(const DirectX::XMFLOAT3& targetPosition, float deltaTime) {
    DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();

    // 目標方向を計算
    DirectX::XMFLOAT3 direction = {
        targetPosition.x - myPos.x,
        0.0f,
        targetPosition.z - myPos.z
    };

    // 障害物回避ベクトルを追加
    DirectX::XMFLOAT3 avoidance = AvoidObstacles();
    direction.x += avoidance.x;
    direction.z += avoidance.z;

    // 正規化
    float length = VectorLength(direction);
    if (length > 0.001f) {
        direction.x /= length;
        direction.z /= length;

        // 移動
        myPos.x += direction.x * moveSpeed * deltaTime;
        myPos.z += direction.z * moveSpeed * deltaTime;

        _parentObject->SetPosition(myPos);

        // 移動方向を向く
        RotateTowards(targetPosition, deltaTime);
    }
}

// 目標方向への回転
void Script_Enemy::RotateTowards(const DirectX::XMFLOAT3& targetPosition, float deltaTime) {
    DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
    DirectX::XMFLOAT3 myRot = _parentObject->GetRotation();

    // 目標への角度を計算
    float dx = targetPosition.x - myPos.x;
    float dz = targetPosition.z - myPos.z;
    float targetYaw = atan2f(dx, dz) * (180.0f / M_PI);

    // 現在の角度との差分を計算
    float angleDiff = targetYaw - myRot.y;

    // -180 ~ 180 の範囲に正規化
    while (angleDiff > 180.0f) angleDiff -= 360.0f;
    while (angleDiff < -180.0f) angleDiff += 360.0f;

    // 滑らかに回転
    float rotationAmount = std::min(std::abs(angleDiff), rotationSpeed * deltaTime);
    if (angleDiff < 0) rotationAmount = -rotationAmount;

    myRot.y += rotationAmount;

    // 0 ~ 360 の範囲に正規化
    while (myRot.y >= 360.0f) myRot.y -= 360.0f;
    while (myRot.y < 0.0f) myRot.y += 360.0f;

    _parentObject->SetRotation(myRot);
}

// 巡回状態の更新
void Script_Enemy::UpdatePatrol(float deltaTime) {
    patrolTimer += deltaTime;

    // プレイヤーを発見したか確認
    float distanceToPlayer;
    if (IsPlayerInSight(distanceToPlayer)) {
        currentState = EnemyState::Chase;
        lastKnownPlayerPosition = playerObject->GetPosition();
        return;
    }

    // 一定時間ごとに巡回方向を変更
    if (patrolTimer > 3.0f) {
        patrolTimer = 0.0f;
        float randomAngle = (rand() % 360) * (M_PI / 180.0f);
        patrolDirection = {
            sinf(randomAngle),
            0.0f,
            cosf(randomAngle)
        };
    }

    // 巡回移動
    DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
    DirectX::XMFLOAT3 targetPos = {
        myPos.x + patrolDirection.x * 5.0f,
        myPos.y,
        myPos.z + patrolDirection.z * 5.0f
    };

    MoveTowards(targetPos, deltaTime);
}

// 追跡状態の更新
void Script_Enemy::UpdateChase(float deltaTime) {
    float distanceToPlayer;

    // プレイヤーが視界内にいる場合
    if (IsPlayerInSight(distanceToPlayer)) {
        lastKnownPlayerPosition = playerObject->GetPosition();

        // 攻撃範囲内なら攻撃状態へ
        if (distanceToPlayer <= attackRange) {
            currentState = EnemyState::Attack;
            return;
        }

        // プレイヤーに向かって移動
        MoveTowards(lastKnownPlayerPosition, deltaTime);
    }
    else {
        // 視界から外れた場合、最後の位置へ移動して探索モードへ
        DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
        float distToLastKnown = VectorLength({
            lastKnownPlayerPosition.x - myPos.x,
            0.0f,
            lastKnownPlayerPosition.z - myPos.z
            });

        if (distToLastKnown > 1.0f) {
            MoveTowards(lastKnownPlayerPosition, deltaTime);
        }
        else {
            // 最後の位置に到達したら探索モードへ
            currentState = EnemyState::Search;
            searchTimer = 0.0f;
        }
    }
}

// 探索状態の更新
void Script_Enemy::UpdateSearch(float deltaTime) {
    searchTimer += deltaTime;

    // プレイヤーを再発見したか確認
    float distanceToPlayer;
    if (IsPlayerInSight(distanceToPlayer)) {
        currentState = EnemyState::Chase;
        lastKnownPlayerPosition = playerObject->GetPosition();
        return;
    }

    // 周囲を見回す（回転）
    DirectX::XMFLOAT3 myRot = _parentObject->GetRotation();
    myRot.y += rotationSpeed * deltaTime * 0.5f; // ゆっくり回転

    while (myRot.y >= 360.0f) myRot.y -= 360.0f;
    _parentObject->SetRotation(myRot);

    // 一定時間経過したら巡回モードに戻る
    if (searchTimer >= searchTime) {
        currentState = EnemyState::Patrol;
        patrolTimer = 0.0f;
    }
}

// 攻撃状態の更新
void Script_Enemy::UpdateAttack(float deltaTime) {
    float distanceToPlayer = GetDistanceToPlayer();

    // プレイヤーの方を向く
    RotateTowards(playerObject->GetPosition(), deltaTime);

    // 攻撃範囲外なら追跡モードに戻る
    if (distanceToPlayer > attackRange) {
        float visionDist;
        if (IsPlayerInSight(visionDist)) {
            currentState = EnemyState::Chase;
        }
        else {
            currentState = EnemyState::Search;
            searchTimer = 0.0f;
        }
        return;
    }

    // ここに攻撃処理を追加
    // 例: アニメーション再生、ダメージ処理など
}

// プレイヤーまでの距離を取得
float Script_Enemy::GetDistanceToPlayer() {
    if (!playerObject) return 9999.0f;

    DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
    DirectX::XMFLOAT3 playerPos = playerObject->GetPosition();

    DirectX::XMFLOAT3 diff = {
        playerPos.x - myPos.x,
        playerPos.y - myPos.y,
        playerPos.z - myPos.z
    };

    return VectorLength(diff);
}

// ベクトルの正規化
DirectX::XMFLOAT3 Script_Enemy::NormalizeVector(const DirectX::XMFLOAT3& v) {
    float length = VectorLength(v);
    if (length < 0.001f) {
        return { 0.0f, 0.0f, 0.0f };
    }
    return { v.x / length, v.y / length, v.z / length };
}

// ベクトルの長さを計算
float Script_Enemy::VectorLength(const DirectX::XMFLOAT3& v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

// Y軸周りにベクトルを回転
DirectX::XMFLOAT3 Script_Enemy::RotateVectorY(const DirectX::XMFLOAT3& v, float angleDeg) {
    float angleRad = angleDeg * (M_PI / 180.0f);
    float cosA = cosf(angleRad);
    float sinA = sinf(angleRad);

    return {
        v.x * cosA - v.z * sinA,
        v.y,
        v.x * sinA + v.z * cosA
    };
}