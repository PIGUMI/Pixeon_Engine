#pragma once
#include "Include/IScript.h"

class Script_Test : public IScript {
public:
    void BeginPlay() override;
    void Update() override;
    void EndPlay() override;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Test();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
