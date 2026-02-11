#include "Player.h"

void Script_Player::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay
	_Head = _parentObject->FindChildObject("Head");
	_Body = _parentObject->FindChildObject("Body");
	_Camera = _Head->GetComponent<Camera>("CameraComponent");
	_parentScene->SetMainCamera(_Camera);
	FixedMouseCursor(true);

	_parentObject->SetInt("HP", 100);
}

void Script_Player:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update
	Movement(DeltaTime);
}

void Script_Player::EndPlay() {
    IScript::EndPlay();// EndPlay
	if (_Head) {
		delete _Head;
		_Head = nullptr;
	}
	if (_Body) {
		delete _Body;
		_Body = nullptr;
	}
	if (_Camera) {
		delete _Camera;
		_Camera = nullptr;
	}
}

void Script_Player::Movement(float DeltaTime) {
	// マウスの移動量を取得
	int MouseX = Input::GetMouseMoveX();
	int MouseY = Input::GetMouseMoveY();
	transform headTransform;
	transform bodyTransform;
	headTransform = _Head->GetTransform();
	bodyTransform = _Body->GetTransform();
	headTransform.rotation.x += (float)MouseY * _Sensitivity;
	headTransform.rotation.y += (float)MouseX * _Sensitivity;

	// 上下左右の回転制限
	if (headTransform.rotation.x > DirectX::XMConvertToRadians(_LimitAngle))headTransform.rotation.x = DirectX::XMConvertToRadians(_LimitAngle);
	if (headTransform.rotation.x < -DirectX::XMConvertToRadians(_LimitAngle))headTransform.rotation.x = -DirectX::XMConvertToRadians(_LimitAngle);
	if (headTransform.rotation.y - bodyTransform.rotation.y > DirectX::XMConvertToRadians(_LimitAngle))headTransform.rotation.y = bodyTransform.rotation.y + DirectX::XMConvertToRadians(_LimitAngle);
	if (headTransform.rotation.y - bodyTransform.rotation.y < -DirectX::XMConvertToRadians(_LimitAngle))headTransform.rotation.y = bodyTransform.rotation.y - DirectX::XMConvertToRadians(_LimitAngle);
	transform playerTransform;
	DirectX::XMFLOAT3 forward;
	DirectX::XMFLOAT3 right;
	playerTransform = _parentObject->GetTransform();
	forward = _Camera->GetForwardVector();
	right = _Camera->GetRightVector();

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

	if (Input::IsKeyPressed('W'))
	{
		AnimationNo = 1;
		playerTransform.position.x += -forward.x * _MoveSpeed;
		playerTransform.position.y += -forward.y * _MoveSpeed;
		playerTransform.position.z += -forward.z * _MoveSpeed;
		bodyTransform = _Body->GetTransform();
		bodyTransform.rotation.y = headTransform.rotation.y;
		_Body->SetTransform(bodyTransform);
		_parentObject->SetTransform(playerTransform);
		_isMoving = true;
	}

	if (Input::IsKeyPressed('S'))
	{
		AnimationNo = 1;
		playerTransform.position.x += forward.x * _MoveSpeed;
		playerTransform.position.y += forward.y * _MoveSpeed;
		playerTransform.position.z += forward.z * _MoveSpeed;
		bodyTransform = _Body->GetTransform();
		bodyTransform.rotation.y = headTransform.rotation.y;
		_Body->SetTransform(bodyTransform);
		_parentObject->SetTransform(playerTransform);
		_isMoving = true;
	}

	if (Input::IsKeyPressed('A'))
	{
		AnimationNo = 1;
		playerTransform.position.x += right.x * (_MoveSpeed * 0.5f);
		playerTransform.position.y += right.y * (_MoveSpeed * 0.5f);
		playerTransform.position.z += right.z * (_MoveSpeed * 0.5f);
		bodyTransform = _Body->GetTransform();
		bodyTransform.rotation.y = headTransform.rotation.y;
		_Body->SetTransform(bodyTransform);
		_parentObject->SetTransform(playerTransform);
		_isMoving = true;
	}

	if (Input::IsKeyPressed('D'))
	{
		AnimationNo = 1;
		playerTransform.position.x += -right.x * (_MoveSpeed * 0.5f);
		playerTransform.position.y += -right.y * (_MoveSpeed * 0.5f);
		playerTransform.position.z += -right.z * (_MoveSpeed * 0.5f);
		bodyTransform = _Body->GetTransform();
		bodyTransform.rotation.y = headTransform.rotation.y;
		_Body->SetTransform(bodyTransform);
		_parentObject->SetTransform(playerTransform);
		_isMoving = true;
	}

	if (_isMoving)
	{
		_walkTimer += DeltaTime * 10.0f;
		float headBobAmount = cosf(_walkTimer) * 0.05f;
		headTransform.position.y = headBobAmount;
		headTransform.position.y = 1.65f;
	}
	else
	{
		_walkTimer = 0.0f;
		headTransform.position.y = 1.65f;
	}
	_Head->SetTransform(headTransform);
	_Body->SetInt("AnimationNo", AnimationNo);
}
