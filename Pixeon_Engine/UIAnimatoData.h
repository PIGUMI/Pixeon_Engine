#pragma once
#include <vector>
#include "Struct.h"
#include "ImageRender.h"

struct vec2
{
	float x;
	float y;
};

struct CurveData
{
	vec2 StartPoint;
	vec2 ControlPoint1;
	vec2 ControlPoint2;
	vec2 EndPoint;
};

struct KeyFrame
{
	bool Active;				// アクティブ情報
	int Layer;					// 描画順
	float StartTime;			// キーフレームの開始時間
	float EndTime;				// キーフレームの終了時間
	CurveData CurveInfo;		// ベジェ曲線の情報
	Transform StartTransform;	// 開始時のTransform情報
	Transform EndTransform;		// 終了時のTransform情報
	UIInfo StartUIInfo;			// 開始時のUI情報
	UIInfo EndUIInfo;			//終了時のUI情報
	ImageRender* Image;			// 描画するイメージ
};

class UIAnimatoData
{
public:
	// 更新処理
	void Update();
	// 実際に描画する
	void Draw();

public:
	float TotalDuration = 0.0f; // アニメーションの総時間
	std::vector<KeyFrame> KeyFrames; // キーフレームのリスト
};

