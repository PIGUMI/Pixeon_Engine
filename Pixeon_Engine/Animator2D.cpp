#include "Animator2D.h"
#include "SettingManager.h"
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

Animator2D::Animator2D()
	:bLoop_(false),
	bFirst_(true),
	fStartTime_(0.0f),
	fNowTime_(0.0f),
	fTotalDuration_(0.0f)
{
}

Animator2D::~Animator2D()
{
	for (auto obj : KeyFrames_)
	{
		delete obj.Image;
		obj.Image = nullptr;
	}
}

void Animator2D::Update()
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
			fStartTime_ = static_cast<float>(nowSec);
			fNowTime_ = 0.0f;
		}
		else
		{
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
			obj.Image->SetOffset3D(DirectX::XMFLOAT3(pos.x, pos.y, 0.0f));
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
			obj.Image->SetSizeWorld(Scl);
		}

		// UV 位置
		if (obj.StartTransform.UVPostion != obj.EndTransform.UVPostion)
		{
			vec2 uvp = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.UVPostion, obj.EndTransform.UVPostion, elapsed, duration);
			DirectX::XMFLOAT4 uvRect = obj.Image->GetUVRect();
			uvRect.x = uvp.x;
			uvRect.y = uvp.y;
			//obj.Image->SetUVRect(uvRect);
		}

		// UV スケール
		if (obj.StartTransform.UVScale != obj.EndTransform.UVScale)
		{
			vec2 uvs = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.UVScale, obj.EndTransform.UVScale, elapsed, duration);
			DirectX::XMFLOAT4 uvRect = obj.Image->GetUVRect();
			uvRect.z = uvs.x;
			uvRect.w = uvs.y;
			//obj.Image->SetUVRect(uvRect);
		}
	}
}

void Animator2D::EditorUpdate()
{
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
			obj.Image->SetOffset3D(DirectX::XMFLOAT3(pos.x, pos.y, 0.0f));
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
			obj.Image->SetSizeWorld(Scl);
		}

		// UV 位置
		if (obj.StartTransform.UVPostion != obj.EndTransform.UVPostion)
		{
			vec2 uvp = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.UVPostion, obj.EndTransform.UVPostion, elapsed, duration);
			DirectX::XMFLOAT4 uvRect = obj.Image->GetUVRect();
			uvRect.x = uvp.x;
			uvRect.y = uvp.y;
			//obj.Image->SetUVRect(uvRect);
		}

		// UV スケール
		if (obj.StartTransform.UVScale != obj.EndTransform.UVScale)
		{
			vec2 uvs = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.UVScale, obj.EndTransform.UVScale, elapsed, duration);
			DirectX::XMFLOAT4 uvRect = obj.Image->GetUVRect();
			uvRect.z = uvs.x;
			uvRect.w = uvs.y;
			//obj.Image->SetUVRect(uvRect);
		}
	}
}

void Animator2D::Draw()
{
	std::vector<KeyFrame*> ActiveKeyFrames;
	// アクティブなキーフレームを収集
	for (auto obj : KeyFrames_)
	{
		if (obj.Active)
		{
			ActiveKeyFrames.push_back(&obj);
		}
	}
	// レイヤー順にソート
	std::sort(ActiveKeyFrames.begin(), ActiveKeyFrames.end(),
		[](const KeyFrame* a, const KeyFrame* b) {
			return a->Layer < b->Layer;
		});
	// アクティブなキーフレームの ImageRender を描画
	for (auto kf : ActiveKeyFrames)
	{
		if (kf->Image)
		{
			switch (viewMode_)
			{
			case Animator2D::UI:
				kf->Image->SetPlacementMode(ImageRender::PlacementMode::UI);
				break;
			case Animator2D::Billboard:
				kf->Image->SetPlacementMode(ImageRender::PlacementMode::Billboard);
				break;
			}
			kf->Image->Draw();
		}
	}
}

void Animator2D::SaveFile()
{
	nlohmann::json SaveJson;
	SaveJson["ProjectName"] = Name_;
	SaveJson["TotalDuration"] = fTotalDuration_;
	SaveJson["Loop"] = bLoop_;
	// キーフレーム群
	nlohmann::json KeyFramesJson = nlohmann::json::array();
	for (const auto& kf : KeyFrames_)
	{
		nlohmann::json kfJson;
		kfJson["Active"] = kf.Active;
		kfJson["Layer"] = kf.Layer;
		kfJson["StartTime"] = kf.StartTime;
		kfJson["EndTime"] = kf.EndTime;
		// CurveInfo
		kfJson["CurveInfo"]["StartPoint"] = { kf.CurveInfo.StartPoint.x, kf.CurveInfo.StartPoint.y };
		kfJson["CurveInfo"]["ControlPoint1"] = { kf.CurveInfo.ControlPoint1.x, kf.CurveInfo.ControlPoint1.y };
		kfJson["CurveInfo"]["ControlPoint2"] = { kf.CurveInfo.ControlPoint2.x, kf.CurveInfo.ControlPoint2.y };
		kfJson["CurveInfo"]["EndPoint"] = { kf.CurveInfo.EndPoint.x, kf.CurveInfo.EndPoint.y };
		// StartTransform
		kfJson["StartTransform"]["Position"] = { kf.StartTransform.Position.x, kf.StartTransform.Position.y };
		kfJson["StartTransform"]["Rotation"] = { kf.StartTransform.Rotation.x, kf.StartTransform.Rotation.y };
		kfJson["StartTransform"]["Scale"] = { kf.StartTransform.Scale.x, kf.StartTransform.Scale.y };
		kfJson["StartTransform"]["UVScale"] = { kf.StartTransform.UVScale.x, kf.StartTransform.UVScale.y };
		kfJson["StartTransform"]["UVPostion"] = { kf.StartTransform.UVPostion.x, kf.StartTransform.UVPostion.y };
		// EndTransform
		kfJson["EndTransform"]["Position"] = { kf.EndTransform.Position.x, kf.EndTransform.Position.y };
		kfJson["EndTransform"]["Rotation"] = { kf.EndTransform.Rotation.x, kf.EndTransform.Rotation.y };
		kfJson["EndTransform"]["Scale"] = { kf.EndTransform.Scale.x, kf.EndTransform.Scale.y };
		kfJson["EndTransform"]["UVScale"] = { kf.EndTransform.UVScale.x, kf.EndTransform.UVScale.y };
		kfJson["EndTransform"]["UVPostion"] = { kf.EndTransform.UVPostion.x, kf.EndTransform.UVPostion.y };

		// ImageRender の保存
		std::ostringstream oss;
		kf.Image->SaveToFile(oss);
		kfJson["Image"] = oss.str();
		KeyFramesJson.push_back(kfJson);
	}
	SaveJson["KeyFrames"] = KeyFramesJson;
	// ファイルに保存
	std::string savePath = SettingManager::GetInstance()->GetAnimator2DProjectFilePath() + "/" + Name_ + ".anim2d";
	std::ofstream outFile(savePath);
	if (outFile.is_open()) {
		outFile << SaveJson.dump(4);
		outFile.close();
	}
}

void Animator2D::LoadFile(std::string FilePath)
{
	std::ifstream inFile(FilePath);
	if (!inFile.is_open()) {
		return;
	}
	nlohmann::json LoadJson;
	inFile >> LoadJson;
	inFile.close();
	// プロジェクト名
	if (LoadJson.contains("ProjectName")) {
		Name_ = LoadJson["ProjectName"].get<std::string>();
	}
	// 総再生時間
	if (LoadJson.contains("TotalDuration")) {
		fTotalDuration_ = LoadJson["TotalDuration"].get<float>();
	}
	// ループ設定
	if (LoadJson.contains("Loop")) {
		bLoop_ = LoadJson["Loop"].get<bool>();
	}
	// キーフレーム群
	if (LoadJson.contains("KeyFrames")) {
		KeyFrames_.clear();
		for (const auto& kfJson : LoadJson["KeyFrames"]) {
			KeyFrame kf;
			kf.Active = kfJson["Active"].get<bool>();
			kf.Layer = kfJson["Layer"].get<int>();
			kf.StartTime = kfJson["StartTime"].get<float>();
			kf.EndTime = kfJson["EndTime"].get<float>();
			// CurveInfo
			auto curveJson = kfJson["CurveInfo"];
			kf.CurveInfo.StartPoint = { curveJson["StartPoint"][0].get<float>(), curveJson["StartPoint"][1].get<float>() };
			kf.CurveInfo.ControlPoint1 = { curveJson["ControlPoint1"][0].get<float>(), curveJson["ControlPoint1"][1].get<float>() };
			kf.CurveInfo.ControlPoint2 = { curveJson["ControlPoint2"][0].get<float>(), curveJson["ControlPoint2"][1].get<float>() };
			kf.CurveInfo.EndPoint = { curveJson["EndPoint"][0].get<float>(), curveJson["EndPoint"][1].get<float>() };
			// StartTransform
			auto startTransJson = kfJson["StartTransform"];
			kf.StartTransform.Position = { startTransJson["Position"][0].get<float>(), startTransJson["Position"][1].get<float>() };
			kf.StartTransform.Rotation = { startTransJson["Rotation"][0].get<float>(), startTransJson["Rotation"][1].get<float>() };
			kf.StartTransform.Scale = { startTransJson["Scale"][0].get<float>(), startTransJson["Scale"][1].get<float>() };
			kf.StartTransform.UVScale = { startTransJson["UVScale"][0].get<float>(), startTransJson["UVScale"][1].get<float>() };
			kf.StartTransform.UVPostion = { startTransJson["UVPostion"][0].get<float>(), startTransJson["UVPostion"][1].get<float>() };
			// EndTransform
			auto endTransJson = kfJson["EndTransform"];
			kf.EndTransform.Position = { endTransJson["Position"][0].get<float>(), endTransJson["Position"][1].get<float>() };
			kf.EndTransform.Rotation = { startTransJson["Rotation"][0].get<float>(), startTransJson["Rotation"][1].get<float>() };
			kf.EndTransform.Scale = { startTransJson["Scale"][0].get<float>(), startTransJson["Scale"][1].get<float>() };
			kf.EndTransform.UVScale = { startTransJson["UVScale"][0].get<float>(), startTransJson["UVScale"][1].get<float>() };
			kf.EndTransform.UVPostion = { startTransJson["UVPostion"][0].get<float>(), startTransJson["UVPostion"][1].get<float>() };
			// ImageRender の読み込み
			if (kfJson.contains("Image")) {
				std::istringstream iss(kfJson["Image"].get<std::string>());
				kf.Image = new ImageRender();
				kf.Image->LoadFromFile(iss);
			}
			KeyFrames_.push_back(kf);
		}
	}
}

void Animator2D::LoadCache(std::ifstream& in)
{
	// プロジェクト名
	std::getline(in, Name_);
	// 総再生時間
	in.read(reinterpret_cast<char*>(&fTotalDuration_), sizeof(float));
	// ループ設定
	in.read(reinterpret_cast<char*>(&bLoop_), sizeof(bool));
	// キーフレーム数
	size_t keyframeCount = 0;
	in.read(reinterpret_cast<char*>(&keyframeCount), sizeof(size_t));
	KeyFrames_.clear();
	for (size_t i = 0; i < keyframeCount; ++i)
	{
		KeyFrame kf;
		// Active
		in.read(reinterpret_cast<char*>(&kf.Active), sizeof(bool));
		// Layer
		in.read(reinterpret_cast<char*>(&kf.Layer), sizeof(int));
		// StartTime
		in.read(reinterpret_cast<char*>(&kf.StartTime), sizeof(float));
		// EndTime
		in.read(reinterpret_cast<char*>(&kf.EndTime), sizeof(float));
		// CurveInfo
		in.read(reinterpret_cast<char*>(&kf.CurveInfo), sizeof(CurveData));
		// StartTransform
		in.read(reinterpret_cast<char*>(&kf.StartTransform), sizeof(UITransform));
		// EndTransform
		in.read(reinterpret_cast<char*>(&kf.EndTransform), sizeof(UITransform));
		// ImageRender の読み込み
		kf.Image = new ImageRender();
		kf.Image->LoadFromFile(in);
		KeyFrames_.push_back(kf);
	}
}

void Animator2D::AddKeyFrame(const KeyFrame& keyframe)
{
	KeyFrames_.push_back(keyframe);
}

void Animator2D::RemoveKeyFrame(KeyFrame* ptr)
{
	delete ptr->Image;
	ptr->Image = nullptr;
	KeyFrames_.erase(std::remove_if(KeyFrames_.begin(), KeyFrames_.end(),
		[ptr](const KeyFrame& kf) { return &kf == ptr; }), KeyFrames_.end());
}

vec2 Animator2D::EaseByBezierCurve(const CurveData& curve, const vec2& startvalue, const vec2& endvalue, float elapsed, float duration)
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