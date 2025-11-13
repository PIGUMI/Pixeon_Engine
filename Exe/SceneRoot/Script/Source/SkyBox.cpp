#include "SkyBox.h"


void Script_SkyBox::BeginPlay() {
    // BeginPlay
    SceneHandle Scene;
    GetCurrentScene(&Scene);
    GetGameObject(Scene, "SkeBox", &skyboxObject);
}

void Script_SkyBox::Update() {
    // Update
}

void Script_SkyBox::EndPlay() {
    // EndPlay
}
