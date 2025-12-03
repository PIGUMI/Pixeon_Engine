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
	bool Active = false;			// アクティブ状態
	int Layer = 0;					// レイヤー
	float StartTime = 0.0f;			// 開始秒
	float EndTime = 0.0f;			// 終了秒
	CurveData CurveInfo;			// ベジェ情報
	UITransform StartTransform;		// 開始トランスフォーム
	UITransform EndTransform;		// 終了トランスフォーム
	ImageRender* Image = nullptr;	// 描画対象（ランタイム解決）
};

inline vec2 operator+(const vec2& a, const vec2& b) { return { a.x + b.x, a.y + b.y }; }
inline vec2 operator-(const vec2& a, const vec2& b) { return { a.x - b.x, a.y - b.y }; }
inline vec2 operator*(const vec2& v, float s) { return { v.x * s, v.y * s }; }
inline bool operator==(const vec2& a, const vec2& b) noexcept
{
	return a.x == b.x && a.y == b.y;
}
inline bool operator!=(const vec2& a, const vec2& b) noexcept
{
	return !(a == b);
}

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
	UIAnimatoData();
	~UIAnimatoData();
	void Update();
	void Draw();

	void SaveFile();
	void LoadFile();
	
	void AddKeyFrame(const KeyFrame& keyframe);
	std::vector<KeyFrame> GetKeyFrames() { return KeyFrames_; }
	std::vector<KeyFrame>* GetKeyFramePtr() { return &KeyFrames_; }
	void SetTotaltime(float total) { fTotalDuration_ = total; }
	void SetProjectName(const std::string& name) { Name_ = name; }
	void GetProjectName(const std::string& name) { Name_ = name; }
	void SetLoop(bool loop) { bLoop_ = loop; }


private:
	// ベジェによるイージング（elapsed: 経過秒, duration: 区間秒）
	vec2 EaseByBezierCurve(const CurveData& curve, const vec2& startvalue, const vec2& endvalue, float elapsed, float duration = 1.0f);

public:
	std::string Name_ = "UIAnimation"; // アニメーション名
	bool bLoop_ = false; // ループ
	bool bFirst_ = true; // 初回フラグ
	float fStartTime_ = 0.0f; // 再生開始時刻（秒）
	float fNowTime_ = 0.0f;   // 現在の再生時刻（秒、0..total）
	float fTotalDuration_ = 0.0f; // 総再生時間（秒）
	std::vector<KeyFrame> KeyFrames_; // キーフレーム群
};