#include "Test.h"


void Script_Test::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay
}

void Script_Test:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update
    transform tras;
    tras = _parentObject->GetTransform();
    tras.position.y += 1.0f;
    _parentObject->SetTransform(tras);
}

void Script_Test::EndPlay() {
    IScript::EndPlay();// EndPlay
}
