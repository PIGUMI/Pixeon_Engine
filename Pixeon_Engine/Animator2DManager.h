#pragma once
#include "Animator2D.h"
#include <string>
#include <vector>

class Animator2DManager
{
public:
	static Animator2DManager* GetInstance();
	static void DestroyInstance();
public:
	Animator2D* GetAnimator2D(const std::string& name);
	void PreloadAnimator2D(const std::string& name);
	bool IsLoaded(const std::string& name);
	void ResetAllAnimator2D();
	void RemoveAllAnimator2D(std::string DeleteName);
private:
	std::vector<Animator2D*> _animator2Ds;
private:
	Animator2DManager() = default;
	~Animator2DManager() = default;
	static Animator2DManager* _instance;
};