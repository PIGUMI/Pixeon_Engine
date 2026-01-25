#pragma once
#include "Include/IScript.h"

class Script_Weapon : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;

private:
    void Animation();
private:
    transform _idleTransform;
    transform _ReadyTransform;
    float _transitionProgress;
    bool _isAiming = false;
    int _coolTime;

    Object _ExplosionEffect;
    Object _Body;

};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Weapon();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
