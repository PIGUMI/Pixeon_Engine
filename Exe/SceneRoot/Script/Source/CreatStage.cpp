#include "CreatStage.h"
#include <cmath>
#include <iostream>

void Script_CreatStage::BeginPlay() {
    IScript::BeginPlay();

    Object* Stage = nullptr;
    Stage = _parentScene->FindObject("Stage");
    ParentObject = Stage->FindChildObject("Object");

    gen = std::mt19937(rd());

    if (!isPlaced && _parentScene != nullptr)
    {
        int placedCount = 0;
        int maxAttempts = objectCount * 20;
        int attempts = 0;

        while (placedCount < objectCount && attempts < maxAttempts)
        {
            attempts++;

            float halfSizeX = objectSizeX / 2.0f;
            float halfSizeZ = objectSizeZ / 2.0f;

            float randomX = GetRandomFloat(mapMinX + halfSizeX, mapMaxX - halfSizeX);
            float randomZ = GetRandomFloat(mapMinZ + halfSizeZ, mapMaxZ - halfSizeZ);

            if (!CheckOverlap(randomX, randomZ, objectSizeX, objectSizeZ))
            {
                int typeIndex = GetRandomInt(0, 2);

                PlaceObject(typeIndex, randomX, randomZ);

                PlacedObject placed;
                placed.x = randomX;
                placed.z = randomZ;
                placed.sizeX = objectSizeX;
                placed.sizeZ = objectSizeZ;
                placedObjects.push_back(placed);

                placedCount++;
            }
        }

        int enemyPlacedCount = 0;
        int enemyMaxAttempts = enemyCount * 20;
        int enemyAttempts = 0;

        while (enemyPlacedCount < enemyCount && enemyAttempts < enemyMaxAttempts)
        {
            enemyAttempts++;

            float halfEnemySizeX = enemySizeX / 2.0f;
            float halfEnemySizeZ = enemySizeZ / 2.0f;

            float randomX = GetRandomFloat(mapMinX + halfEnemySizeX, mapMaxX - halfEnemySizeX);
            float randomZ = GetRandomFloat(mapMinZ + halfEnemySizeZ, mapMaxZ - halfEnemySizeZ);

            if (!CheckOverlap(randomX, randomZ, enemySizeX + enemyMarginDistance * 2, enemySizeZ + enemyMarginDistance * 2))
            {
                PlaceEnemy(randomX, randomZ);

                PlacedObject placed;
                placed.x = randomX;
                placed.z = randomZ;
                placed.sizeX = enemySizeX + enemyMarginDistance * 2;
                placed.sizeZ = enemySizeZ + enemyMarginDistance * 2;
                placedObjects.push_back(placed);

                enemyPlacedCount++;
            }
        }
        isPlaced = true;
    }

    if (Stage)
    {
        delete Stage;
        Stage = nullptr;
    }
}

void Script_CreatStage::Update(float DeltaTime) {
    IScript::Update(DeltaTime);
}

void Script_CreatStage::EndPlay() {
    IScript::EndPlay();
    delete ParentObject;
    ParentObject = nullptr;
    placedObjects.clear();
}

bool Script_CreatStage::CheckOverlap(float x, float z, float sizeX, float sizeZ) {
    for (const auto& obj : placedObjects)
    {
        float halfSizeX = (sizeX + marginDistance) / 2.0f;
        float halfSizeZ = (sizeZ + marginDistance) / 2.0f;

        float objHalfSizeX = (obj.sizeX + marginDistance) / 2.0f;
        float objHalfSizeZ = (obj.sizeZ + marginDistance) / 2.0f;

        if (std::abs(x - obj.x) < (halfSizeX + objHalfSizeX) &&
            std::abs(z - obj.z) < (halfSizeZ + objHalfSizeZ))
        {
            return true;
        }
    }

    return false;
}

void Script_CreatStage::PlaceObject(int typeIndex, float x, float z) {
    Object* prefabObj = nullptr;
    prefabObj = _parentScene->FindPrefabObject(prefabNames[typeIndex]);

    if (!prefabObj) {
        return;
    }

    Object* cloneObject = nullptr;
    cloneObject = _parentScene->AddObject(prefabObj);

    if (cloneObject && ParentObject) {
        ParentObject->AddChildObject(cloneObject);
        DirectX::XMFLOAT3 Pos = { x, 0, z };
        cloneObject->SetPosition(Pos);
    }

    delete prefabObj;
    delete cloneObject;
}

void Script_CreatStage::PlaceEnemy(float x, float z) {
    Object* enemyPrefab = nullptr;
    enemyPrefab = _parentScene->FindPrefabObject(enemyPrefabName);

    if (!enemyPrefab) {
        return;
    }

    Object* enemyObject = nullptr;
    enemyObject = _parentScene->AddObject(enemyPrefab);

    if (enemyObject && ParentObject) {
        ParentObject->AddChildObject(enemyObject);
        DirectX::XMFLOAT3 Pos = { x, enemyOffsetY, z };
        enemyObject->SetPosition(Pos);
    }

    delete enemyPrefab;
    delete enemyObject;
}

float Script_CreatStage::GetRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

int Script_CreatStage::GetRandomInt(int min, int max) {
    std::uniform_int_distribution<int> dis(min, max);
    return dis(gen);
}