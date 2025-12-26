#pragma once
#include "Include/IScript.h"

enum PlayerState {
    Idle,
    Walking,
    Running
};

class Script_PlayerMove : public IScript {
public:
    void BeginPlay() override;
    void Update() override;
    void EndPlay() override;
private:
    Object player;
    Component Animation;
    Component Camera;
    Component rigidBody;
	PlayerState currentState = Idle;
	PlayerState previousState = Idle;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_PlayerMove();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
