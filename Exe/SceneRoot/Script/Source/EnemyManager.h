#pragma once
#include "Include/IScript.h"

class Script_EnemyManager : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;
private:
	float GetRandomFloat(float min, float max);
    void CalcEnemySpawnPos(float playerX, float playerZ, float& outX, float& outZ);
private:
    Object _Enemy;
	Object _Player;
    int _EnemyCount;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_EnemyManager();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
