#include "Player.h"
#include <DirectXMath.h>
#include <cmath>
#include <Windows.h>
#include <string>
void Script_Player::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay
	FindChildObjectByName(_parentObject, "Head", &_Head);
	FindChildObjectByName(_parentObject, "Body", &_Body);
	FindComponent(_Head, "CameraComponent", &_Camera);

	SetMainCameraByPtr(_Camera);
	FixedMouseCursor(true);
	SetVariableInt(_parentObject, "HP", 100);
}

void Script_Player:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update
	Movement(DeltaTime);
}

void Script_Player::EndPlay() {
    IScript::EndPlay();
}

void Script_Player::Movement(float DeltaTime) {
	// マウスの移動量を取得
	int Mouse_X = GetMouseMoveX();
	int Mouse_Y = GetMouseMoveY();
	transform head_Transform;
	transform body_Transform;
	GetObjectTransform(_Head, &head_Transform);
	GetObjectTransform(_Body, &body_Transform);
	head_Transform.rotation.x += (float)Mouse_Y * _Sensitivity;
	head_Transform.rotation.y += (float)Mouse_X * _Sensitivity;

	// 上下左右の回転制限
	if (head_Transform.rotation.x > DirectX::XMConvertToRadians(_LimitAngle))head_Transform.rotation.x = DirectX::XMConvertToRadians(_LimitAngle);
	if (head_Transform.rotation.x < -DirectX::XMConvertToRadians(_LimitAngle))head_Transform.rotation.x = -DirectX::XMConvertToRadians(_LimitAngle);
	if (head_Transform.rotation.y - body_Transform.rotation.y > DirectX::XMConvertToRadians(_LimitAngle))head_Transform.rotation.y = body_Transform.rotation.y + DirectX::XMConvertToRadians(_LimitAngle);
	if (head_Transform.rotation.y - body_Transform.rotation.y < -DirectX::XMConvertToRadians(_LimitAngle))head_Transform.rotation.y = body_Transform.rotation.y - DirectX::XMConvertToRadians(_LimitAngle);
	transform player_Transform;
	Float3 forward;
	Float3 right;
	GetObjectTransform(_parentObject, &player_Transform);
	GetCameraForwardVector(_Camera, &forward);
	GetCameraRightVector(_Camera, &right);
	// 前方向の正規化
	forward.y = 0.0f;
	float length = sqrtf(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	if (length > 0.0f)
	{
		forward.x /= length;
		forward.y /= length;
		forward.z /= length;
	}
	// 水平方向の正規化
	right.y = 0.0f;
	length = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
	if (length > 0.0f)
	{
		right.x /= length;
		right.y /= length;
		right.z /= length;
	}
	_isMoving = false;
	// キー入力による移動処理
	int AnimationNo = 0;

	if (KeyPressed('W'))
	{
		AnimationNo = 1;
		player_Transform.position.x += -forward.x * _MoveSpeed;
		player_Transform.position.y += -forward.y * _MoveSpeed;
		player_Transform.position.z += -forward.z * _MoveSpeed;
		GetObjectTransform(_Body, &body_Transform);
		body_Transform.rotation.y = head_Transform.rotation.y;
		SetObjectTransform(_Body, &body_Transform);
		SetObjectTransform(_parentObject, &player_Transform);
		_isMoving = true;
	}
	if (KeyPressed('S'))
	{
		AnimationNo = 1;
		player_Transform.position.x -= -forward.x * _MoveSpeed;
		player_Transform.position.y -= -forward.y * _MoveSpeed;
		player_Transform.position.z -= -forward.z * _MoveSpeed;
		GetObjectTransform(_Body, &body_Transform);
		body_Transform.rotation.y = head_Transform.rotation.y;
		SetObjectTransform(_Body, &body_Transform);
		SetObjectTransform(_parentObject, &player_Transform);
		_isMoving = true;
	}
	if (KeyPressed('A'))
	{
		AnimationNo = 1;
		player_Transform.position.x += right.x * (_MoveSpeed * 0.5f);
		player_Transform.position.y += right.y * (_MoveSpeed * 0.5f);
		player_Transform.position.z += right.z * (_MoveSpeed * 0.5f);
		GetObjectTransform(_Body, &body_Transform);
		body_Transform.rotation.y = head_Transform.rotation.y;
		SetObjectTransform(_Body, &body_Transform);
		SetObjectTransform(_parentObject, &player_Transform);
		_isMoving = true;
	}
	if (KeyPressed('D'))
	{
		AnimationNo = 1;
		player_Transform.position.x -= right.x * (_MoveSpeed * 0.5f);
		player_Transform.position.y -= right.y * (_MoveSpeed * 0.5f);
		player_Transform.position.z -= right.z * (_MoveSpeed * 0.5f);
		GetObjectTransform(_Body, &body_Transform);
		body_Transform.rotation.y = head_Transform.rotation.y;
		SetObjectTransform(_Body, &body_Transform);
		SetObjectTransform(_parentObject, &player_Transform);
		_isMoving = true;
	}
	if (_isMoving)
	{
		_walkTimer += DeltaTime * 10.0f;
		float headBobAmount = cosf(_walkTimer) * 0.05f;
		head_Transform.position.y = headBobAmount;
		head_Transform.position.y += 1.65f;
	}
	else
	{
		_walkTimer = 0.0f;
		head_Transform.position.y = 1.65f;
	}
	SetObjectTransform(_Head, &head_Transform);
	SetVariableInt(_Body, "AnimationNo", AnimationNo);
}
