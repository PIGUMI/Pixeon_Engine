#pragma once
#include "Include/IScript.h"

class Script_Player : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;
private:
	void Movement(float DeltaTime);
public:
    float _Sensitivity = 0.005f;
    float _LimitAngle = 70.0f;
    float _MoveSpeed = 0.1f;
private:
    float _walkTimer = 0.0f;
    bool _isMoving = false;
    Object* _Head;
	Object* _Body;
	Camera* _Camera;
public:
#define PROPERTY_LIST(ACTION) \
    ACTION(FLOAT, _Sensitivity) \
        ACTION(FLOAT, _LimitAngle) \
        ACTION(FLOAT, _MoveSpeed)
    DECLARE_SCRIPT_PROPERTIES()
#undef PROPERTY_LIST
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Player();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
