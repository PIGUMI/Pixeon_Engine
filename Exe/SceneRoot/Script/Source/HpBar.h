#pragma once
#include "Include/IScript.h"

class Script_HpBar : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;
private:
	Object playerObject;
	Component AnimatorComp;
    Animator2d project;
    Keyframe Hp;
	int currentHp;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_HpBar();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
