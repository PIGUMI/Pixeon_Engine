#pragma once
#include "Include/IScript.h"

class Script_Net : public IScript {
public:
    void BeginPlay() override;
    void Update() override;
    void EndPlay() override;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Net();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
