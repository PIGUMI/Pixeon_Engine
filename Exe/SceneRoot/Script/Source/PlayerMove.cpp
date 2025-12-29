#include "PlayerMove.h"
#include <Windows.h>
#include <DirectXMath.h>


void Script_PlayerMove::BeginPlay() {
	SceneHandle Temp;
	GetCurrentScene(&Temp);
	// オブジェクトの取得
	FindObjectByName(Temp, "Player", &player);
	// コンポーネントの取得
	FindComponent(player, "Animation", &Animation);
	FindComponent(player, "CameraComponent", &Camera);
	FindComponent(player, "RigidBody", &rigidBody);
	// RigidBodyの設定
    RigidBodySetMass(rigidBody, 10.0f);
    RigidBodySetFriction(rigidBody, 0.8f);
    RigidBodySetLinearDamping(rigidBody, 0.1f);
    RigidBodySetAngularDamping(rigidBody, 0.5f);
    RigidBodySetRollingFriction(rigidBody, 0.3f);
    RigidBodySetRestitution(rigidBody, 0.0f);

	// カメラの設定
	int camNum;
	APIResult rs;
	rs = GetCameraNumber(Camera, &camNum);
	SetMainCamera(camNum);
	// マウスカーソルを固定
	FixedMouseCursor(true);
}

void Script_PlayerMove::Update() {
	currentState = Idle;

	UpdateMovement();
	UpdateAnimation();
}

void Script_PlayerMove::EndPlay() {
}

void Script_PlayerMove::UpdateMovement()
{
    transform trans;
    Float3 vec;
    GetObjectTransform(player, &trans);
    GetCameraForwardVector(Camera, &vec);

    // **重要:  方向ベクトルを水平方向のみに正規化**
    Float3 horizontalDir = { vec.x, 0.0f, vec.z };

    // ベクトルの長さを計算して正規化
    float length = sqrtf(horizontalDir.x * horizontalDir.x + horizontalDir.z * horizontalDir.z);

    if (length > 0.001f) {  // ゼロ除算を防ぐ
        horizontalDir.x /= length;
        horizontalDir.z /= length;
    }
    else {
        // カメラが真上/真下を向いている場合のフォールバック
        horizontalDir.x = 0.0f;
        horizontalDir.z = -1.0f;
    }

    // 現在の速度を取得
    Float3 currentVel;
    RigidBodyGetVelocity(rigidBody, &currentVel);

    // 入力処理
    if (KeyPressed('W')) {
        float targetSpeed;

        if (KeyPressed(VK_SHIFT)) {
            currentState = Running;
            targetSpeed = 60.0f;
        }
        else {
            currentState = Walking;
            targetSpeed = 40.0f;
        }

        // **正規化された水平方向ベクトルを使用**
        Float3 targetVelocity = {
            -horizontalDir.x * targetSpeed,
            currentVel.y,  // Y軸（重力）は保持
            -horizontalDir.z * targetSpeed
        };

        RigidBodySetVelocity(rigidBody, &targetVelocity);
    }
    else {
        currentState = Idle;

        // 入力がない場合は水平方向の速度を急速に減衰
        Float3 stoppingVel = {
            currentVel.x * 0.5f,
            currentVel.y,
            currentVel.z * 0.5f
        };
        RigidBodySetVelocity(rigidBody, &stoppingVel);
    }

    // カメラの操作
    float MouseX = (float)GetMouseMoveX() * 0.001f;
    float MouseY = (float)GetMouseMoveY() * 0.001f;

    CameraTransform camTrans;
    GetCameraTransform(Camera, &camTrans);
    camTrans.rotation.x += MouseX;
    camTrans.rotation.y -= MouseY;
    SetCameraTransform(Camera, &camTrans);

    // プレイヤーの回転制御
    Float3 PlayerRot = trans.rotation;
    PlayerRot.y = camTrans.rotation.x - DirectX::XMConvertToRadians(180.0f);
    SetObjectRotation(player, PlayerRot);
}

void Script_PlayerMove::UpdateAnimation()
{
	// アニメーション
	if (previousState != currentState) {

		switch (currentState)
		{
		case Idle:
			SetAnimationClip(Animation, 1);
			break;
		case Walking:
			SetAnimationClip(Animation, 2);
			break;
		case Running:
			SetAnimationClip(Animation, 3);
			break;
		default:
			break;
		}
		previousState = currentState;
	}
}
