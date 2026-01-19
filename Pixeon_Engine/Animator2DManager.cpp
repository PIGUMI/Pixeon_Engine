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
	// キャッシュから検索
	for (auto animator : _animator2Ds)
	{
		if (animator->GetProjectName() == name)
		{
			return animator->Copy();
		}
	}

	// キャッシュにない場合は新規ロード
	Animator2D* newAnimator = new Animator2D();
	newAnimator->LoadFile(SettingManager::GetInstance()->GetAnimator2DProjectFilePath() + name);
	_animator2Ds.push_back(newAnimator);
	return newAnimator->Copy();
}

/*
* 関数名　PreloadAnimator2D
* 引　数　プリロードするAnimator2D名
* 戻り値　なし
* 説　明　指定されたAnimator2Dを事前にロードしてキャッシュする
*/
void Animator2DManager::PreloadAnimator2D(const std::string& name)
{
	auto start = std::chrono::high_resolution_clock::now();

	for (auto animator : _animator2Ds)
	{
		if (animator->GetProjectName() == name)
		{
			auto end = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

			std::string msg = "[PreloadAnimator2D] Cache hit for " + name + " (" + std::to_string(duration.count()) + "us)\n";
			OutputDebugStringA(msg.c_str());

			return;
		}
	}
	Animator2D* newAnimator = new Animator2D();
	newAnimator->LoadFile(SettingManager::GetInstance()->GetAnimator2DProjectFilePath() + name);
	_animator2Ds.push_back(newAnimator);

	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

	std::string msg = "[PreloadAnimator2D] NEW LOAD for " + name + " took " + std::to_string(duration.count()) + "ms\n";
	OutputDebugStringA(msg.c_str());
}

/*
* 関数名　IsLoaded
* 引　数　確認するAnimator2D名
* 戻り値　ロード済みならtrue
* 説　明　指定されたAnimator2Dがキャッシュされているか確認する
*/
bool Animator2DManager::IsLoaded(const std::string& name)
{
	for (auto animator : _animator2Ds)
	{
		if (animator->GetProjectName() == name)
		{
			return true;
		}
	}
	return false;
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