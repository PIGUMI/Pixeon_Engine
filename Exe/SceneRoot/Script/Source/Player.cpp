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
	int MouseX = Input::GetMouseMoveX();
	int MouseY = Input::GetMouseMoveY();
}
