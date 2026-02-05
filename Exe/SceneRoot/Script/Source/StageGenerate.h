#pragma once
#include "Include/IScript.h"
#include <random>
#include <vector>

class Script_StageGenerate : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;

private:
    const char* prefabNames[3] = {
        "ContainerABA",  // 見た目タイプ1のプレハブ名
        "ContainerAAA",  // 見た目タイプ2のプレハブ名
        "ContainerBBB"   // 見た目タイプ3のプレハブ名
    };

    struct PlacedObject {
        float x;
        float z;
        float sizeX;
        float sizeZ;
    };

    std::vector<PlacedObject> placedObjects;

    // ランダム生成用
    std::random_device rd;
    std::mt19937 gen;

    bool isPlaced = false;     // 配置済みフラグ

    // ヘルパー関数
    bool CheckOverlap(float x, float z, float sizeX, float sizeZ);
    void PlaceObject(int typeIndex, float x, float z);
    float GetRandomFloat(float min, float max);
    int GetRandomInt(int min, int max);

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

    // パラメータ
    int objectCount = 30;          // 配置するオブジェクトの総数
    float objectSizeX = 7.3f;      // オブジェクトのXサイズ
    float objectSizeZ = 5.5f;      // オブジェクトのZサイズ
    float mapMinX = -38.0f;        // マップの最小X座標
    float mapMaxX = 38.0f;         // マップの最大X座標
    float mapMinZ = -38.0f;        // マップの最小Z座標
    float mapMaxZ = 38.0f;         // マップの最大Z座標
    float marginDistance = 1.0f;   // オブジェクト間のマージン距離
	Object ParentObject;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_StageGenerate();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}