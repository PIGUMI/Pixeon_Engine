#include "SkyBox.h"

void Script_SkyBox::BeginPlay() {
    // BeginPlay
	SceneHandle Temp;
	GetCurrentScene(&Temp);
	// オブジェクトの取得
	FindObjectByName(Temp, "Player", &player);
	FindObjectByName(Temp, "SkyBox", &skybox);
}

void Script_SkyBox::Update() {
	transform playerTrans;
	transform skyboxTrans;
	GetObjectTransform(skybox, &skyboxTrans);
	GetObjectTransform(player, &playerTrans);
	skyboxTrans.position = playerTrans.position;
	SetObjectTransform(skybox, &skyboxTrans);
}

void Script_SkyBox::EndPlay() {
    // EndPlay
}
