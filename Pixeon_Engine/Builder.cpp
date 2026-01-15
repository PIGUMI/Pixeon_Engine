#include "Builder.h"

Builder* Builder::_instance = nullptr;

/*
* 関数名　:GetInstance
* 引　数　:なし
* 戻り値　:インスタンスポインター
* 概　要　:Builderクラスのインスタンスを取得する
*/
Builder* Builder::GetInstance()
{
	if (_instance == nullptr)
	{
		_instance = new Builder();
	}
	return _instance;
}

/*
* 関数名　:DeleteInstance
* 引　数　:なし
* 戻り値　:なし
* 概　要　:Builderクラスのインスタンスを削除する
*/
void Builder::DeleteInstance()
{
	if (_instance != nullptr)
	{
		delete _instance;
		_instance = nullptr;
	}
}

/*
* 関数名　:Init
* 引　数　:なし
* 戻り値　:なし
* 概　要　:ビルド設定の初期化を行う
*/
void Builder::Init()
{
	_builderData.gameName = "MyGame";
	_builderData.startScene = "MainScene";
	_builderData.gameVersion = "1.0.0";
}

/*
* 関数名　:Build
* 引　数　:なし
* 戻り値　:成功なら0、失敗なら-1
* 概　要　:ビルド処理を行う
*/
int Builder::Build()
{
}

