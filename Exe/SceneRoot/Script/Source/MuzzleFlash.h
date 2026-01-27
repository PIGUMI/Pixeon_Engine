#pragma once
#include "Include/IScript.h"

class Script_MuzzleFlash : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;
private:
	Component _effectComp = nullptr;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_MuzzleFlash();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
