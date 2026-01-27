#include "MuzzleFlash.h"

void Script_MuzzleFlash::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay
	FindComponent(_parentObject, "EffectComponent", &_effectComp);
}

void Script_MuzzleFlash:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update
	bool isPlaying = false;
	EffectIsPlaying(_effectComp, &isPlaying);
	if (!isPlaying) {
		RemoveObjectFromScene(_parentScene, _parentObject);
	}
}

void Script_MuzzleFlash::EndPlay() {
    IScript::EndPlay();// EndPlay
}
