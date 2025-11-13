#include "SkyBox.h"

void Script_SkyBox::BeginPlay() {
    // BeginPlay
    SceneHandle CurrentScene;
	GetCurrentScene(&CurrentScene);
	GetGameObject(CurrentScene, "SkyBox", &SkyBox);
	GetGameObject(CurrentScene, "Cam", &Cam);
}

void Script_SkyBox::Update() {
	TransformData Trans;
	GetGameObjectTransform(Cam, &Trans);
	SetGameObjectTransform(SkyBox, &Trans);
}

void Script_SkyBox::EndPlay() {

}
