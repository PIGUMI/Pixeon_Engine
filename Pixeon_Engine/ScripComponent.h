#pragma once
/*　実装概要
* 動的スクリプト用コンポーネント
* 前回のスクリプト用コンポーネントはファイルを複製し読み込んでいたが、i/o負荷が高いため
* 読み込んだスクリプトをメモリ上に保存し、動的にコンパイル・実行する方式に変更
*/

#include "Component.h"

class ScripComponent : public Component
{
public :
	void Init(Object* owner) override;
	void BeginPlay() override;
	void EditUpdate() override;
	void InGameUpdate() override;
	void UInit() override;
	void DrawInspector() override;

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;


private:
	ScripComponent();
	~ScripComponent();
};

