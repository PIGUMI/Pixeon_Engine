#include "PlayerMove.h"

void Script_PlayerMove::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay
	FindComponent(_parentObject, "Animation", &_animator);
    NowAnimationState = AnimationState;
}

void Script_PlayerMove:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update

	AnimationState = 0;

    if(KeyPressed('W')){
        Float3 pos;
        GetObjectPosition(_parentObject, &pos);
        pos.z += MoveSpeed * DeltaTime;
        SetObjectPosition(_parentObject,pos);
		AnimationState = 1;
	}
    if(KeyPressed('S')){
        Float3 pos;
        GetObjectPosition(_parentObject, &pos);
        pos.z -= MoveSpeed * DeltaTime;
		SetObjectPosition(_parentObject,pos);
		AnimationState = 1;
    }
    if(KeyPressed('A')){
        Float3 pos;
        GetObjectPosition(_parentObject, &pos);
		pos.x -= MoveSpeed * DeltaTime;
        SetObjectPosition(_parentObject,pos);
		AnimationState = 1;
    }
    if (KeyPressed('D')) {
        Float3 pos;
        GetObjectPosition(_parentObject, &pos);
        pos.x += MoveSpeed * DeltaTime;
        SetObjectPosition(_parentObject,pos);
		AnimationState = 1;
    }

    if(NowAnimationState != AnimationState){
        SetAnimationClip(_animator, AnimationState);
		NowAnimationState = AnimationState;
	}

}

void Script_PlayerMove::EndPlay() {
    IScript::EndPlay();
}
