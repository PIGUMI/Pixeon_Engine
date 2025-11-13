#pragma once
#include "Include/IScript.h"

class Script_NeoSkyBox : public IScript {
public:
    void BeginPlay() override;
    void Update() override;
    void EndPlay() override;
private:
	GameObjectHandle skyboxObject;
	GameObjectHandle mainCameraObject;
    ComponentHandle CameraComp;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_NeoSkyBox();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
