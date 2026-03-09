#include "Player.h"

void Script_Player::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay
}

void Script_Player:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update

    DirectX::XMFLOAT3 Pos = _parentObject->GetPosition();

    if (Input::IsKeyPressed('W'))
    {
        Pos.x += 1.0f * DeltaTime;
    }
    if (Input::IsKeyPressed('S'))
    {
        Pos.x -= 1.0f * DeltaTime;
    }
    if (Input::IsKeyPressed('A'))
    {
        Pos.z += 1.0f * DeltaTime;
    }
    if (Input::IsKeyPressed('D'))
    {
        Pos.z -= 1.0f * DeltaTime;
    }
    _parentObject->SetPosition(Pos);
}

void Script_Player::EndPlay() {
    IScript::EndPlay();// EndPlay
}
