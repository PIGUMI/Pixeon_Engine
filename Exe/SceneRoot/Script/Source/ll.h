#pragma once
#include "Include/IScript.h"

class Script_ll : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_ll();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
