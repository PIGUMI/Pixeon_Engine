#pragma once
#include "Include/IScript.h"

class Script_PlayerMove : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;

public:
	float MoveSpeed = 5.0f;
public:
#define PROPERTY_LIST(ACTION) \
    ACTION(FLOAT, MoveSpeed)

    DECLARE_SCRIPT_PROPERTIES()
#undef PROPERTY_LIST
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_PlayerMove();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
