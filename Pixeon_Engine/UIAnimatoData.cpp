#include "UIAnimatoData.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <algorithm>

// 現在時刻（秒）を返すヘルパー
static double GetTimeSeconds()
{
	using clock = std::chrono::steady_clock;
	auto now = clock::now();
	auto epoch = now.time_since_epoch();
	return std::chrono::duration_cast<std::chrono::duration<double>>(epoch).count();
}

UIAnimatoData::UIAnimatoData()
	:bLoop_(false),
  bFirst_(true),
  fStartTime_(0.0f),
  fNowTime_(0.0f),
  fTotalDuration_(0.0f)
{
}
UIAnimatoData::~UIAnimatoData()
{
	for(auto obj : KeyFrames_)
	{
		delete obj.Image;
		obj.Image = nullptr;
	}
}

void UIAnimatoData::Update()
{
	// 現在時刻取得（秒）
	double nowSec = GetTimeSeconds();

	if (bFirst_)
	{
		bFirst_ = false;
		fStartTime_ = static_cast<float>(nowSec);
	}

	// 再生ヘッド（秒）
	fNowTime_ = static_cast<float>(nowSec - static_cast<double>(fStartTime_));

	// ループ処理: 総時間を超えたらループ開始
	if (fTotalDuration_ > 0.0f && fNowTime_ >= fTotalDuration_)
	{
		if (bLoop_)
		{
			// 再生開始を現在時刻に合わせる（playhead を 0 に戻す）
			fStartTime_ = static_cast<float>(nowSec);
			fNowTime_ = 0.0f;
		}
		else
		{
			// 末尾に固定
			fNowTime_ = fTotalDuration_;
		}
	}

	// すべてのキーフレームを参照でループし、アクティブ判定と適用
	for (auto& obj : KeyFrames_)
	{
		// アクティブ条件：StartTime <= now <= EndTime
		obj.Active = (fNowTime_ >= obj.StartTime && fNowTime_ <= obj.EndTime);

		if (!obj.Active) continue;
		if (obj.Image == nullptr) continue;

		// 区間の経過と長さ
		float elapsed = fNowTime_ - obj.StartTime;
		float duration = obj.EndTime - obj.StartTime;
		// duration が 0 なら瞬時に最終値を適用
		float tNormalized = 0.0f;
		if (duration <= 0.0f) tNormalized = 1.0f;
		else tNormalized = std::clamp(elapsed / duration, 0.0f, 1.0f);

		// 位置の補間（ベジェ）
		if (obj.StartTransform.Position != obj.EndTransform.Position)
		{
			vec2 pos = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.Position, obj.EndTransform.Position, elapsed, duration);
			DirectX::XMFLOAT2 Pos;
			Pos.x = pos.x;
			Pos.y = pos.y;
			obj.Image->SetOffset2D(Pos);
		}

		if (obj.StartTransform.Rotation != obj.EndTransform.Rotation)
		{
			vec2 rot = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.Rotation, obj.EndTransform.Rotation, elapsed, duration);
			// TODO: ImageRender に回転を適用する API があれば呼ぶ
			// 例: obj.Image->SetRotation(rot.x);
			(void)rot; // 未使用時の警告回避
		}

		// スケール
		if (obj.StartTransform.Scale != obj.EndTransform.Scale)
		{
			vec2 scl = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.Scale, obj.EndTransform.Scale, elapsed, duration);
			DirectX::XMFLOAT2 Scl;
			Scl.x = scl.x;
			Scl.y = scl.y;
			obj.Image->SetSize2D(Scl);
		}

		// UV 位置
		if (obj.StartTransform.UVPostion != obj.EndTransform.UVPostion)
		{
			vec2 uvp = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.UVPostion, obj.EndTransform.UVPostion, elapsed, duration);
			DirectX::XMFLOAT4 uvRect = obj.Image->GetUVRect();
			uvRect.x = uvp.x;
			uvRect.y = uvp.y;
			obj.Image->SetUVRect(uvRect);
		}

		// UV スケール
		if (obj.StartTransform.UVScale != obj.EndTransform.UVScale)
		{
			vec2 uvs = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.UVScale, obj.EndTransform.UVScale, elapsed, duration);
			DirectX::XMFLOAT4 uvRect = obj.Image->GetUVRect();
			uvRect.z = uvs.x;
			uvRect.w = uvs.y;
			obj.Image->SetUVRect(uvRect);
		}
	}
}

void UIAnimatoData::SaveFile()
{
}

void UIAnimatoData::LoadFile()
{
}

void UIAnimatoData::AddKeyFrame(const KeyFrame& keyframe)
{
	KeyFrames_.push_back(keyframe);
}

vec2 UIAnimatoData::EaseByBezierCurve(const CurveData& curve, const vec2& startvalue, const vec2& endvalue, float elapsed, float duration)
{
	// elapsed と duration から 0..1 の t を計算
	float t;
	if (duration <= 0.0f) {
		// 瞬時適用
		t = 1.0f;
	}
	else {
		t = elapsed / duration;
	}

	if (t <= 0.0f) return startvalue;
	if (t >= 1.0f) return endvalue;

	// オーサーが定義したベジェの長さを現在のセグメント長にスケールして運用する方針
	vec2 authoredVec = curve.EndPoint - curve.StartPoint;
	vec2 currentVec = endvalue - startvalue;
	float authoredLen = Length(authoredVec);
	float currentLen = Length(currentVec);

	const float EPS = 1e-6f;
	float scale = 1.0f;
	if (authoredLen > EPS) {
		scale = currentLen / authoredLen;
	}

	// コントロールポイントのオフセット（オーサー座標系 → 実座標系にスケール）
	vec2 offset1 = curve.ControlPoint1 - curve.StartPoint; // cp1 relative to start
	vec2 offset2 = curve.ControlPoint2 - curve.EndPoint;   // cp2 relative to end (note: signed from end)

	vec2 p0 = startvalue;
	vec2 p1 = startvalue + offset1 * scale;
	vec2 p2 = endvalue + offset2 * scale;
	vec2 p3 = endvalue;

	return EvalCubicBezier(p0, p1, p2, p3, std::clamp(t, 0.0f, 1.0f));
}