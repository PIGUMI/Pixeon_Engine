#pragma once
#include "Component.h"
#include "Animator2D.h"
#include <vector>

class Animator2DComponent : public Component
{
public:
	void Init(Object* Prt) override;
	void InGameUpdate() override;
	void Draw() override;

	void DrawInspector() override;

private:
	std::vector<Animator2D*> _animators;
};
