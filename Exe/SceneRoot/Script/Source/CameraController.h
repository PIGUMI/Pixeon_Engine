#pragma once
#include "Include/IScript.h"

class Script_CameraController : public IScript {
public:
    void BeginPlay() override;
    void Update() override;
    void EndPlay() override;
private:
    GameObjectHandle _target;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_CameraController();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
