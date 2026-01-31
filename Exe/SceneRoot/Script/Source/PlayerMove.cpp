#include "PlayerMove.h"

void Script_PlayerMove::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay
}

void Script_PlayerMove:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update

    if(KeyPressed('W')){
        Float3 pos;
        GetObjectPosition(_parentObject, &pos);
        pos.z += MoveSpeed * DeltaTime;
        SetObjectPosition(_parentObject,pos);
	}
    if(KeyPressed('S')){
        Float3 pos;
        GetObjectPosition(_parentObject, &pos);
        pos.z -= MoveSpeed * DeltaTime;
		SetObjectPosition(_parentObject,pos);
    }
    if(KeyPressed('A')){
        Float3 pos;
        GetObjectPosition(_parentObject, &pos);
		pos.x -= MoveSpeed * DeltaTime;
        SetObjectPosition(_parentObject,pos);
    }
    if (KeyPressed('D')) {
        Float3 pos;
        GetObjectPosition(_parentObject, &pos);
        pos.x += MoveSpeed * DeltaTime;
        SetObjectPosition(_parentObject,pos);
    }


}

void Script_PlayerMove::EndPlay() {
    IScript::EndPlay();// EndPlay
}
