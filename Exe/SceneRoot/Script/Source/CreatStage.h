#pragma once
#include "Include/IScript.h"
#include <random>
#include <vector>

class Script_CreatStage : public IScript {
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

    struct PlacedObject {
        float x;
        float z;
        float sizeX;
        float sizeZ;
    };

    std::vector<PlacedObject> placedObjects;

    // ÉâÉìÉ_ÉÄê∂ê¨óp
    std::random_device rd;
    std::mt19937 gen;

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

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_CreatStage();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
