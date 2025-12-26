#pragma once
#include "Include/IScript.h"

class Script_SkyBox : public IScript {
public:
    void BeginPlay() override;
    void Update() override;
    void EndPlay() override;
private:
    Object player;
	Object skybox;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_SkyBox();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
