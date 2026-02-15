#include "Enemy.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void Script_Enemy::BeginPlay() {
    IScript::BeginPlay();

    playerObject = _parentScene->FindObject("Player");
    animationComponent = _parentObject->GetComponent<Animation>("Animation");

    currentState = EnemyState::Idle;
    currentAnimState = AnimState::Idle;
    stateTimer = 0.0f;
    stuckTimer = 0.0f;
    lastPosition = _parentObject->GetPosition();
    stuckCheckPosition = lastPosition;

    if (animationComponent) {
        animationComponent->SetClip(0);
        animationComponent->Play();
    }
}

void Script_Enemy::Update(float DeltaTime) {
    IScript::Update(DeltaTime);

    if (!playerObject) {
        return;
    }

    CheckAndResolveStuck(DeltaTime);

    if (attackCooldown > 0.0f) {
        attackCooldown -= DeltaTime;
    }

    switch (currentState) {
    case EnemyState::Idle:
        UpdateIdle(DeltaTime);
        break;
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

void Script_Enemy::SetAnimation(AnimState newAnimState) {
    if (currentAnimState == newAnimState || !animationComponent) {
        return;
    }

    currentAnimState = newAnimState;

    switch (newAnimState) {
    case AnimState::Idle:
        animationComponent->SetClip(0);
        break;
    case AnimState::Move:
        animationComponent->SetClip(1);
        break;
    }
}

void Script_Enemy::SetNewRandomTarget() {
    float randomAngle = (rand() % 628) * 0.01f;
    DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
    targetPosition = {
        myPos.x + cosf(randomAngle) * 10.0f,
        myPos.y,
        myPos.z + sinf(randomAngle) * 10.0f
    };
}

bool Script_Enemy::IsPlayerVisible(DirectX::XMFLOAT3& outPlayerPos) {
    DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
    myPos.y += 1.0f;

    DirectX::XMFLOAT3 forward = GetForwardVector();
    float currentRange = (currentState == EnemyState::Chase) ? chaseRange : detectionRange;

    float angles[] = { -visionAngle, 0.0f, visionAngle };

    for (float angleOffset : angles) {
        float cosA = cosf(angleOffset);
        float sinA = sinf(angleOffset);

        DirectX::XMFLOAT3 rayDir = {
            forward.x * cosA - forward.z * sinA,
            0.0f,
            forward.x * sinA + forward.z * cosA
        };

        float len = sqrtf(rayDir.x * rayDir.x + rayDir.z * rayDir.z);
        if (len > 0.001f) {
            rayDir.x /= len;
            rayDir.z /= len;
        }

        RayHit hit;
        if (_parentScene->Raycast(myPos, rayDir, currentRange, &hit)) {
            if (hit.bHit && std::string(hit.hitObjectName) == "Player") {
                outPlayerPos = playerObject->GetPosition();
                return true;
            }
        }
    }

    return false;
}

bool Script_Enemy::CheckObstacleInDirection(const DirectX::XMFLOAT3& direction, float distance) {
    DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
    myPos.y += 0.5f;

    float len = sqrtf(direction.x * direction.x + direction.z * direction.z);
    if (len < 0.001f) return false;

    DirectX::XMFLOAT3 dir = { direction.x / len, direction.y, direction.z / len };

    RayHit hit;
    return _parentScene->RaycastIgnoreTriggers(myPos, dir, distance, &hit) && hit.bHit;
}

DirectX::XMFLOAT3 Script_Enemy::GetAvoidanceDirection() {
    DirectX::XMFLOAT3 forward = GetForwardVector();
    DirectX::XMFLOAT3 right = GetRightVector();

    if (!CheckObstacleInDirection(forward, obstacleCheckDistance)) {
        return forward;
    }

    const float cos45 = 0.707f;
    DirectX::XMFLOAT3 forwardRight = {
        forward.x * cos45 + right.x * cos45,
        0.0f,
        forward.z * cos45 + right.z * cos45
    };
    if (!CheckObstacleInDirection(forwardRight, obstacleCheckDistance)) {
        return forwardRight;
    }

    DirectX::XMFLOAT3 forwardLeft = {
        forward.x * cos45 - right.x * cos45,
        0.0f,
        forward.z * cos45 - right.z * cos45
    };
    if (!CheckObstacleInDirection(forwardLeft, obstacleCheckDistance)) {
        return forwardLeft;
    }

    if (!CheckObstacleInDirection(right, obstacleCheckDistance)) {
        return right;
    }

    DirectX::XMFLOAT3 left = { -right.x, 0.0f, -right.z };
    if (!CheckObstacleInDirection(left, obstacleCheckDistance)) {
        return left;
    }

    return { 0.0f, 0.0f, 0.0f };
}

void Script_Enemy::SmoothRotateToTarget(const DirectX::XMFLOAT3& targetPos, float deltaTime) {
    DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();

    float dx = targetPos.x - myPos.x;
    float dz = targetPos.z - myPos.z;

    if (dx * dx + dz * dz < 0.01f) {
        isRotating = false;
        return;
    }

    DirectX::XMFLOAT3 myRot = _parentObject->GetRotation();
    float targetYaw = atan2f(dx, dz);
    float angleDiff = targetYaw - myRot.y;

    while (angleDiff > M_PI) angleDiff -= 2.0f * M_PI;
    while (angleDiff < -M_PI) angleDiff += 2.0f * M_PI;

    if (std::abs(angleDiff) < 0.05f) {
        myRot.y = targetYaw;
        isRotating = false;
    }
    else {
        isRotating = true;
        float maxRotation = rotationSpeed * deltaTime;
        float rotation = (std::abs(angleDiff) < maxRotation) ? angleDiff :
            (angleDiff < 0 ? -maxRotation : maxRotation);
        myRot.y += rotation;
    }

    myRot.y = NormalizeAngle(myRot.y);
    _parentObject->SetRotation(myRot);
}

void Script_Enemy::MoveInDirection(const DirectX::XMFLOAT3& direction, float deltaTime) {
    float len = sqrtf(direction.x * direction.x + direction.z * direction.z);
    if (len < 0.001f) return;

    DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
    myPos.x += (direction.x / len) * moveSpeed * deltaTime;
    myPos.z += (direction.z / len) * moveSpeed * deltaTime;
    _parentObject->SetPosition(myPos);
}

void Script_Enemy::CheckAndResolveStuck(float deltaTime) {
    DirectX::XMFLOAT3 currentPos = _parentObject->GetPosition();

    if (currentState == EnemyState::Patrol || currentState == EnemyState::Chase) {
        stuckTimer += deltaTime;

        if (stuckTimer >= 0.5f) {
            float dx = currentPos.x - stuckCheckPosition.x;
            float dz = currentPos.z - stuckCheckPosition.z;
            float movedDistance = sqrtf(dx * dx + dz * dz);

            if (movedDistance < 0.5f) {
                currentState = EnemyState::Idle;
                stateTimer = 0.0f;
                stuckTimer = 0.0f;
                stuckCheckPosition = currentPos;
            }
            else {
                stuckCheckPosition = currentPos;
                stuckTimer = 0.0f;
            }
        }
    }
    else {
        stuckTimer = 0.0f;
        stuckCheckPosition = currentPos;
    }

    lastPosition = currentPos;
}

void Script_Enemy::UpdateIdle(float deltaTime) {
    SetAnimation(AnimState::Idle);
    stateTimer += deltaTime;

    DirectX::XMFLOAT3 playerPos;
    if (IsPlayerVisible(playerPos)) {
        currentState = EnemyState::Chase;
        lastKnownPlayerPosition = playerPos;
        stateTimer = 0.0f;
        stuckTimer = 0.0f;
        stuckCheckPosition = _parentObject->GetPosition();
        return;
    }

    if (stateTimer >= idleTime) {
        currentState = EnemyState::Patrol;
        stateTimer = 0.0f;
        stuckTimer = 0.0f;
        stuckCheckPosition = _parentObject->GetPosition();
        SetNewRandomTarget();
    }
}

void Script_Enemy::UpdatePatrol(float deltaTime) {
    DirectX::XMFLOAT3 playerPos;
    if (IsPlayerVisible(playerPos)) {
        currentState = EnemyState::Chase;
        lastKnownPlayerPosition = playerPos;
        stateTimer = 0.0f;
        stuckTimer = 0.0f;
        stuckCheckPosition = _parentObject->GetPosition();
        return;
    }

    SmoothRotateToTarget(targetPosition, deltaTime);

    if (isRotating) {
        SetAnimation(AnimState::Idle);
        return;
    }

    DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
    float dx = targetPosition.x - myPos.x;
    float dz = targetPosition.z - myPos.z;
    float distToTarget = sqrtf(dx * dx + dz * dz);

    if (distToTarget < 1.0f) {
        currentState = EnemyState::Idle;
        stateTimer = 0.0f;
        stuckTimer = 0.0f;
        stuckCheckPosition = myPos;
        return;
    }

    DirectX::XMFLOAT3 moveDir = GetAvoidanceDirection();

    if (moveDir.x == 0.0f && moveDir.z == 0.0f) {
        currentState = EnemyState::Idle;
        stateTimer = 0.0f;
        stuckTimer = 0.0f;
        stuckCheckPosition = myPos;
        return;
    }

    SetAnimation(AnimState::Move);
    MoveInDirection(moveDir, deltaTime);
}

void Script_Enemy::UpdateChase(float deltaTime) {
    DirectX::XMFLOAT3 playerPos;
    bool canSeePlayer = IsPlayerVisible(playerPos);

    if (canSeePlayer) {
        lastKnownPlayerPosition = playerPos;

        DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
        float dx = playerPos.x - myPos.x;
        float dz = playerPos.z - myPos.z;
        float distToPlayer = sqrtf(dx * dx + dz * dz);

        if (distToPlayer <= attackRange) {
            currentState = EnemyState::Attack;
            stateTimer = 0.0f;
            stuckTimer = 0.0f;
            stuckCheckPosition = myPos;
            return;
        }

        SmoothRotateToTarget(playerPos, deltaTime);

        if (!isRotating) {
            DirectX::XMFLOAT3 moveDir = GetAvoidanceDirection();

            if (moveDir.x == 0.0f && moveDir.z == 0.0f) {
                currentState = EnemyState::Idle;
                stateTimer = 0.0f;
                stuckTimer = 0.0f;
                stuckCheckPosition = myPos;
                return;
            }

            SetAnimation(AnimState::Move);
            MoveInDirection(moveDir, deltaTime);
        }
        else {
            SetAnimation(AnimState::Idle);
        }
    }
    else {
        DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
        float dx = lastKnownPlayerPosition.x - myPos.x;
        float dz = lastKnownPlayerPosition.z - myPos.z;
        float distToLastKnown = sqrtf(dx * dx + dz * dz);

        if (distToLastKnown > 1.0f) {
            SmoothRotateToTarget(lastKnownPlayerPosition, deltaTime);

            if (!isRotating) {
                DirectX::XMFLOAT3 moveDir = GetAvoidanceDirection();

                if (moveDir.x == 0.0f && moveDir.z == 0.0f) {
                    currentState = EnemyState::Idle;
                    stateTimer = 0.0f;
                    stuckTimer = 0.0f;
                    stuckCheckPosition = myPos;
                    return;
                }

                SetAnimation(AnimState::Move);
                MoveInDirection(moveDir, deltaTime);
            }
            else {
                SetAnimation(AnimState::Idle);
            }
        }
        else {
            currentState = EnemyState::Search;
            stateTimer = 0.0f;
            stuckTimer = 0.0f;
            stuckCheckPosition = myPos;
        }
    }
}

void Script_Enemy::UpdateSearch(float deltaTime) {
    SetAnimation(AnimState::Idle);
    stateTimer += deltaTime;

    DirectX::XMFLOAT3 playerPos;
    if (IsPlayerVisible(playerPos)) {
        currentState = EnemyState::Chase;
        lastKnownPlayerPosition = playerPos;
        stateTimer = 0.0f;
        stuckTimer = 0.0f;
        stuckCheckPosition = _parentObject->GetPosition();
        return;
    }

    DirectX::XMFLOAT3 myRot = _parentObject->GetRotation();
    myRot.y += rotationSpeed * 0.3f * deltaTime;
    myRot.y = NormalizeAngle(myRot.y);
    _parentObject->SetRotation(myRot);

    if (stateTimer >= searchTime) {
        currentState = EnemyState::Idle;
        stateTimer = 0.0f;
        stuckTimer = 0.0f;
        stuckCheckPosition = _parentObject->GetPosition();
    }
}

void Script_Enemy::UpdateAttack(float deltaTime) {
    SetAnimation(AnimState::Idle);
    DirectX::XMFLOAT3 playerPos;

    if (IsPlayerVisible(playerPos)) {
        DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
        float dx = playerPos.x - myPos.x;
        float dz = playerPos.z - myPos.z;
        float distToPlayer = sqrtf(dx * dx + dz * dz);

        SmoothRotateToTarget(playerPos, deltaTime);

        if (distToPlayer > attackRange * 1.3f) {
            currentState = EnemyState::Chase;
            stuckTimer = 0.0f;
            stuckCheckPosition = myPos;
            return;
        }

        if (attackCooldown <= 0.0f && !isRotating) {
            attackCooldown = 2.0f;
        }
    }
    else {
        currentState = EnemyState::Search;
        stateTimer = 0.0f;
        stuckTimer = 0.0f;
        stuckCheckPosition = _parentObject->GetPosition();
    }
}

DirectX::XMFLOAT3 Script_Enemy::GetForwardVector() {
    DirectX::XMFLOAT3 rot = _parentObject->GetRotation();
    return { sinf(rot.y), 0.0f, cosf(rot.y) };
}

DirectX::XMFLOAT3 Script_Enemy::GetRightVector() {
    DirectX::XMFLOAT3 rot = _parentObject->GetRotation();
    return { cosf(rot.y), 0.0f, -sinf(rot.y) };
}

DirectX::XMFLOAT3 Script_Enemy::NormalizeVector(const DirectX::XMFLOAT3& v) {
    float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length < 0.001f) {
        return { 0.0f, 0.0f, 0.0f };
    }
    return { v.x / length, v.y / length, v.z / length };
}

float Script_Enemy::VectorLength(const DirectX::XMFLOAT3& v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

float Script_Enemy::NormalizeAngle(float angle) {
    while (angle > M_PI) angle -= 2.0f * M_PI;
    while (angle < -M_PI) angle += 2.0f * M_PI;
    return angle;
}