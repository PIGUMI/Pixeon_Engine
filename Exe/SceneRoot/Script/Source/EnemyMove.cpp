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
    rotationSpeed = 3.0f;

    stuckTimer = 0.0f;
    stuckThreshold = 1.5f;

    avoidanceTimer = 0.0f;
    avoidanceDuration = 1.5f;
    currentAvoidanceDirection = CreateFloat3(0, 0, 0);

    wanderTimer = 0.0f;
    wanderDuration = 3.0f;
    wanderDirection = GetRandomDirection();

    idleTimer = 0.0f;
    idleDuration = 2.0f;

    desiredDirection = CreateFloat3(0, 0, 1);
    smoothedDirection = CreateFloat3(0, 0, 1);
    directionSmoothSpeed = 2.0f;

    directionLockTimer = 0.0f;
    directionLockDuration = 0.5f;
    lockedDirection = CreateFloat3(0, 0, 1);

    obstacleCheckTimer = 0.0f;
    obstacleCheckInterval = 0.3f;
    lastObstacleCheck = false;

    currentState = AIState::Wandering;
    previousState = AIState::Wandering;
    lastPosition = GetCurrentPosition();

    srand(static_cast<unsigned int>(time(nullptr)));
}

void Script_EnemyMove::Update(float DeltaTime) {
    IScript::Update(DeltaTime);

    if (!aiObject) return;

    Float3 aiPos = GetCurrentPosition();

    if(aiPos.y < -10.0f) {
		RemoveObjectFromScene(currentScene, aiObject);
	}

    float distanceToPlayer = 0.0f;
    bool playerDetected = false;

    if (playerObject) {
        Float3 playerPos;
        GetObjectPosition(playerObject, &playerPos);
        distanceToPlayer = GetDistance(aiPos, playerPos);
        playerDetected = (distanceToPlayer <= detectionRange);
    }

    if (directionLockTimer > 0.0f) {
        directionLockTimer -= DeltaTime;
    }

    if (playerDetected) {
        UpdateChasing(DeltaTime);
    }
    else {
        UpdateWandering(DeltaTime);
    }

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

        lockedDirection = currentAvoidanceDirection;
        directionLockTimer = directionLockDuration;
    }

    lastPosition = aiPos;
}

void Script_EnemyMove::UpdateWandering(float deltaTime) {
    if (avoidanceTimer > 0.0f) {

        desiredDirection = lockedDirection;
        avoidanceTimer -= deltaTime;
        currentState = AIState::Avoiding;
    }
    else if (currentState == AIState::Idle) {
        idleTimer += deltaTime;
        if (idleTimer >= idleDuration) {
            currentState = AIState::Wandering;
            wanderDirection = GetRandomDirection();
            wanderTimer = 0.0f;
            idleTimer = 0.0f;

            lockedDirection = wanderDirection;
            directionLockTimer = directionLockDuration;
        }
        return;
    }
    else {
        previousState = currentState;
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

                lockedDirection = wanderDirection;
                directionLockTimer = directionLockDuration;
            }
            return;
        }

        if (directionLockTimer <= 0.0f) {
            obstacleCheckTimer += deltaTime;
            if (obstacleCheckTimer >= obstacleCheckInterval) {
                lastObstacleCheck = CheckObstacle(wanderDirection, avoidanceDistance);
                obstacleCheckTimer = 0.0f;
            }

            if (lastObstacleCheck && avoidanceTimer <= 0.0f) {
                currentAvoidanceDirection = FindAvoidanceDirection();
                avoidanceTimer = avoidanceDuration;
                currentState = AIState::Avoiding;

                lockedDirection = currentAvoidanceDirection;
                directionLockTimer = directionLockDuration;
                return;
            }
        }

        desiredDirection = wanderDirection;
    }

    smoothedDirection = SmoothDamp(smoothedDirection, desiredDirection, deltaTime);
    MoveInDirection(smoothedDirection);
}

void Script_EnemyMove::UpdateChasing(float deltaTime) {
    if (avoidanceTimer > 0.0f) {
        desiredDirection = lockedDirection;
        avoidanceTimer -= deltaTime;
        currentState = AIState::Avoiding;
    }
    else {
        previousState = currentState;
        currentState = AIState::Chasing;

        Float3 directionToPlayer = GetDirectionToPlayer();
        float distanceToPlayer = sqrtf(
            directionToPlayer.x * directionToPlayer.x +
            directionToPlayer.z * directionToPlayer.z
        );

        if (distanceToPlayer < 1.5f) {
            currentState = AIState::Idle;
            return;
        }

        if (directionLockTimer <= 0.0f) {
            obstacleCheckTimer += deltaTime;
            if (obstacleCheckTimer >= obstacleCheckInterval) {
                lastObstacleCheck = CheckObstacle(directionToPlayer, avoidanceDistance);
                obstacleCheckTimer = 0.0f;
            }

            if (lastObstacleCheck && avoidanceTimer <= 0.0f) {
                currentAvoidanceDirection = FindAvoidanceDirection();
                avoidanceTimer = avoidanceDuration;
                currentState = AIState::Avoiding;

                lockedDirection = currentAvoidanceDirection;
                directionLockTimer = directionLockDuration;
                return;
            }
        }

        desiredDirection = directionToPlayer;
    }

    smoothedDirection = SmoothDamp(smoothedDirection, desiredDirection, deltaTime);
    MoveInDirection(smoothedDirection);
}

void Script_EnemyMove::MoveTowardsPlayer() {

}

void Script_EnemyMove::MoveInDirection(Float3 direction) {
    transform currentTransform;
    GetObjectTransform(aiObject, &currentTransform);

    direction = NormalizeVector(direction);

    float dirLength = sqrtf(direction.x * direction.x + direction.z * direction.z);
    if (dirLength < 0.01f) {
        return;
    }

    Float3 newPos = CreateFloat3(
        currentTransform.position.x + direction.x * moveSpeed * 0.016f,
        currentTransform.position.y,
        currentTransform.position.z + direction.z * moveSpeed * 0.016f
    );

    float targetRotationY = atan2f(direction.x, direction.z);
    float currentRotationY = currentTransform.rotation.y;

    float rotationDiff = targetRotationY - currentRotationY;
    while (rotationDiff > 3.14159f) rotationDiff -= 6.28318f;
    while (rotationDiff < -3.14159f) rotationDiff += 6.28318f;

    float actualRotationSpeed = rotationSpeed;
    if (currentState == AIState::Avoiding) {
        actualRotationSpeed *= 0.5f;
    }
    else if (currentState == AIState::Chasing) {
        actualRotationSpeed *= 0.8f;
    }

    float rotationStep = actualRotationSpeed * 0.016f;
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

    const int numDirections = 12;
    float angleStep = 3.14159f * 2.0f / numDirections;

    struct DirectionCandidate {
        Float3 direction;
        float score;
        float clearDistance;
    };

    DirectionCandidate candidates[24];
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

            float checkDistance = avoidanceDistance * 2.0f;

            if (!CheckObstacle(testDirection, checkDistance)) {
                Float3 normTest = NormalizeVector(testDirection);
                Float3 normCurrent = NormalizeVector(currentDir);
                float dotProduct = normTest.x * normCurrent.x + normTest.z * normCurrent.z;

                float angleScore = dotProduct * 2.0f;
                float proximityScore = 1.0f / (float)(i + 1);

                candidates[candidateCount].direction = testDirection;
                candidates[candidateCount].score = angleScore + proximityScore;
                candidates[candidateCount].clearDistance = checkDistance;
                candidateCount++;

                if (candidateCount >= 24) break;
            }
        }
        if (candidateCount >= 24) break;
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

    Float3 normCurrent = NormalizeVector(currentDir);
    return CreateFloat3(-normCurrent.x, 0, -normCurrent.z);
}

Float3 Script_EnemyMove::LerpDirection(Float3 from, Float3 to, float t) {
    if (t >= 1.0f) return to;
    if (t <= 0.0f) return from;

    Float3 result = CreateFloat3(
        from.x + (to.x - from.x) * t,
        from.y + (to.y - from.y) * t,
        from.z + (to.z - from.z) * t
    );

    return NormalizeVector(result);
}

Float3 Script_EnemyMove::SmoothDamp(Float3 current, Float3 target, float deltaTime) {
    // Exponential smoothing ‚ðŽg—p
    float smoothFactor = 1.0f - expf(-directionSmoothSpeed * deltaTime);

    Float3 result = CreateFloat3(
        current.x + (target.x - current.x) * smoothFactor,
        current.y + (target.y - current.y) * smoothFactor,
        current.z + (target.z - current.z) * smoothFactor
    );

    return NormalizeVector(result);
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