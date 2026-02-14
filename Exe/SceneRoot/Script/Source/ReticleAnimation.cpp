#include "ReticleAnimation.h"

void Script_ReticleAnimation::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay
    Animator2d* animator;
	animator = _parentObject->GetComponent<Animator2d>("Animator2DComponent");
	animatorProject = animator->GetAnimator2D(KeyFrameName);

    if (animator)
    {
		delete animator;
		animator = nullptr;
    }
}

void Script_ReticleAnimation::Update(float DeltaTime) {
    IScript::Update(DeltaTime);// 
    if (animatorProject)
    {
        if (animatorProject->IsEnd())
        {
			_parentScene->RemoveObject(_parentObject);
        }
	}
}

void Script_ReticleAnimation::EndPlay() {
    IScript::EndPlay();// EndPlay
    if (animatorProject)
    {
		delete animatorProject;
		animatorProject = nullptr;
    }
}
