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

Animator2D* Animator2DManager::GetAnimator2D(const std::string& name)
{
	for (auto animator : animator2Ds)
	{
		if (animator->GetProjectName() == name)
		{
			return animator->Copy();
		}
	}
	Animator2D* newAnimator = new Animator2D();
	newAnimator->LoadFile(SettingManager::GetInstance()->GetAnimator2DProjectFilePath() + name);
	animator2Ds.push_back(newAnimator);
	return newAnimator->Copy();
}

void Animator2DManager::ResetAllAnimator2D()
{
	for (auto animator : animator2Ds)
	{
		delete animator;
	}
	animator2Ds.clear();
}

void Animator2DManager::RemoveAllAnimator2D(std::string DeleteName)
{
	for (auto it = animator2Ds.begin(); it != animator2Ds.end(); )
	{
		if ((*it)->GetProjectName() == DeleteName)
		{
			delete *it;
			it = animator2Ds.erase(it);
		}
		else
		{
			++it;
		}
	}
}
