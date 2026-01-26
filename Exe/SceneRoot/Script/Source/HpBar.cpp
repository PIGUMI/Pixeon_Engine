#include "HpBar.h"
#include <Windows.h>
#include <string>

void Script_HpBar::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay
	FindObjectByName(_parentScene, "Player", &playerObject);

	FindComponent(_parentObject,"Animator2DComponent",&AnimatorComp);
	GetAnimator2D(AnimatorComp,"HP",&project);
	FindKeyFrame(project,"Hp",&Hp);

}

void Script_HpBar:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update

	int hp;
	GetVariableInt(playerObject, "HP", &hp);
	if(currentHp != hp){
		currentHp = hp;
		float fHP;
		fHP = (float)hp - 50.0f;
		SetVertexOffsetRight(Hp,fHP);
	}

	if (KeyPressed('P'))
	{
		float fHP;
		FindKeyFrame(project, "Hp", &Hp);
		GetVertexOffsetRight(Hp, &fHP);
		fHP += 1.0f;
		SetVertexOffsetRight(Hp, fHP);
	}
	if( KeyPressed('O'))
	{
		float fHP;
		FindKeyFrame(project, "Hp", &Hp);
		GetVertexOffsetRight(Hp, &fHP);
		fHP -= 1.0f;
		SetVertexOffsetRight(Hp, fHP);
	}
}

void Script_HpBar::EndPlay() {
    IScript::EndPlay();// EndPlay
}
