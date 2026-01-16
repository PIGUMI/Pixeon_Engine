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
	int MouseX = 0;
	int MouseY = 0;

	MouseX = GetMouseMoveX();
	MouseY = GetMouseMoveY();

	transform headTransform;
	GetObjectTransform(headObject, &headTransform);
	headTransform.rotation.x -= (float)MouseY * 0.01f;
	headTransform.rotation.y += (float)MouseX * 0.01f;
	if(headTransform.rotation.x > 1.5f)
	{
		headTransform.rotation.x = 1.5f;
	}
	if(headTransform.rotation.x < -1.5f)
	{
		headTransform.rotation.x = -1.5f;
	}
	SetObjectTransform(headObject, &headTransform);

	transform playerTransform;
	GetObjectTransform(playerObject, &playerTransform);
	if(KeyPressed('W'))
	{
		transform bodyTransform;
		Float3 forward;
		GetCameraForwardVector(CameraComp, &forward);
		// YŽ²¬•ª‚ð–³Ž‹‚µ‚Ä…•½ˆÚ“®
		forward.y = 0.0f;
		// ³‹K‰»
		float length = sqrtf(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
		if (length > 0.0f)
		{
			forward.x /= length;
			forward.y /= length;
			forward.z /= length;
		}
		playerTransform.position.x += -forward.x * 0.1f;
		playerTransform.position.y += -forward.y * 0.1f;
		playerTransform.position.z += -forward.z * 0.1f;

		GetObjectTransform(bodyObject, &bodyTransform);
		bodyTransform.rotation.y = headTransform.rotation.y;
		SetObjectTransform(bodyObject, &bodyTransform);
	}
	SetObjectTransform(playerObject, &playerTransform);

	if(KeyTriggered('Q'))
	{
		SceneHandle scene;
		GetCurrentScene(&scene);
		AddObjectToScene(scene, Bullet);
	}
}

void Script_Player::EndPlay() {
}
