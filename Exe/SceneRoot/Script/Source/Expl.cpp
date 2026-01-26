#include "Expl.h"

void Script_Expl::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay
    Component Anim;
    FindComponent(_parentObject, "Animator2DComponent",&Anim);
    GetAnimator2D(Anim, "Explosion", &Exlp);
}

void Script_Expl:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update

    bool isEnd = false;
    Animator2DIsEnd(Exlp, &isEnd);
    if (isEnd)
    {
        RemoveObjectFromScene(_parentScene, _parentObject);
    }
}

void Script_Expl::EndPlay() {
    IScript::EndPlay();// EndPlay
}
