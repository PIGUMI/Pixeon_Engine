#pragma once
#include "Include/IScript.h"

class Script_Enemy : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;

private:
    // AI状態
    enum class EnemyState {
        Patrol,      // 巡回
        Chase,       // 追跡
        Search,      // 探索
        Attack       // 攻撃
    };

    // 視界判定用レイキャスト
    bool CheckVisionRaycast(const DirectX::XMFLOAT3& direction, float distance, RayHit& outHit);
    bool IsPlayerInSight(float& outDistance);

    // 障害物回避
    DirectX::XMFLOAT3 AvoidObstacles();

    // 移動処理
    void MoveTowards(const DirectX::XMFLOAT3& targetPosition, float deltaTime);
    void RotateTowards(const DirectX::XMFLOAT3& targetPosition, float deltaTime);

    // 状態別更新処理
    void UpdatePatrol(float deltaTime);
    void UpdateChase(float deltaTime);
    void UpdateSearch(float deltaTime);
    void UpdateAttack(float deltaTime);

    // ヘルパー関数
    float GetDistanceToPlayer();
    DirectX::XMFLOAT3 NormalizeVector(const DirectX::XMFLOAT3& v);
    float VectorLength(const DirectX::XMFLOAT3& v);
    DirectX::XMFLOAT3 RotateVectorY(const DirectX::XMFLOAT3& v, float angleDeg);

public:
#define PROPERTY_LIST(ACTION) \
    ACTION(FLOAT, detectionRange, 5.0f, 30.0f) \
    ACTION(FLOAT, chaseRange, 10.0f, 50.0f) \
    ACTION(FLOAT, moveSpeed, 0.5f, 5.0f) \
    ACTION(FLOAT, rotationSpeed, 30.0f, 180.0f) \
    ACTION(FLOAT, attackRange, 0.5f, 3.0f) \
    ACTION(FLOAT, searchTime, 3.0f, 10.0f) \
    ACTION(FLOAT, visionAngle, 30.0f, 90.0f) \
    ACTION(FLOAT, obstacleAvoidDistance, 1.0f, 5.0f)

    DECLARE_SCRIPT_PROPERTIES()
#undef PROPERTY_LIST

private:
    // プロパティ変数
    float detectionRange = 10.0f;      // 初期視界範囲
    float chaseRange = 20.0f;          // 追跡時の視界範囲
    float moveSpeed = 2.0f;            // 移動速度
    float rotationSpeed = 90.0f;       // 回転速度（度/秒）
    float attackRange = 1.5f;          // 攻撃範囲
    float searchTime = 5.0f;           // 探索時間
    float visionAngle = 45.0f;         // 視界の角度
    float obstacleAvoidDistance = 2.0f; // 障害物回避距離

    // 内部状態
    EnemyState currentState = EnemyState::Patrol;
    Object* playerObject = nullptr;
    DirectX::XMFLOAT3 lastKnownPlayerPosition = { 0.0f, 0.0f, 0.0f };
    float searchTimer = 0.0f;
    float patrolTimer = 0.0f;
    DirectX::XMFLOAT3 patrolDirection = { 1.0f, 0.0f, 0.0f };
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Enemy();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}