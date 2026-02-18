#pragma once
#include "Include/IScript.h"

class Script_Enemy : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;

private:
    enum class EnemyState {
        Idle,
        Patrol,
        Chase,
        Search,
        Attack,
		Dead
    };

    enum class AnimState {
        Idle,
        Move
    };

    bool IsPlayerVisible(DirectX::XMFLOAT3& outPlayerPos);
    bool CheckObstacleInDirection(const DirectX::XMFLOAT3& direction, float distance);
    DirectX::XMFLOAT3 GetAvoidanceDirection();
    void SmoothRotateToTarget(const DirectX::XMFLOAT3& targetPos, float deltaTime);
    void MoveInDirection(const DirectX::XMFLOAT3& direction, float deltaTime);
    void CheckAndResolveStuck(float deltaTime);
    void UpdateIdle(float deltaTime);
    void UpdatePatrol(float deltaTime);
    void UpdateChase(float deltaTime);
    void UpdateSearch(float deltaTime);
    void UpdateAttack(float deltaTime);
    void SetAnimation(AnimState newAnimState);
    void SetNewRandomTarget();
    DirectX::XMFLOAT3 GetForwardVector();
    DirectX::XMFLOAT3 GetRightVector();
    DirectX::XMFLOAT3 NormalizeVector(const DirectX::XMFLOAT3& v);
    float VectorLength(const DirectX::XMFLOAT3& v);
    float NormalizeAngle(float angle);

public:
#define PROPERTY_LIST(ACTION) \
    ACTION(FLOAT, detectionRange) \
    ACTION(FLOAT, chaseRange) \
    ACTION(FLOAT, moveSpeed) \
    ACTION(FLOAT, rotationSpeed) \
    ACTION(FLOAT, attackRange) \
    ACTION(FLOAT, searchTime) \
    ACTION(FLOAT, visionAngle) \
    ACTION(FLOAT, obstacleCheckDistance) \
    ACTION(FLOAT, idleTime)

    DECLARE_SCRIPT_PROPERTIES()
#undef PROPERTY_LIST

private:
    float detectionRange = 10.0f;
    float chaseRange = 20.0f;
    float moveSpeed = 1.5f;
    float rotationSpeed = 2.0f;
    float attackRange = 1.5f;
    float searchTime = 5.0f;
    float visionAngle = 0.785f;
    float obstacleCheckDistance = 1.5f;
    float idleTime = 2.0f;

    EnemyState currentState = EnemyState::Idle;
    AnimState currentAnimState = AnimState::Idle;
    Object* playerObject = nullptr;
    Animation* animationComponent = nullptr;

    DirectX::XMFLOAT3 targetPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 lastKnownPlayerPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 lastPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 stuckCheckPosition = { 0.0f, 0.0f, 0.0f };

    float stateTimer = 0.0f;
    float attackCooldown = 0.0f;
    float stuckTimer = 0.0f;

    bool isRotating = false;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Enemy();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}