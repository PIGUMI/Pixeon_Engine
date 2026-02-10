#include "CreatStage.h"
#include <cmath>
#include <iostream>

void Script_CreatStage::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay
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
        isPlaced = true;
    }

    if (Stage)
    {
        delete Stage;
        Stage = nullptr;
    }
}

void Script_CreatStage:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update
}

void Script_CreatStage::EndPlay() {
    IScript::EndPlay();// EndPlay
	delete ParentObject;
	ParentObject = nullptr;
    placedObjects.clear();
}

bool Script_CreatStage::CheckOverlap(float x, float z, float sizeX, float sizeZ) {
    for (const auto& obj : placedObjects)
    {
        // AABB（軸に沿ったバウンディングボックス）による衝突判定
        float halfSizeX = (sizeX + marginDistance) / 2.0f;
        float halfSizeZ = (sizeZ + marginDistance) / 2.0f;

        float objHalfSizeX = (obj.sizeX + marginDistance) / 2.0f;
        float objHalfSizeZ = (obj.sizeZ + marginDistance) / 2.0f;

        // 2つの矩形が重なっているかチェック
        if (std::abs(x - obj.x) < (halfSizeX + objHalfSizeX) &&
            std::abs(z - obj.z) < (halfSizeZ + objHalfSizeZ))
        {
            return true; // 重なっている
        }
    }

    return false; // 重なっていない
}

void Script_CreatStage::PlaceObject(int typeIndex, float x, float z) {
    Object* prefabObj = nullptr;
    prefabObj = _parentScene->FindPrefabObject(prefabNames[typeIndex]);
    Object* cloneObject = nullptr;
    cloneObject = _parentScene->AddObject(prefabObj);
    ParentObject->AddChildObject(cloneObject);
    DirectX::XMFLOAT3 Pos = { x,0,z };
    cloneObject->SetPosition(Pos);
    
    delete prefabObj;
    delete cloneObject;
}

float Script_CreatStage::GetRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

int Script_CreatStage::GetRandomInt(int min, int max) {
    std::uniform_int_distribution<int> dis(min, max);
    return dis(gen);
}