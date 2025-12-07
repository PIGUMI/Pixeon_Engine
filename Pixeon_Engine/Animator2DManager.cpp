#include "Animator2DManager.h"
#include "SettingManager.h"

Animator2DManager* Animator2DManager::instance = nullptr;

Animator2DManager* Animator2DManager::GetInstance()
{
	if (instance == nullptr)
	{
		instance = new Animator2DManager();
		instance->ResetAllAnimator2D();
	}
	return instance;
}

void Animator2DManager::DestroyInstance()
{
	if (instance != nullptr)
	{
		delete instance;
		instance = nullptr;
	}
}

Animator2D Animator2DManager::GetAnimator2D(const std::string& name)
{
	for (auto animator : animator2Ds)
	{
		if (animator->GetProjectName() == name)
		{
			return *animator;
		}
	}
	Animator2D* newAnimator = new Animator2D();
	newAnimator->LoadFile(SettingManager::GetInstance()->GetAnimator2DProjectFilePath() + name);
	animator2Ds.push_back(newAnimator);
	return *newAnimator;
}

void Animator2DManager::ResetAllAnimator2D()
{
	for (auto animator : animator2Ds)
	{
		delete animator;
	}
	animator2Ds.clear();
}
