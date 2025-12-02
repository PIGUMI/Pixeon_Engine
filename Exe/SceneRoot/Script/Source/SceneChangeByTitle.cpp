#include "SceneChangeByTitle.h"

void Script_SceneChangeByTitle::BeginPlay() {
    // BeginPlay
}

void Script_SceneChangeByTitle::Update() {
    // Update
    bool IsKey = false;
	IsKeyTrigger('W', &IsKey);
    if (IsKey) {
		ChangeScene("InGame");
	}
}

void Script_SceneChangeByTitle::EndPlay() {
    // EndPlay
}
