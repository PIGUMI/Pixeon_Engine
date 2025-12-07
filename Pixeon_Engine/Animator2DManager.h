#pragma once
#include "Animator2D.h"

class Animator2DManager
{
public:
	static Animator2DManager* GetInstance();
	static void DestroyInstance();
public:
	Animator2D GetAnimator2D(const std::string& name);
	void ResetAllAnimator2D();
private:
	std::vector<Animator2D*> animator2Ds;
private:
	Animator2DManager() = default;
	~Animator2DManager() = default;
	static Animator2DManager* instance;
};
