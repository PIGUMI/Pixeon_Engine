#pragma once
#include "Include/IScript.h"

class Script_EnemyMove : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;

private:
    enum class AIState {
        Idle,           // 待機
        Wandering,      // ランダム徘徊
        Chasing,        // プレイヤー追跡
        Avoiding        // 障害物回避
    };

    // オブジェクトとコンポーネント
    Object aiObject;
    Object playerObject;
    Scene currentScene;

    // AI設定パラメータ
    float detectionRange;        // プレイヤー検出範囲
    float moveSpeed;             // 移動速度
    float avoidanceDistance;     // 障害物検出距離
    float rotationSpeed;         // 回転速度

    // スタック検出用
    float stuckTimer;            // スタック判定タイマー
    float stuckThreshold;        // スタック判定時間
    Float3 lastPosition;         // 前フレームの位置

    // 回避システム用
    float avoidanceTimer;        // 回避継続タイマー
    float avoidanceDuration;     // 回避を継続する時間
    Float3 currentAvoidanceDirection;  // 現在の回避方向

    // ランダム徘徊用
    float wanderTimer;           // 徘徊方向変更タイマー
    float wanderDuration;        // 同じ方向に歩く時間
    Float3 wanderDirection;      // 現在の徘徊方向
    float idleTimer;             // 待機タイマー
    float idleDuration;          // 待機時間

    AIState currentState;

    void UpdateWandering(float deltaTime);
    void UpdateChasing(float deltaTime);
    void MoveTowardsPlayer();
    void MoveInDirection(Float3 direction);

    bool CheckObstacle(Float3 direction, float distance);
    Float3 FindAvoidanceDirection();
    Float3 GetRandomDirection();
    Float3 GetDirectionToPlayer();
    Float3 GetCurrentPosition();

    float GetDistance(Float3 a, Float3 b);
    Float3 NormalizeVector(Float3 v);
    float RandomFloat(float min, float max);
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_EnemyMove();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}