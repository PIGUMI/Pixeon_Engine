#pragma once
#include "Include/IScript.h"

class Script_PlayerMove : public IScript {
public:
    void BeginPlay() override;
    void Update() override;
    void EndPlay() override;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_PlayerMove();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
