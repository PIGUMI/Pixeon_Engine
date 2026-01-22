// HITTEST.h
#pragma once
#include "Include/IScript.h"
#include <unordered_map>

class Script_HITTEST : public IScript {
public:
    void BeginPlay() override;
    void Update() override;
    void EndPlay() override;

    void OnCollisionEnter(const APICollisionInfo* info);
    void OnCollisionStay(const APICollisionInfo* info);
    void OnCollisionExit(const APICollisionInfo* info);

private:
    Object thisObject;
    Component capsuleCollision;
    SceneHandle currentScene;
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_HITTEST();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}