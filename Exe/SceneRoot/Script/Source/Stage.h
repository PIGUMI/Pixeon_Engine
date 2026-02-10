// Stage.h
#pragma once
#include "Include/IScript.h"
#include <random>
#include <vector>

class PlacedObjectData {
public:
    float x, z, sizeX, sizeZ;
    Object* objectPtr = nullptr;

    PlacedObjectData(float x_, float z_, float sx, float sz, Object* ptr = nullptr)
        : x(x_), z(z_), sizeX(sx), sizeZ(sz), objectPtr(ptr) {
    }
};

class Script_Stage : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;

private:
    std::string prefabNames[3] = {
        "ContainerABA",
        "ContainerAAA",
        "ContainerBBB"
    };

    // ========================================
    // é©ìÆä«óùÇ≥ÇÍÇÈÉäÉ\Å[ÉX
    // ========================================
    std::vector<PlacedObjectData>* placedObjects = nullptr;
    std::mt19937* gen = nullptr;

    Object* ParentObject = nullptr;
    bool isPlaced = false;

    bool CheckOverlap(float x, float z, float sizeX, float sizeZ);
    void PlaceObject(int typeIndex, float x, float z);
    float GetRandomFloat(float min, float max);
    int GetRandomInt(int min, int max);

public:
    int objectCount = 30;
    float objectSizeX = 7.3f;
    float objectSizeZ = 5.5f;
    float mapMinX = -38.0f;
    float mapMaxX = 38.0f;
    float mapMinZ = -38.0f;
    float mapMaxZ = 38.0f;
    float marginDistance = 1.0f;

public:
#define PROPERTY_LIST(ACTION) \
    ACTION(INT, objectCount) \
    ACTION(FLOAT, objectSizeX) \
    ACTION(FLOAT, objectSizeZ) \
    ACTION(FLOAT, mapMinX) \
    ACTION(FLOAT, mapMaxX) \
    ACTION(FLOAT, mapMinZ) \
    ACTION(FLOAT, mapMaxZ) \
    ACTION(FLOAT, marginDistance)

    DECLARE_SCRIPT_PROPERTIES()
#undef PROPERTY_LIST
};