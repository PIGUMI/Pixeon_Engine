#include "EnemyMove.h"
#include <cmath>
#include <cstdlib>
#include <ctime>

void Script_EnemyMove::BeginPlay() {
    IScript::BeginPlay();

    GetCurrentScene(&currentScene);
    aiObject = _parentObject;

    FindObjectByName(currentScene, "Player", &playerObject);

    detectionRange = 50.0f;
    moveSpeed = 2.5f;
    avoidanceDistance = 3.5f;
    rotationSpeed = 5.0f;

    stuckTimer = 0.0f;
    stuckThreshold = 1.0f;

    avoidanceTimer = 0.0f;
    avoidanceDuration = 0.5f;
    currentAvoidanceDirection = CreateFloat3(0, 0, 0);

    wanderTimer = 0.0f;
    wanderDuration = 3.0f;
    wanderDirection = GetRandomDirection();

    idleTimer = 0.0f;
    idleDuration = 2.0f;

    currentState = AIState::Wandering;
    lastPosition = GetCurrentPosition();

    srand(static_cast<unsigned int>(time(nullptr)));
}

void Script_EnemyMove::Update(float DeltaTime) {
    IScript::Update(DeltaTime);

    if (!aiObject) return;

    Float3 aiPos = GetCurrentPosition();

    float distanceToPlayer = 0.0f;
    bool playerDetected = false;

    if (playerObject) {
        Float3 playerPos;
        GetObjectPosition(playerObject, &playerPos);
        distanceToPlayer = GetDistance(aiPos, playerPos);
        playerDetected = (distanceToPlayer <= detectionRange);
    }

    if (playerDetected) {
        UpdateChasing(DeltaTime);
    }
    else {
        UpdateWandering(DeltaTime);
    }

    // スタック検出
    float movedDistance = GetDistance(aiPos, lastPosition);
    if (movedDistance < 0.05f && (currentState == AIState::Chasing || currentState == AIState::Wandering)) {
        stuckTimer += DeltaTime;
    }
    else {
        stuckTimer = 0.0f;
    }

    if (stuckTimer > stuckThreshold) {
        currentAvoidanceDirection = FindAvoidanceDirection();
        avoidanceTimer = avoidanceDuration;
        currentState = AIState::Avoiding;
        stuckTimer = 0.0f;
    }

    lastPosition = aiPos;
}

void Script_EnemyMove::UpdateWandering(float deltaTime) {
    if (avoidanceTimer > 0.0f) {
        MoveInDirection(currentAvoidanceDirection);
        avoidanceTimer -= deltaTime;
        currentState = AIState::Avoiding;
        return;
    }

    if (currentState == AIState::Idle) {
        idleTimer += deltaTime;
        if (idleTimer >= idleDuration) {
            currentState = AIState::Wandering;
            wanderDirection = GetRandomDirection();
            wanderTimer = 0.0f;
            idleTimer = 0.0f;
        }
        return;
    }

    currentState = AIState::Wandering;
    wanderTimer += deltaTime;

    if (wanderTimer >= wanderDuration) {

        if (RandomFloat(0.0f, 1.0f) < 0.3f) {
            currentState = AIState::Idle;
            idleTimer = 0.0f;
            idleDuration = RandomFloat(1.0f, 3.0f);
        }
        else {
            wanderDirection = GetRandomDirection();
            wanderTimer = 0.0f;
            wanderDuration = RandomFloat(2.0f, 5.0f);
        }
        return;
    }

    if (CheckObstacle(wanderDirection, avoidanceDistance)) {
        currentAvoidanceDirection = FindAvoidanceDirection();
        avoidanceTimer = avoidanceDuration;
        currentState = AIState::Avoiding;
        return;
    }

    MoveInDirection(wanderDirection);
}

void Script_EnemyMove::UpdateChasing(float deltaTime) {
    if (avoidanceTimer > 0.0f) {
        MoveInDirection(currentAvoidanceDirection);
        avoidanceTimer -= deltaTime;
        currentState = AIState::Avoiding;
        return;
    }

    currentState = AIState::Chasing;
    MoveTowardsPlayer();
}

void Script_EnemyMove::MoveTowardsPlayer() {
    if (!playerObject) return;

    transform currentTransform;
    GetObjectTransform(aiObject, &currentTransform);

    Float3 directionToPlayer = GetDirectionToPlayer();

    float distanceToPlayer = sqrtf(
        directionToPlayer.x * directionToPlayer.x +
        directionToPlayer.z * directionToPlayer.z
    );

    if (distanceToPlayer < 1.5f) {
        currentState = AIState::Idle;
        return;
    }

    Float3 moveDirection = directionToPlayer;

    if (CheckObstacle(directionToPlayer, avoidanceDistance)) {
        currentAvoidanceDirection = FindAvoidanceDirection();
        avoidanceTimer = avoidanceDuration;
        currentState = AIState::Avoiding;
        return;
    }

    MoveInDirection(moveDirection);
}

void Script_EnemyMove::MoveInDirection(Float3 direction) {
    transform currentTransform;
    GetObjectTransform(aiObject, &currentTransform);

    direction = NormalizeVector(direction);

    Float3 newPos = CreateFloat3(
        currentTransform.position.x + direction.x * moveSpeed * 0.016f,
        currentTransform.position.y,
        currentTransform.position.z + direction.z * moveSpeed * 0.016f
    );

    Float3 moveVec = CreateFloat3(
        newPos.x - currentTransform.position.x,
        0,
        newPos.z - currentTransform.position.z
    );

    if (CheckObstacle(moveVec, 1.0f)) {
        if (avoidanceTimer <= 0.0f) {
            currentAvoidanceDirection = FindAvoidanceDirection();
            avoidanceTimer = avoidanceDuration;
        }
        return;
    }

    float targetRotationY = atan2f(direction.x, direction.z);

    float currentRotationY = currentTransform.rotation.y;

    float rotationDiff = targetRotationY - currentRotationY;
    while (rotationDiff > 3.14159f) rotationDiff -= 6.28318f;
    while (rotationDiff < -3.14159f) rotationDiff += 6.28318f;

    float rotationStep = rotationSpeed * 0.016f;
    float newRotationY;

    if (fabsf(rotationDiff) < rotationStep) {
        newRotationY = targetRotationY;
    }
    else {
        newRotationY = currentRotationY + (rotationDiff > 0 ? rotationStep : -rotationStep);
    }

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

bool Script_EnemyMove::CheckObstacle(Float3 direction, float distance) {
    Float3 currentPos = GetCurrentPosition();

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

            if (strcmp(hitName, aiName) != 0 && hit.hitObject != playerObject) {
                return true;
            }
        }
    }

    return false;
}

Float3 Script_EnemyMove::FindAvoidanceDirection() {
    Float3 currentDir = (currentState == AIState::Chasing)
        ? GetDirectionToPlayer()
        : wanderDirection;

    const int numDirections = 8;
    float angleStep = 3.14159f * 2.0f / numDirections;

    struct DirectionCandidate {
        Float3 direction;
        float score;
    };

    DirectionCandidate candidates[16];
    int candidateCount = 0;

    for (int i = 1; i <= numDirections / 2; i++) {
        for (int side = -1; side <= 1; side += 2) {
            float angle = angleStep * i * side;

            float cosAngle = cosf(angle);
            float sinAngle = sinf(angle);

            Float3 testDirection = CreateFloat3(
                currentDir.x * cosAngle - currentDir.z * sinAngle,
                0,
                currentDir.x * sinAngle + currentDir.z * cosAngle
            );

            if (!CheckObstacle(testDirection, avoidanceDistance)) {
                Float3 normTest = NormalizeVector(testDirection);
                Float3 normCurrent = NormalizeVector(currentDir);
                float dotProduct = normTest.x * normCurrent.x + normTest.z * normCurrent.z;

                candidates[candidateCount].direction = testDirection;
                candidates[candidateCount].score = dotProduct;
                candidateCount++;

                if (candidateCount >= 16) break;
            }
        }
        if (candidateCount >= 16) break;
    }

    if (candidateCount > 0) {
        int bestIndex = 0;
        for (int i = 1; i < candidateCount; i++) {
            if (candidates[i].score > candidates[bestIndex].score) {
                bestIndex = i;
            }
        }
        return candidates[bestIndex].direction;
    }

    return GetRandomDirection();
}

Float3 Script_EnemyMove::GetRandomDirection() {
    float angle = RandomFloat(0.0f, 6.28318f);
    return CreateFloat3(sinf(angle), 0, cosf(angle));
}

Float3 Script_EnemyMove::GetDirectionToPlayer() {
    if (!playerObject) return CreateFloat3(0, 0, 0);

    Float3 aiPos = GetCurrentPosition();
    Float3 playerPos;
    GetObjectPosition(playerObject, &playerPos);

    return CreateFloat3(
        playerPos.x - aiPos.x,
        0,
        playerPos.z - aiPos.z
    );
}

Float3 Script_EnemyMove::GetCurrentPosition() {
    Float3 pos;
    GetObjectPosition(aiObject, &pos);
    return pos;
}

float Script_EnemyMove::GetDistance(Float3 a, Float3 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dz = b.z - a.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

Float3 Script_EnemyMove::NormalizeVector(Float3 v) {
    float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length > 0.001f) {
        return CreateFloat3(v.x / length, v.y / length, v.z / length);
    }
    return CreateFloat3(0, 0, 0);
}

float Script_EnemyMove::RandomFloat(float min, float max) {
    float random = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return min + random * (max - min);
}

void Script_EnemyMove::EndPlay() {
    IScript::EndPlay();
}