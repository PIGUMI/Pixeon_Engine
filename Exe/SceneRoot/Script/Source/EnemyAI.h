#pragma once
#include "Include/IScript.h"

class Script_EnemyAI : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;
public:
	bool IsCheckingPlayer = false; // ÉvÉåÉCÉÑÅ[ÇíTÇµÇƒÇ¢ÇÈÇ©
	float Y = 0.0f; // âÒì]äpìx
public:
#define PROPERTY_LIST(ACTION) \
    ACTION(BOOL,IsCheckingPlayer) \
    ACTION(FLOAT,Y) \

    DECLARE_SCRIPT_PROPERTIES()
#undef PROPERTY_LIST
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_EnemyAI();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
