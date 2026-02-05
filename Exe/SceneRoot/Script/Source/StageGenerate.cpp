#include "StageGenerate.h"
#include <cmath>

void Script_StageGenerate::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay

    Object Stage;
	FindObjectByName(_parentScene, "Stage", &Stage);
	FindChildObjectByName(Stage, "Object", &ParentObject);

    // ランダムジェネレータの初期化
    gen = std::mt19937(rd());

    // オブジェクトを配置
    if (!isPlaced && _parentScene != nullptr)
    {
        int placedCount = 0;
        int maxAttempts = objectCount * 20; // 無限ループ防止（試行回数上限）
        int attempts = 0;

        while (placedCount < objectCount && attempts < maxAttempts)
        {
            attempts++;

            // ランダムな位置を生成（オブジェクトがマップ範囲内に収まるように）
            float halfSizeX = objectSizeX / 2.0f;
            float halfSizeZ = objectSizeZ / 2.0f;

            float randomX = GetRandomFloat(mapMinX + halfSizeX, mapMaxX - halfSizeX);
            float randomZ = GetRandomFloat(mapMinZ + halfSizeZ, mapMaxZ - halfSizeZ);

            // 重なりチェック
            if (!CheckOverlap(randomX, randomZ, objectSizeX, objectSizeZ))
            {
                // ランダムに見た目タイプを選択（0, 1, 2のいずれか）
                int typeIndex = GetRandomInt(0, 2);

                // オブジェクトを配置
                PlaceObject(typeIndex, randomX, randomZ);

                // 配置済みリストに追加
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
}

void Script_StageGenerate::Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update
}

void Script_StageGenerate::EndPlay() {
    IScript::EndPlay();// EndPlay

    // クリーンアップ
    placedObjects.clear();
}

bool Script_StageGenerate::CheckOverlap(float x, float z, float sizeX, float sizeZ)
{
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

void Script_StageGenerate::PlaceObject(int typeIndex, float x, float z)
{
    // プレハブを取得
    Object prefabObj = nullptr;
    APIResult result = FindPrefabObjectByName(prefabNames[typeIndex], &prefabObj);

    if (result != PN_SUCCESS || prefabObj == nullptr)
    {
        // プレハブが見つからない場合はスキップ
        return;
    }

    // シーンにオブジェクトを追加（クローンを作成）
    Object clonedObj = nullptr;
    result = AddObjectToScene(_parentScene, prefabObj, &clonedObj);

    if (result != PN_SUCCESS || clonedObj == nullptr)
    {
        return;
    }
	ObjectParenthood(clonedObj,ParentObject);
    // 位置を設定（Y軸は0で固定）
    Float3 position = CreateFloat3(x, 0.0f, z);
    SetObjectPosition(clonedObj, position);
}

float Script_StageGenerate::GetRandomFloat(float min, float max)
{
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

int Script_StageGenerate::GetRandomInt(int min, int max)
{
    std::uniform_int_distribution<int> dis(min, max);
    return dis(gen);
}