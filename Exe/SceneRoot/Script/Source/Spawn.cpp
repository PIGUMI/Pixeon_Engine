#include "Spawn.h"
#include <Windows.h>

GameObjectHandle Model;
SceneHandle scene;
int Count = 0;

void Script_Spawn::BeginPlay() {
    // BeginPlay;
	GetCurrentScene(&scene);
	GetGameObject(scene, "Model", &Model);
}

void Script_Spawn::Update() {
    // Update
    Count++;
	AddGameObject(scene,Model);
}

void Script_Spawn::EndPlay() {
    // EndPlay
	std::string msg = "Spawned " + std::to_string(Count) + " Models!";
	MessageBoxA(NULL, msg.c_str(), "Spawn Script", MB_OK);
}
