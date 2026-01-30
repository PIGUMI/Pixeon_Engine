#pragma once
#include "Include/IScript.h"

class Script_ScriptTest : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;

public:
	int TestVar = 0;
	float TestFloat = 0.0f;
public:
#define PROPERTY_LIST(ACTION) \
    ACTION(INT, TestVar) \
    ACTION(FLOAT, TestFloat)

    DECLARE_SCRIPT_PROPERTIES()
#undef PROPERTY_LIST
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_ScriptTest();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
