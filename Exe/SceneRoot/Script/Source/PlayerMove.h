#pragma once
#include "Include/IScript.h"

class Script_PlayerMove : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;

private:
    void Movement(float DeltaTime);
    
private:
	float _Sensitivity;
	float _LimitAngle;
	float _MoveSpeed;
	float _walkTimer;
    bool _isMoving;
    Object _Head;
	Object _Body;
    Component _Camera;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_PlayerMove();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
