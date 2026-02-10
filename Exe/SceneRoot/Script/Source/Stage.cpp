// Stage.cpp
#include "Stage.h"
#include <cmath>
#include <iostream>

void Script_Stage::BeginPlay() {
    IScript::BeginPlay();

    // ========================================
    // 自動管理されるリソースを生成
    // ========================================
    placedObjects = CreateManaged<std::vector<PlacedObjectData>>();
    gen = CreateManaged<std::mt19937>();

    if (!_parentScene) {
        std::cerr << "[Script_Stage] Error: _parentScene is null" << std::endl;
        return;
    }

    Object* Stage = _parentScene->FindObject("Stage");
    if (!Stage) {
        std::cerr << "[Script_Stage] Error: Stage not found" << std::endl;
        return;
    }

    ParentObject = Stage->FindChildObject("Object");
    if (!ParentObject) {
        std::cerr << "[Script_Stage] Error: Object not found" << std::endl;
        return;
    }

    std::random_device rd;
    *gen = std::mt19937(rd());

    if (isPlaced) return;

    int placedCount = 0;
    int maxAttempts = objectCount * 20;
    int attempts = 0;

    while (placedCount < objectCount && attempts < maxAttempts) {
        attempts++;

        float halfSizeX = objectSizeX / 2.0f;
        float halfSizeZ = objectSizeZ / 2.0f;
        float randomX = GetRandomFloat(mapMinX + halfSizeX, mapMaxX - halfSizeX);
        float randomZ = GetRandomFloat(mapMinZ + halfSizeZ, mapMaxZ - halfSizeZ);

        if (!CheckOverlap(randomX, randomZ, objectSizeX, objectSizeZ)) {
            int typeIndex = GetRandomInt(0, 2);
            PlaceObject(typeIndex, randomX, randomZ);
            placedCount++;
        }
    }

    isPlaced = true;
}

void Script_Stage::Update(float DeltaTime) {
    IScript::Update(DeltaTime);
}

void Script_Stage::EndPlay() {
    // 自動的にすべてのリソースが解放される
    IScript::EndPlay();
}

bool Script_Stage::CheckOverlap(float x, float z, float sizeX, float sizeZ) {
    if (!placedObjects) return false;

    for (const auto& obj : *placedObjects) {
        float halfSizeX = (sizeX + marginDistance) / 2.0f;
        float halfSizeZ = (sizeZ + marginDistance) / 2.0f;
        float objHalfSizeX = (obj.sizeX + marginDistance) / 2.0f;
        float objHalfSizeZ = (obj.sizeZ + marginDistance) / 2.0f;

        if (std::abs(x - obj.x) < (halfSizeX + objHalfSizeX) &&
            std::abs(z - obj.z) < (halfSizeZ + objHalfSizeZ)) {
            return true;
        }
    }
    return false;
}

void Script_Stage::PlaceObject(int typeIndex, float x, float z) {
    // CreateManagedObject で自動管理
    Object* newObj = CreateManagedObject(prefabNames[typeIndex]);
    if (!newObj) return;

    ParentObject->AddChildObject(newObj);
    Float3 pos = CreateFloat3(x, 0.0f, z);
    newObj->SetPosition(Pixeon::ToXMFloat3(pos));

    if (placedObjects) {
        placedObjects->emplace_back(x, z, objectSizeX, objectSizeZ, newObj);
    }
}

float Script_Stage::GetRandomFloat(float min, float max) {
    if (!gen) return min;
    std::uniform_real_distribution<float> dist(min, max);
    return dist(*gen);
}

int Script_Stage::GetRandomInt(int min, int max) {
    if (!gen) return min;
    std::uniform_int_distribution<int> dist(min, max);
    return dist(*gen);
}