#include "Expl.h"

void Script_Expl::BeginPlay() {
	IScript::BeginPlay();
    Component comp;
	FindComponent(_parentObject, "Animator2DComponent", &comp);
	GetAnimator2D(comp,"Explosion",& _animator);
}

void Script_Expl:: Update() {
	bool isEnd = false;
	Animator2DIsEnd(_animator, &isEnd);
	if (isEnd) {
		SceneHandle scene;
		GetCurrentScene(&scene);
		RemoveObjectFromScene(scene,_parentObject);
	}
}

void Script_Expl::EndPlay() {
    // EndPlay
}
