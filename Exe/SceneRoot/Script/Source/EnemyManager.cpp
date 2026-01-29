#include "EnemyManager.h"
#include <cstdlib>
#include <cmath>

#define ENEMY_COUNT (10)

void Script_EnemyManager::BeginPlay() {
    IScript::BeginPlay();
	FindPrefabObjectByName("Entity",&_Enemy);
}

void Script_EnemyManager:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);

    int count = 0;
	CountChildObjects(_parentObject, &count);
	if (count >= ENEMY_COUNT) return;
    Object EntityTemp;
    AddObjectToScene(_parentScene, _Enemy, &EntityTemp);
    ObjectParenthood(EntityTemp, _parentObject);
}

void Script_EnemyManager::EndPlay() {
    IScript::EndPlay();
}


float Script_EnemyManager::GetRandomFloat(float min, float max)
{
    return min + (max - min) * (rand() / (float)RAND_MAX);
}

void Script_EnemyManager::CalcEnemySpawnPos(float playerX, float playerZ, float& outX, float& outZ)
{
    float angle = GetRandomFloat(0.0f, 2.0f * 3.14159265f);
    float distance = GetRandomFloat(10.0f, 20.0f);
    outX = playerX + cosf(angle) * distance;
    outZ = playerZ + sinf(angle) * distance;
}
