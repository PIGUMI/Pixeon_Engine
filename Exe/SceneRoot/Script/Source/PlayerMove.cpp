#include "PlayerMove.h"
#include <Windows.h>


void Script_PlayerMove::BeginPlay() {
	SceneHandle Temp;
	GetCurrentScene(&Temp);
	FindObjectByName(Temp, "Player", &player);
	FindComponent(player, "Animation", &Animation);
	FindComponent(player, "CameraComponent", &Camera);
	int camNum;
	APIResult rs;
	rs = GetCameraNumber(Camera, &camNum);
	if(rs != PN_SUCCESS) {
		MessageBoxA(NULL, "Failed to get camera number", "Error", MB_OK);
	}
	else
	{
		MessageBoxA(NULL, ("Camera number: " + std::to_string(camNum)).c_str(), "Info", MB_OK);
	}
	SetMainCamera(camNum);
	FixedMouseCursor(true);
}

void Script_PlayerMove::Update() {
	currentState = Idle;
	transform trans;
	GetObjectTransform(player, &trans);

	if (KeyPressed('W')) {
		currentState = Walking;
		trans.position.z += 0.1f;
	}

	float MouseX = 0;
	float MouseY = 0;
	MouseX = (float)GetMouseMoveX();
	MouseY = (float)GetMouseMoveY();
	MouseX = MouseX * 0.001f;
	MouseY = MouseY * 0.001f;
	

	CameraTransform camTrans;
	GetCameraTransform(Camera,&camTrans);
	camTrans.rotation.x += MouseX;
	camTrans.rotation.y -= MouseY;
	SetCameraTransform(Camera,&camTrans);

	SetObjectTransform(player, &trans);

	if(previousState != currentState) {

		switch (currentState)
		{
		case Idle:
			SetAnimationClip(Animation, 1);
			break;
		case Walking:
			SetAnimationClip(Animation, 2);
			break;
		case Running:
			break;
		default:
			break;
		}
		previousState = currentState;
	}
}

void Script_PlayerMove::EndPlay() {
}
