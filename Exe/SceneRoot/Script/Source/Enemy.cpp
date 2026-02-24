/*
* Enemy AI Script
*/

#include "Enemy.h"
#include <cmath>
#include <algorithm>
#include <Windows.h>


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void Script_Enemy::BeginPlay() {
    IScript::BeginPlay();

    playerObject = _parentScene->FindObject("Player");
    animationComponent = _parentObject->GetComponent<Animation>("Animation");
	rigidbodyComponent = _parentObject->GetComponent<Rigidbody>("RigidBody");

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
    case EnemyState::Dead :
		SetAnimation(AnimState::Dead);
        break;
    }
}

void Script_Enemy::EndPlay() {
    IScript::EndPlay();
}

void Script_Enemy::CallCustom(const std::string& functionName)
{
    if(functionName == "Die") {
        currentState = EnemyState::Dead;
	}    
}

/*
* 関数名： SetAnimation
* 引　数： newAnimState - 新しいアニメーション状態
* 戻り値：なし
* 説　明： アニメーション状態を切り替える関数。現在の状態と同じ場合やアニメーションコンポーネントがない場合は何もしない。
*/
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
	case AnimState::Dead:
        _parentObject->RemoveComponent(rigidbodyComponent);
        animationComponent->SetLoop(false);
        animationComponent->SetClip(2);
		animationComponent->Play();
		break;
    }
}

/*
* 関数名： SetNewRandomTarget
* 引　数：なし
* 戻り値：なし
* 説　明： パトロール状態で新しいランダムな目的地を設定する関数。現在の位置から半径10の範囲内でランダムな点を生成する。
*/
void Script_Enemy::SetNewRandomTarget() {
    float randomAngle = (rand() % 628) * 0.01f;
    DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
    targetPosition = {
        myPos.x + cosf(randomAngle) * 10.0f,
        myPos.y,
        myPos.z + sinf(randomAngle) * 10.0f
    };
}

/*
* 関数名： IsPlayerVisible
* 引　数：outPlayerPos - プレイヤーの位置を出力する参照変数
* 戻り値：プレイヤーが視界内にいる場合はtrue、そうでない場合はfalse
* 説　明： プレイヤーが敵の視界内にいるかどうかを判定する関数。敵の前方に複数のレイを飛ばして、プレイヤーが見えるかどうかをチェックする。
*/
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

/*
* 関数名： CheckObstacleInDirection
* 引　数：direction - チェックする方向のベクトル、distance - チェックする距離
* 戻り値：指定した方向に障害物がある場合はtrue、そうでない場合はfalse
* 説　明： 指定した方向に障害物があるかどうかを判定する関数。敵の位置から指定した距離までレイを飛ばして、障害物があるかどうかをチェックする。
*/
bool Script_Enemy::CheckObstacleInDirection(const DirectX::XMFLOAT3& direction, float distance) {
    DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
    myPos.y += 0.5f;

    float len = sqrtf(direction.x * direction.x + direction.z * direction.z);
    if (len < 0.001f) return false;

    DirectX::XMFLOAT3 dir = { direction.x / len, direction.y, direction.z / len };

    RayHit hit;
    return _parentScene->RaycastIgnoreTriggers(myPos, dir, distance, &hit) && hit.bHit;
}

/*
* 関数名： GetAvoidanceDirection
* 引　数：なし
* 戻り値：障害物を回避するための移動方向のベクトル
* 説　明： 障害物を回避するための移動方向を計算する関数。
*/
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

/*
* 関数名： SmoothRotateToTarget
* 引　数：targetPos - 回転して向くべき目標位置、deltaTime - 前回のフレームからの経過時間
* 戻り値：なし
* 説　明： 目標位置に向かってスムーズに回転する関数。現在の回転と目標位置への角度を計算し、回転速度に基づいて少しずつ回転させる。
*/
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

    if (std::abs(angleDiff) < 0.1f) {
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

/*
* 関数名： MoveInDirection
* 引　数：direction - 移動する方向のベクトル、deltaTime - 前回のフレームからの経過時間
* 戻り値：なし
* 説　明： 指定した方向に移動する関数。移動速度と経過時間に基づいて、敵の位置を更新する。
*/
void Script_Enemy::MoveInDirection(const DirectX::XMFLOAT3& direction, float deltaTime) {
    float len = sqrtf(direction.x * direction.x + direction.z * direction.z);
    if (len < 0.001f) return;

    DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
    myPos.x += (direction.x / len) * moveSpeed * deltaTime;
    myPos.z += (direction.z / len) * moveSpeed * deltaTime;
    _parentObject->SetPosition(myPos);
}

/*
* 関数名： CheckAndResolveStuck
* 引　数：deltaTime - 前回のフレームからの経過時間
* 戻り値：なし
* 説　明： 敵が移動中にスタックしているかどうかをチェックし、スタックしている場合は状態をアイドルに切り替える関数。一定時間同じ位置にいる場合はスタックと判断する。
*/
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

/*
* 関数名： UpdateIdle
* 引　数：deltaTime - 前回のフレームからの経過時間
* 戻り値：なし
* 説　明： アイドル状態の更新関数。プレイヤーが視界内に入った場合はチェイス状態に切り替える。一定時間アイドル状態が続いた場合はパトロール状態に切り替える。
*/
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

/*
* 関数名： UpdatePatrol
* 引　数：deltaTime - 前回のフレームからの経過時間
* 戻り値：なし
* 説　明： パトロール状態の更新関数。プレイヤーが視界内に入った場合はチェイス状態に切り替える。
*/
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

    SmoothRotateToTarget(targetPosition, deltaTime);

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

/*
* 関数名： UpdateChase
* 引　数：deltaTime - 前回のフレームからの経過時間
* 戻り値：なし
* 説　明： チェイス状態の更新関数。プレイヤーが視界内にいる場合は追跡を続ける。攻撃範囲内に入った場合は攻撃状態に切り替える.
*/
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
        DirectX::XMFLOAT3 myPos = _parentObject->GetPosition();
        float dx = lastKnownPlayerPosition.x - myPos.x;
        float dz = lastKnownPlayerPosition.z - myPos.z;
        float distToLastKnown = sqrtf(dx * dx + dz * dz);

        if (distToLastKnown > 1.0f) {
            SmoothRotateToTarget(lastKnownPlayerPosition, deltaTime);

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
            currentState = EnemyState::Search;
            stateTimer = 0.0f;
            stuckTimer = 0.0f;
            stuckCheckPosition = myPos;
        }
    }
}

/*
* 関数名： UpdateSearch
* 引　数：deltaTime - 前回のフレームからの経過時間
* 戻り値：なし
* 説　明： サーチ状態の更新関数。プレイヤーが視界内に入った場合はチェイス状態に切り替える。一定時間サーチ状態が続いた場合はアイドル状態に切り替える。
*/
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

/*
* 関数名： UpdateAttack
* 引　数：deltaTime - 前回のフレームからの経過時間
* 戻り値：なし
* 説　明： アタック状態の更新関数。プレイヤーが視界内にいる場合は攻撃を続ける。視界外に出た場合はサーチ状態に切り替える。
*/
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

/*
* 関数名： GetForwardVector
* 引　数：なし
* 戻り値：敵の前方方向を表すベクトル
* 説　明： 敵の前方方向を計算して返す関数。敵の回転から前方ベクトルを計算する。
*/
DirectX::XMFLOAT3 Script_Enemy::GetForwardVector() {
    DirectX::XMFLOAT3 rot = _parentObject->GetRotation();
    return { sinf(rot.y), 0.0f, cosf(rot.y) };
}

/*
* 関数名： GetRightVector
* 引　数：なし
* 戻り値：敵の右方向を表すベクトル
* 説　明： 敵の右方向を計算して返す関数。敵の回転から右ベクトルを計算する。
*/
DirectX::XMFLOAT3 Script_Enemy::GetRightVector() {
    DirectX::XMFLOAT3 rot = _parentObject->GetRotation();
    return { cosf(rot.y), 0.0f, -sinf(rot.y) };
}

/*
* 関数名： NormalizeVector
* 引　数：v - 正規化するベクトル
* 戻り値：正規化されたベクトル
* 説　明： ベクトルを正規化して返す関数。長さが0に近い場合はゼロベクトルを返す。
*/
DirectX::XMFLOAT3 Script_Enemy::NormalizeVector(const DirectX::XMFLOAT3& v) {
    float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length < 0.001f) {
        return { 0.0f, 0.0f, 0.0f };
    }
    return { v.x / length, v.y / length, v.z / length };
}

/*
* 関数名： VectorLength
* 引　数：v - 長さを計算するベクトル
* 戻り値：ベクトルの長さ
* 説　明： ベクトルの長さを計算して返す関数。ベクトルの各成分の二乗の和の平方根を計算する。
*/
float Script_Enemy::VectorLength(const DirectX::XMFLOAT3& v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

/*
* 関数名： NormalizeAngle
* 引　数：angle - 正規化する角度（ラジアン）
* 戻り値：正規化された角度（-πからπの範囲）
* 説　明： 角度を-πからπの範囲に正規化して返す関数。角度がπを超える場合は2πを引き、-π未満の場合は2πを加えることで正規化する。
*/
float Script_Enemy::NormalizeAngle(float angle) {
    while (angle > M_PI) angle -= 2.0f * M_PI;
    while (angle < -M_PI) angle += 2.0f * M_PI;
    return angle;
}