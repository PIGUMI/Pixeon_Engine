#pragma once
#include "Include/IScript.h"

class Script_HitMarker : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;
public:
	float hitMarkerDuration = 0.2f; // ヒットマーカーの表示時間（秒）
public:
#define PROPERTY_LIST(ACTION) \
    ACTION(FLOAT, hitMarkerDuration)

    DECLARE_SCRIPT_PROPERTIES()
#undef PROPERTY_LIST
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_HitMarker();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
