#pragma once
#include "Include/IScript.h"

class Script_attachWeapon : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;
private:
    ModelRender* Model = nullptr;
	DirectX::XMFLOAT3 OffsetPosition = { 0.0f,0.0f,0.0f };
public:
    std::string AttachObjectName = "Model";
	int BoneIndex = -1;
#define PROPERTY_LIST(ACTION) \
    ACTION(STRING,AttachObjectName) \
    ACTION(INT,BoneIndex)

    DECLARE_SCRIPT_PROPERTIES()
#undef PROPERTY_LIST
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_attachWeapon();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
