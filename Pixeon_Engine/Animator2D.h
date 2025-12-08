#pragma once
#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include "Struct.h"
#include "ImageRender.h"

/* ベジェ曲線情報 */
struct CurveData
{
	DirectX::XMFLOAT2 StartPoint	= { 0.0f,0.0f };
	DirectX::XMFLOAT2 ControlPoint1 = { 0.0f,0.0f};
	DirectX::XMFLOAT2 ControlPoint2 = { 1.0f,1.0f };
	DirectX::XMFLOAT2 EndPoint		= { 1.0f,1.0f };
};

// 2Dベクトル
struct UITransform
{
	DirectX::XMFLOAT2 Position	= { 0.0f,0.0f };
	DirectX::XMFLOAT2 Rotation	= { 0.0f,0.0f };
	DirectX::XMFLOAT2 Scale			= { 100.0f,100.0f };
	DirectX::XMFLOAT2 UVScale		= { 1.0f,1.0f };
	DirectX::XMFLOAT2 UVPosition	= { 0.0f,0.0f };
};

// キーフレーム情報
struct KeyFrame
{
	bool Active = false;			// アクティブ状態
	int Layer = 0;					// レイヤー
	float StartTime = 0.0f;			// 開始秒
	float EndTime = 1.0f;			// 終了秒
	CurveData CurveInfo;			// ベジェ情報
	UITransform StartTransform;		// 開始トランスフォーム
	UITransform EndTransform;		// 終了トランスフォーム
	UITransform NowTransform;		// 現在トランスフォーム（編集用）
	ImageRender* Image = nullptr;	// 描画対象（ランタイム解決）
	std::string Texture;			// テクスチャ名（保存用）
};

inline float Length(const DirectX::XMFLOAT2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

inline DirectX::XMFLOAT2 EvalCubicBezier(const DirectX::XMFLOAT2& p0, const DirectX::XMFLOAT2& p1, const DirectX::XMFLOAT2& p2, const DirectX::XMFLOAT2& p3, float t)
{
	float u = 1.0f - t;
	float tt = t * t;
	float uu = u * u;
	float uuu = uu * u;
	float ttt = tt * t;

	DirectX::XMFLOAT2 p = { 0.0f, 0.0f };
	p.x = uuu * p0.x + 3.0f * uu * t * p1.x + 3.0f * u * tt * p2.x + ttt * p3.x;
	p.y = uuu * p0.y + 3.0f * uu * t * p1.y + 3.0f * u * tt * p2.y + ttt * p3.y;
	return p;
}
// Animator2D ProjectData
class Animator2D
{
public:
	enum ViewMode
	{
		UI,
		Billboard,
	};
public:
	Animator2D();
	~Animator2D();
	void Update();
	void EditorUpdate();
	void Draw();
	void Debug();

	void SaveFile();
	void LoadFile(std::string FilePath);
	void LoadCache(std::ifstream& in);

	void AddKeyFrame(const KeyFrame& keyframe);
	void RemoveKeyFrame(KeyFrame* ptr);

	std::vector<KeyFrame> GetKeyFrames() { return KeyFrames_; }
	std::vector<KeyFrame>* GetKeyFramePtr() { return &KeyFrames_; }

	void SetTotalTime(float total) { fTotalDuration_ = total; }
	float GetTotalTime() const { return fTotalDuration_; }

	void SetProjectName(const std::string& name) { Name_ = name; }
	std::string GetProjectName() { return Name_; }

	void SetLoop(bool loop) { bLoop_ = loop; }
	bool GetLoop() const { return bLoop_; }

	void SetViewMode(ViewMode mode) { viewMode_ = mode; }
	ViewMode GetViewMode() const { return viewMode_; }

	void SetFirstFlag(bool first) { bFirst_ = first; }
	bool GetEndedFlag() const { return bEnded_; }

private:
	void KeyFrameUpdate();
	// ベジェによるイージング
	DirectX::XMFLOAT2 EaseByBezierCurve(const CurveData& curve, const DirectX::XMFLOAT2& startvalue, const DirectX::XMFLOAT2& endvalue, float elapsed, float duration = 1.0f);

public:
	std::string Name_ = "Animator2D"; // アニメーション名
	bool bLoop_ = false;// ループ
	bool bFirst_ = true; // 初回フラグ
	bool bEnded_ = false; // 再生終了フラグ
	float fStartTime_ = 0.0f; // 再生開始時刻（秒）
	float fNowTime_ = 0.0f;   // 現在の再生時刻（秒）
	float fTotalDuration_ = 0.0f; // 総再生時間（秒）
	std::vector<KeyFrame> KeyFrames_; // キーフレーム群
	ViewMode viewMode_ = ViewMode::UI;
	int DrawCount = 0;
};