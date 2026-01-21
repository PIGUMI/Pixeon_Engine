#pragma once
#include "Include/IScript.h"
#include <vector>

class Script_AI : public IScript {
private:
    Object aiObject;
    Object playerObject;
    Component rigidBody;
    SceneHandle currentScene;

    float detectionRange;    // プレイヤー検出範囲
    float moveSpeed;         // 移動速度
    float avoidanceDistance; // 障害物検出距離
    float rotationSpeed;     // 回転速度
    float stuckTimer;        // 行き詰まり判定タイマー
    float stuckThreshold;    // 行き詰まり判定時間
    Float3 lastPosition;     // 前フレームの位置

    float avoidanceTimer;              // 回避方向維持タイマー
    float avoidanceDuration;           // 回避方向を維持する時間
    Float3 currentAvoidanceDirection;  // 現在の回避方向

    enum class AIState {
        Idle,
        Moving,
        Avoiding
    };
    AIState currentState;

public:
    void BeginPlay() override;
    void Update() override;
    void EndPlay() override;

private:
    void MoveTowardsPlayer();
    bool CheckObstacle(Float3 direction, float distance);
    Float3 FindAvoidanceDirection();
    Float3 GetDirectionToPlayer();
    Float3 GetCurrentPosition();
    float GetDistance(Float3 a, Float3 b);
    Float3 NormalizeVector(Float3 v);
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_AI();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}