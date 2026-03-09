#pragma once
#include "Include/IScript.h"

class Script_Player : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;
public:
#define PROPERTY_LIST(ACTION)

    DECLARE_SCRIPT_PROPERTIES()
#undef PROPERTY_LIST
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Player();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
