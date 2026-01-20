#include "Player.h"
#include <DirectXMath.h>
#include <cmath>

void Script_Player::BeginPlay() {
	SceneHandle scene;
	GetCurrentScene(&scene);
	FindObjectByName(scene, "Player", &playerObject);
	FindChildObjectByName(playerObject, "Head", &headObject);
	FindChildObjectByName(playerObject, "Body", &bodyObject);
	FindComponent(headObject, "CameraComponent", &CameraComp);
	FindPrefabObjectByName("Bullet", &Bullet);
	Object UI;
	FindObjectByName(scene, "UI", &UI);
	FindChildObjectByName(UI, "Hp", &HitPointUI);
	Component AnimatorComp;
	FindComponent(HitPointUI, "Animator2DComponent", &AnimatorComp);
	GetAnimator2D(AnimatorComp, "HP", &HP);
	FixedMouseCursor(true);
	
	Component cameraComp;
	FindComponent(headObject, "CameraComponent", &cameraComp);
	SetMainCameraByPtr(cameraComp);
}

void Script_Player::Update() {

	Movement();
	if (KeyPressed('Q'))
	{
		float fHP;
		FindKeyFrame(HP, "Hp", &hp);
		GetVertexOffsetRight(hp, &fHP);
		fHP -= 1.0f;
		SetVertexOffsetRight(hp, fHP);
	}
	if (KeyPressed('E'))
	{
		float fHP;
		FindKeyFrame(HP, "Hp", &hp);
		GetVertexOffsetRight(hp, &fHP);
		fHP += 1.0f;
		SetVertexOffsetRight(hp, fHP);
	}
}

void Script_Player::EndPlay() {
}

void Script_Player::Movement()
{
	int Mouse_X = 0;
	int Mouse_Y = 0;

	Mouse_X = GetMouseMoveX();
	Mouse_Y = GetMouseMoveY();

	transform head_Transform;
	GetObjectTransform(headObject, &head_Transform);
	transform body_Transform;
	GetObjectTransform(bodyObject, &body_Transform);

	head_Transform.rotation.x -= (float)Mouse_Y * 0.005f;
	head_Transform.rotation.y += (float)Mouse_X * 0.005f;

	if (head_Transform.rotation.x > DirectX::XMConvertToRadians(70.0f))
	{
		head_Transform.rotation.x = DirectX::XMConvertToRadians(70.0f);
	}
	if (head_Transform.rotation.x < -DirectX::XMConvertToRadians(70.0f))
	{
		head_Transform.rotation.x = -DirectX::XMConvertToRadians(70.0f);
	}
	if (head_Transform.rotation.y - body_Transform.rotation.y > DirectX::XMConvertToRadians(70.0f))
	{
		head_Transform.rotation.y = body_Transform.rotation.y + DirectX::XMConvertToRadians(70.0f);
	}
	if (head_Transform.rotation.y - body_Transform.rotation.y < -DirectX::XMConvertToRadians(70.0f))
	{
		head_Transform.rotation.y = body_Transform.rotation.y - DirectX::XMConvertToRadians(70.0f);
	}


	transform playerTransform;
	GetObjectTransform(playerObject, &playerTransform);

	transform bodyTransform;
	Float3 forward;
	GetCameraForwardVector(CameraComp, &forward);
	forward.y = 0.0f;
	float length = sqrtf(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	if (length > 0.0f)
	{
		forward.x /= length;
		forward.y /= length;
		forward.z /= length;
	}

	Float3 right;
	GetCameraRightVector(CameraComp, &right);
	right.y = 0.0f;
	length = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
	if (length > 0.0f)
	{
		right.x /= length;
		right.y /= length;
		right.z /= length;
	}

	isMoving = false;

	if (KeyPressed('W'))
	{
		playerTransform.position.x += -forward.x * 0.1f;
		playerTransform.position.y += -forward.y * 0.1f;
		playerTransform.position.z += -forward.z * 0.1f;

		GetObjectTransform(bodyObject, &bodyTransform);
		bodyTransform.rotation.y = head_Transform.rotation.y;
		SetObjectTransform(bodyObject, &bodyTransform);
		SetObjectTransform(playerObject, &playerTransform);
		isMoving = true;
	}
	if (KeyPressed('S'))
	{
		playerTransform.position.x -= -forward.x * 0.1f;
		playerTransform.position.y -= -forward.y * 0.1f;
		playerTransform.position.z -= -forward.z * 0.1f;
		GetObjectTransform(bodyObject, &bodyTransform);
		bodyTransform.rotation.y = head_Transform.rotation.y;
		SetObjectTransform(bodyObject, &bodyTransform);
		SetObjectTransform(playerObject, &playerTransform);
		isMoving = true;
	}
	if (KeyPressed('A'))
	{
		playerTransform.position.x += right.x * 0.05f;
		playerTransform.position.y += right.y * 0.05f;
		playerTransform.position.z += right.z * 0.05f;
		GetObjectTransform(bodyObject, &bodyTransform);
		bodyTransform.rotation.y = head_Transform.rotation.y;
		SetObjectTransform(bodyObject, &bodyTransform);
		SetObjectTransform(playerObject, &playerTransform);
		isMoving = true;
	}
	if (KeyPressed('D'))
	{
		playerTransform.position.x -= right.x * 0.05f;
		playerTransform.position.y -= right.y * 0.05f;
		playerTransform.position.z -= right.z * 0.05f;
		GetObjectTransform(bodyObject, &bodyTransform);
		bodyTransform.rotation.y = head_Transform.rotation.y;
		SetObjectTransform(bodyObject, &bodyTransform);
		SetObjectTransform(playerObject, &playerTransform);
		isMoving = true;
	}

	if (isMoving)
	{
		walkTimer += 0.15f;
		float headBobAmount = cosf(walkTimer) * 0.05f;
		head_Transform.position.y = headBobAmount;
		head_Transform.position.y += 1.5f;
	}
	else
	{
		walkTimer = 0.0f;
		head_Transform.position.y = 1.5f;
	}

	SetObjectTransform(headObject, &head_Transform);
}