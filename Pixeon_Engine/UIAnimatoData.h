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

struct UITransform
{
	vec2 Position;
	vec2 Rotation;
	vec2 Scale;
	vec2 UVScale;
	vec2 UVPostion;
};

struct KeyFrame
{
	bool Active;				// アクティブ情報
	int Layer;					// 描画順
	float StartTime;			// キーフレームの開始時間
	float EndTime;				// キーフレームの終了時間
	CurveData CurveInfo;		// ベジェ曲線の情報
	UITransform StartTransform; // 開始時のトランスフォーム
	UITransform EndTransform;   // 終了時のトランスフォーム
	ImageRender* Image;			// 描画するイメージ
};

inline vec2 operator+(const vec2& a, const vec2& b) { return { a.x + b.x, a.y + b.y }; }
inline vec2 operator-(const vec2& a, const vec2& b) { return { a.x - b.x, a.y - b.y }; }
inline vec2 operator*(const vec2& v, float s) { return { v.x * s, v.y * s }; }
inline float Length(const vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

inline vec2 EvalCubicBezier(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float t)
{
	float u = 1.0f - t;
	float tt = t * t;
	float uu = u * u;
	float uuu = uu * u;
	float ttt = tt * t;

	vec2 p = { 0.0f, 0.0f };
	p.x = uuu * p0.x + 3.0f * uu * t * p1.x + 3.0f * u * tt * p2.x + ttt * p3.x;
	p.y = uuu * p0.y + 3.0f * uu * t * p1.y + 3.0f * u * tt * p2.y + ttt * p3.y;
	return p;
}

class UIAnimatoData
{
public:
	// 更新処理
	void Update();
	// 実際に描画する
	void Draw();

private:
	// ベジェ情報、開始位置、終了位置、経過秒数、総合時間
	vec2 EaseByBezierCurve(const CurveData& curve,const vec2& startPos,const vec2& endPos,float elapsed,float duration = 1.0f);

public:
	float NowTime;
	float TotalDuration = 0.0f; // アニメーションの総時間
	std::vector<KeyFrame> KeyFrames; // キーフレームのリスト
};

