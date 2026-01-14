/*
* ファイル名　Animator2DManager
* 説　　　明　Animator2Dの管理を行うシングルトンクラス
*/
#include "Animator2DManager.h"
#include "SettingManager.h"

Animator2DManager* Animator2DManager::_instance = nullptr;

/*
* 関数名　GetInstance
* 引　数　なし
* 戻り値　Animator2DManager*
* 説　明　Animator2DManagerのインスタンスを取得する
*/
Animator2DManager* Animator2DManager::GetInstance()
{
	if (_instance == nullptr)
	{
		_instance = new Animator2DManager();
		_instance->ResetAllAnimator2D();
	}
	return _instance;
}

/*
* 関数名　DestroyInstance
* 引　数　なし
* 戻り値　なし
* 説　明　Animator2DManagerのインスタンスを破棄する
*/
void Animator2DManager::DestroyInstance()
{
	if (_instance != nullptr)
	{
		_instance->ResetAllAnimator2D();
		delete _instance;
		_instance = nullptr;
	}
}

/*
* 関数名　GetAnimator2D
* 引　数　取得するAnimator2D名
* 戻り値　Animator2Dのポインタ
* 説　明　名前からAnimator2Dを取得する
*/
Animator2D* Animator2DManager::GetAnimator2D(const std::string& name)
{
	for (auto animator : _animator2Ds)
	{
		if (animator->GetProjectName() == name)
		{
			return animator->Copy();
		}
	}
	Animator2D* newAnimator = new Animator2D();
	newAnimator->LoadFile(SettingManager::GetInstance()->GetAnimator2DProjectFilePath() + name);
	_animator2Ds.push_back(newAnimator);
	return newAnimator->Copy();
}

/*
* 関数名　ResetAllAnimator2D
* 引　数　なし
* 戻り値　なし
* 説　明　管理している全てのAnimator2Dを破棄する
*/
void Animator2DManager::ResetAllAnimator2D()
{
	for (auto animator : _animator2Ds)
	{
		delete animator;
	}
	_animator2Ds.clear();
}

/*
* 関数名　RemoveAllAnimator2D
* 引　数　削除するAnimator2D名
* 戻り値　なし
* 説　明　指定した名前のAnimator2Dを全て破棄する
*/
void Animator2DManager::RemoveAllAnimator2D(std::string DeleteName)
{
	for (auto it = _animator2Ds.begin(); it != _animator2Ds.end(); )
	{
		if ((*it)->GetProjectName() == DeleteName)
		{
			delete* it;
			it = _animator2Ds.erase(it);
		}
		else
		{
			++it;
		}
	}
}