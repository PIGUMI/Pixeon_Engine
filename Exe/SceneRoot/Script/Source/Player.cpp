#include "Player.h"

void Script_Player::BeginPlay() {
    SceneHandle scene;
	GetCurrentScene(&scene);
	FindObjectByName(scene, "Player", &playerObject);
	FindChildObjectByName(playerObject, "Head", &headObject);
	FindChildObjectByName(playerObject, "Body", &bodyObject);
	FindComponent(headObject, "CameraComponent", &CameraComp);
	FindPrefabObjectByName("Bullet", &Bullet);
	FixedMouseCursor(true);
	SetMainCamera(1);
}

void Script_Player::Update() {

	Movement();

	if(KeyTriggered('Q'))
	{
		SceneHandle scene;
		GetCurrentScene(&scene);
		AddObjectToScene(scene, Bullet);
	}
}

void Script_Player::EndPlay() {
}

void Script_Player::Movement()
{
	// Ž‹ŠE‘€ì
	int Mouse_X = 0;
	int Mouse_Y = 0;

	// ƒ}ƒEƒX‚ÌˆÚ“®—Ê‚ðŽæ“¾
	Mouse_X = GetMouseMoveX();
	Mouse_Y = GetMouseMoveY();

	// “ª•”‚Ì‰ñ“]‚ðXV
	transform head_Transform;
	GetObjectTransform(headObject, &head_Transform);

	// ƒ}ƒEƒX‚ÌˆÚ“®—Ê‚ÉŠî‚Ã‚¢‚Ä‰ñ“]‚ð’²®
	head_Transform.rotation.x -= (float)Mouse_Y * 0.005f;
	head_Transform.rotation.y += (float)Mouse_X * 0.005f;

	// ã‰º‚Ì‰ñ“]‚ð§ŒÀ
	if (head_Transform.rotation.x > 1.5f)
	{
		head_Transform.rotation.x = 1.5f;
	}
	if (head_Transform.rotation.x < -1.5f)
	{
		head_Transform.rotation.x = -1.5f;
	}

	// “ª•”‚Ì•ÏŠ·‚ðÝ’è
	SetObjectTransform(headObject, &head_Transform);

	// ˆÚ“®‘€ì
	transform playerTransform;
	GetObjectTransform(playerObject, &playerTransform);

	// ³‹K‰»
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

	if(KeyPressed('W'))
	{
		playerTransform.position.x += -forward.x * 0.1f;
		playerTransform.position.y += -forward.y * 0.1f;
		playerTransform.position.z += -forward.z * 0.1f;

		GetObjectTransform(bodyObject, &bodyTransform);
		bodyTransform.rotation.y = head_Transform.rotation.y;
		SetObjectTransform(bodyObject, &bodyTransform);
		SetObjectTransform(playerObject, &playerTransform);
	}
	if(KeyPressed('S'))
	{
		playerTransform.position.x -= -forward.x * 0.1f;
		playerTransform.position.y -= -forward.y * 0.1f;
		playerTransform.position.z -= -forward.z * 0.1f;
		GetObjectTransform(bodyObject, &bodyTransform);
		bodyTransform.rotation.y = head_Transform.rotation.y + 3.14f;
		SetObjectTransform(bodyObject, &bodyTransform);
		SetObjectTransform(playerObject, &playerTransform);
	}
	if(KeyPressed('A'))
	{
		playerTransform.position.x += right.x * 0.05f;
		playerTransform.position.y += right.y * 0.05f;
		playerTransform.position.z += right.z * 0.05f;
		GetObjectTransform(bodyObject, &bodyTransform);
		bodyTransform.rotation.y = head_Transform.rotation.y - 1.57f;
		SetObjectTransform(bodyObject, &bodyTransform);
		SetObjectTransform(playerObject, &playerTransform);
	}
	if(KeyPressed('D'))
	{
		playerTransform.position.x -= right.x * 0.05f;
		playerTransform.position.y -= right.y * 0.05f;
		playerTransform.position.z -= right.z * 0.05f;
		GetObjectTransform(bodyObject, &bodyTransform);
		bodyTransform.rotation.y = head_Transform.rotation.y + 1.57f;
		SetObjectTransform(bodyObject, &bodyTransform);
		SetObjectTransform(playerObject, &playerTransform);
	}
}
