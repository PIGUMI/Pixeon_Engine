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

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

private:
	void DrawAnimator2DPopup();

private:
	std::vector<std::string> projectFiles_;
	int selectedProjectIndex_ = -1;
	std::vector<Animator2D*> _animators;
};
