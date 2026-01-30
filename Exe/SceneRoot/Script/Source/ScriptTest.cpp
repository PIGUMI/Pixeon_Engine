#include "ScriptTest.h"

void Script_ScriptTest::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay
	TestVar = 42;
	TestFloat = 3.14f;
}

void Script_ScriptTest:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update
}

void Script_ScriptTest::EndPlay() {
    IScript::EndPlay();// EndPlay
}
