#include "Stage.h"
#include <cmath>

void Script_Stage::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay

    Object* Stage;
    Stage = _parentScene->FindObject("Stage");
    ParentObject = Stage->FindChildObject("Object");

	gen = std::mt19937(rd());



}

void Script_Stage:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update
}

void Script_Stage::EndPlay() {
    IScript::EndPlay();// EndPlay
}
