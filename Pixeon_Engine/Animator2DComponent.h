#pragma once
#include "Component.h"
#include "Animator2D.h"
#include <vector>

class Animator2DComponent : public AbstractComponent {
public:
	void Init(AbstractObject* Prt) override;
	void InGameUpdate() override;
	void EditUpdate() override;
	void Draw(int Layer) override;
	void UInit() override;

	void DrawInspector() override;

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;
	Animator2D* GetAnimator2D(const std::string& name);
private:
	void DrawAnimator2DPopup();
private:
	std::vector<std::string> projectFiles_;
	int selectedProjectIndex_ = -1;
	std::vector<std::string> animatorNames_;
	std::vector<ViewMode> animatorViewModes_;
	std::vector<Animator2D*> _animators;
};