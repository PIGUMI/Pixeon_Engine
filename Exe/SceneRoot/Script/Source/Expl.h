#pragma once
#include "Include/IScript.h"

class Script_Expl : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;
private:
	Animator2d _animator;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Expl();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
