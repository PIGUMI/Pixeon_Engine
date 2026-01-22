#pragma once
#include "Include/IScript.h"

class Script_Bullet : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;

private:
    int         LifeTime = 10;
    SceneHandle _scene = nullptr;
    Component   _rigidBody = nullptr;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Bullet();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}