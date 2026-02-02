#pragma once
#include "Include/IScript.h"

class Script_Title : public IScript {
public:
    void BeginPlay() override;
    void Update(float DeltaTime) override;
    void EndPlay() override;
public:
	int SelectIndex = 0;
	std::string Solo = "Title_Solo";
	std::string Multi = "Title_Multi";

private:
	Object _SelectObject = nullptr;
public:
#define PROPERTY_LIST(ACTION) \
    ACTION(INT, SelectIndex) \
    ACTION(STRING , Solo) \
    ACTION(STRING , Multi)

    DECLARE_SCRIPT_PROPERTIES()
#undef PROPERTY_LIST
};

extern "C" __declspec(dllexport) IScript* CreateScriptInstance() {
    return new Script_Title();
}

extern "C" __declspec(dllexport) void DestroyScriptInstance(IScript* script) {
    delete script;
}
