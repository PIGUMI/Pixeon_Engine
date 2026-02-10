#include "Player.h"

void Script_Player::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay
	_Head = _parentObject->FindChildObject("Head");
	_Body = _parentObject->FindChildObject("Body");
	_Head->GetComponent<Camera>("CameraComponent");
}

void Script_Player:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update
}

void Script_Player::EndPlay() {
    IScript::EndPlay();// EndPlay
}
