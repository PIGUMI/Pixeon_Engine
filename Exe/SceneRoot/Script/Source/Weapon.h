#pragma once
#include "Include/IScript.h"

class Script_Weapon : public IScript {
public:
    void BeginPlay() override;
    void Update() override;
    void EndPlay() override;
    void Animation();
    void Effect();
private:
    transform _StartTransform;
    transform _EndTransform;
    float _transitionProgress = 0.0f;
    bool _isAiming = false;
    Object Player;
    Object Head;
    SceneHandle scene;
    Component CameraComp;
    Object Bullet;

	Object ExplosionEffect;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Weapon();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}