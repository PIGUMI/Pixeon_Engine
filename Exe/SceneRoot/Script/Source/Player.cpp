#include "Player.h"

void Script_Player::BeginPlay() {
    SceneHandle scene;
	GetCurrentScene(&scene);
	FindObjectByName(scene, "Player", &playerObject);
	FindChildObjectByName(playerObject, "Head", &headObject);
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
	SetObjectTransform(headObject, &headTransform);

}

void Script_Player::EndPlay() {
    // EndPlay
}
