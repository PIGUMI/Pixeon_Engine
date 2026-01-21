#include "AI.h"
#include <cmath>

void Script_AI::BeginPlay() {
    GetCurrentScene(&currentScene);
    aiObject = _parentObject;

    FindObjectByName(currentScene, "Player", &playerObject);
    FindComponent(aiObject, "RigidBody", &rigidBody);

    // パラメータ初期化
    detectionRange = 50.0f;
    moveSpeed = 5.0f;
    avoidanceDistance = 3.5f;
    rotationSpeed = 5.0f;
    stuckTimer = 0.0f;
    stuckThreshold = 1.0f;
    avoidanceTimer = 0.0f;
    avoidanceDuration = 0.5f;

    currentState = AIState::Idle;
    lastPosition = GetCurrentPosition();
    currentAvoidanceDirection = CreateFloat3(0, 0, 0);
}

void Script_AI::Update() {
    if (!playerObject || !rigidBody) return;

    Float3 aiPos = GetCurrentPosition();
    Float3 playerPos;
    GetObjectPosition(playerObject, &playerPos);

    float distanceToPlayer = GetDistance(aiPos, playerPos);

    if (distanceToPlayer <= detectionRange) {
        MoveTowardsPlayer();

        // 行き詰まり検出
        float movedDistance = GetDistance(aiPos, lastPosition);
        if (movedDistance < 0.05f) {
            stuckTimer += 0.016f;
        }
        else {
            stuckTimer = 0.0f;
        }

        lastPosition = aiPos;
    }
    else {
        currentState = AIState::Idle;
        stuckTimer = 0.0f;
        avoidanceTimer = 0.0f;
    }
}

void Script_AI::EndPlay() {
}

void Script_AI::MoveTowardsPlayer() {
    if (!rigidBody) return;

    // 現在のトランスフォームを取得
    transform currentTransform;
    GetObjectTransform(aiObject, &currentTransform);

    Float3 directionToPlayer = GetDirectionToPlayer();

    // プレイヤーまでの距離をチェック
    float distanceToPlayer = sqrtf(
        directionToPlayer.x * directionToPlayer.x +
        directionToPlayer.z * directionToPlayer.z
    );

    // プレイヤーに近すぎる場合は停止
    if (distanceToPlayer < 1.5f) {
        currentState = AIState::Idle;
        avoidanceTimer = 0.0f;
        return;
    }

    Float3 moveDirection = directionToPlayer;

    // 回避モード中かチェック
    if (avoidanceTimer > 0.0f) {
        moveDirection = currentAvoidanceDirection;
        avoidanceTimer -= 0.016f;
        currentState = AIState::Avoiding;
    }
    else {
        // 前方に障害物があるかチェック
        bool hasObstacle = CheckObstacle(directionToPlayer, avoidanceDistance);

        if (hasObstacle || stuckTimer > stuckThreshold) {
            currentAvoidanceDirection = FindAvoidanceDirection();
            moveDirection = currentAvoidanceDirection;
            avoidanceTimer = avoidanceDuration;
            currentState = AIState::Avoiding;
            stuckTimer = 0.0f;
        }
        else {
            currentState = AIState::Moving;
        }
    }

    // 正規化
    moveDirection = NormalizeVector(moveDirection);

    // 新しい位置を計算
    Float3 newPos = CreateFloat3(
        currentTransform.position.x + moveDirection.x * moveSpeed * 0.016f,
        currentTransform.position.y,
        currentTransform.position.z + moveDirection.z * moveSpeed * 0.016f
    );

    // 移動先に障害物がないか最終チェック
    Float3 moveVec = CreateFloat3(
        newPos.x - currentTransform.position.x,
        0,
        newPos.z - currentTransform.position.z
    );

    if (!CheckObstacle(moveVec, 1.0f)) {
        // 目標回転角度を計算（Y軸回転のみ）
        float targetRotationY = atan2f(moveDirection.x, moveDirection.z);

        // 現在の回転角度
        float currentRotationY = currentTransform.rotation.y;

        // 角度の差を計算（-π～πの範囲に正規化）
        float rotationDiff = targetRotationY - currentRotationY;
        while (rotationDiff > 3.14159f) rotationDiff -= 6.28318f;
        while (rotationDiff < -3.14159f) rotationDiff += 6.28318f;

        // 回転を補間（スムーズに回転）
        float rotationStep = rotationSpeed * 0.016f;
        float newRotationY;

        if (fabsf(rotationDiff) < rotationStep) {
            // 目標角度に近い場合は直接設定
            newRotationY = targetRotationY;
        }
        else {
            // ゆっくり回転
            newRotationY = currentRotationY + (rotationDiff > 0 ? rotationStep : -rotationStep);
        }

        // 新しいトランスフォームを設定
        transform newTransform;
        newTransform.position = newPos;
        newTransform.rotation = CreateFloat3(
            currentTransform.rotation.x,
            newRotationY,
            currentTransform.rotation.z
        );
        newTransform.scale = currentTransform.scale;

        SetObjectTransform(aiObject, &newTransform);
    }
    else {
        // 障害物がある場合は回避モードに強制移行
        if (avoidanceTimer <= 0.0f) {
            currentAvoidanceDirection = FindAvoidanceDirection();
            avoidanceTimer = avoidanceDuration;
        }
    }
}

bool Script_AI::CheckObstacle(Float3 direction, float distance) {
    Float3 currentPos = GetCurrentPosition();

    // 複数の高さでレイキャストをチェック
    float heights[] = { 0.5f, 1.0f };

    Float3 normalizedDir = NormalizeVector(direction);

    for (int i = 0; i < 2; i++) {
        Float3 rayOrigin = CreateFloat3(
            currentPos.x,
            currentPos.y + heights[i],
            currentPos.z
        );

        RayHit hit;
        APIResult result = RaycastIgnoreTriggers(
            currentScene, rayOrigin, normalizedDir, distance, &hit
        );

        if (result == PN_SUCCESS && hit.bHit) {
            char hitName[64];
            char aiName[64];
            GetObjectName(hit.hitObject, hitName, 64);
            GetObjectName(aiObject, aiName, 64);

            // 自分自身とプレイヤーは無視
            if (strcmp(hitName, aiName) != 0 && hit.hitObject != playerObject) {
                return true;
            }
        }
    }

    return false;
}

Float3 Script_AI::FindAvoidanceDirection() {
    Float3 directionToPlayer = GetDirectionToPlayer();

    // 8方向をチェック
    const int numDirections = 8;
    float angleStep = 3.14159f * 2.0f / numDirections;

    struct DirectionCandidate {
        Float3 direction;
        float score;
    };

    DirectionCandidate candidates[16];
    int candidateCount = 0;

    // まず左右を優先的にチェック
    for (int i = 1; i <= numDirections / 2; i++) {
        for (int side = -1; side <= 1; side += 2) {
            float angle = angleStep * i * side;

            float cosAngle = cosf(angle);
            float sinAngle = sinf(angle);

            Float3 testDirection = CreateFloat3(
                directionToPlayer.x * cosAngle - directionToPlayer.z * sinAngle,
                0,
                directionToPlayer.x * sinAngle + directionToPlayer.z * cosAngle
            );

            // この方向に障害物がないかチェック
            if (!CheckObstacle(testDirection, avoidanceDistance)) {
                // スコアを計算（プレイヤー方向に近いほど高スコア）
                Float3 normTest = NormalizeVector(testDirection);
                Float3 normPlayer = NormalizeVector(directionToPlayer);
                float dotProduct = normTest.x * normPlayer.x + normTest.z * normPlayer.z;

                candidates[candidateCount].direction = testDirection;
                candidates[candidateCount].score = dotProduct;
                candidateCount++;

                if (candidateCount >= 16) break;
            }
        }
        if (candidateCount >= 16) break;
    }

    // 最もスコアの高い方向を選択
    if (candidateCount > 0) {
        int bestIndex = 0;
        for (int i = 1; i < candidateCount; i++) {
            if (candidates[i].score > candidates[bestIndex].score) {
                bestIndex = i;
            }
        }
        return candidates[bestIndex].direction;
    }

    // どうしても見つからない場合は後ろに下がる
    return CreateFloat3(-directionToPlayer.x, 0, -directionToPlayer.z);
}

Float3 Script_AI::GetDirectionToPlayer() {
    Float3 aiPos = GetCurrentPosition();
    Float3 playerPos;
    GetObjectPosition(playerObject, &playerPos);

    return CreateFloat3(
        playerPos.x - aiPos.x,
        0,
        playerPos.z - aiPos.z
    );
}

Float3 Script_AI::GetCurrentPosition() {
    Float3 pos;
    GetObjectPosition(aiObject, &pos);
    return pos;
}

float Script_AI::GetDistance(Float3 a, Float3 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

Float3 Script_AI::NormalizeVector(Float3 v) {
    float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length > 0.001f) {
        return CreateFloat3(v.x / length, v.y / length, v.z / length);
    }
    return CreateFloat3(0, 0, 0);
}