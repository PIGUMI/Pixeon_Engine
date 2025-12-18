#include "CameraController.h"

void Script_CameraController::BeginPlay() {
    SceneHandle scene;
	GetCurrentScene(&scene);
	GetGameObject(scene, "Player", &_target);
}

void Script_CameraController::Update() {
}

void Script_CameraController::EndPlay() {
}
