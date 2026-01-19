#pragma once
#include "Include/IScript.h"

class Script_Player : public IScript {
public:
    void BeginPlay() override;
    void Update() override;
    void EndPlay() override;
private:
    void Movement();
private:
    int hitPoint = 50;
    float walkTimer = 0.0f;
    bool isMoving = false;
private:
    Object playerObject;
    Object headObject;
    Object bodyObject;
    Object HitPointUI;
    Animator2d HP;
    Keyframe hp;
    Component CameraComp;
    Object Bullet;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Player();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}