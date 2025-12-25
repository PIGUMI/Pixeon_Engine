#include "PlayerMove.h"
#include <Windows.h>


void Script_PlayerMove::BeginPlay() {
	SceneHandle Temp;
	GetCurrentScene(&Temp);
	FindObjectByName(Temp, "Player", &player);
	FindComponent(player, "Animation", &Animation);
	Component Camera;
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
}

void Script_PlayerMove::Update() {
	currentState = Idle;
	transform trans;
	GetObjectTransform(player, &trans);

	if (KeyPressed('W')) {
		currentState = Walking;
		trans.position.z += 0.1f;
	}




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
