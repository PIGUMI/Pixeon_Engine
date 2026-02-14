#pragma once
#include "Include/IScript.h"

class Script_ReticleAnimation : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;
public:
	Animator2dProject* animatorProject;
	std::string KeyFrameName;
public:
#define PROPERTY_LIST(ACTION) \
    ACTION(STRING,KeyFrameName)

    DECLARE_SCRIPT_PROPERTIES()
#undef PROPERTY_LIST
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_ReticleAnimation();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
