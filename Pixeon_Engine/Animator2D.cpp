#include "Animator2D.h"
#include "SettingManager.h"
#include "GUI.h"
#include "Math.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <algorithm>

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
	PreviewImage = new ImageRender();
	PreviewImage->Init(owner_);
}

Animator2D::~Animator2D()
{
	if (PreviewImage)
	{
		delete PreviewImage;
		PreviewImage = nullptr;
	}
}

void Animator2D::Update()
{
	double nowSec = GetTimeSeconds();

	if (bFirst_)
	{
		bFirst_ = false;
		fStartTime_ = static_cast<float>(nowSec);
	}

	fNowTime_ = static_cast<float>(nowSec - static_cast<double>(fStartTime_));
	bEnded_ = false;

	if (fTotalDuration_ > 0.0f && fNowTime_ >= fTotalDuration_)
	{
		if (bLoop_)
		{
			fStartTime_ = static_cast<float>(nowSec);
			fNowTime_ = 0.0f;
		}
		else
		{
			bEnded_ = true;
			fNowTime_ = fTotalDuration_;
		}
	}
	KeyFrameUpdate();
}

void Animator2D::EditorUpdate()
{
	KeyFrameUpdate();
}

void Animator2D::Draw(int Layer)
{
	std::vector<KeyFrame> SortedKeyFrames = KeyFrames_;

	std::sort(SortedKeyFrames.begin(), SortedKeyFrames.end(),
		[](const KeyFrame& a, const KeyFrame& b)
		{
			return a.Layer < b.Layer;
		});

	DrawCount = 0;
	for (auto& kf : SortedKeyFrames)
	{
		if (!kf.Active) continue;
		DrawCount++;
		if (PreviewImage)
		{
			switch (viewMode_)
			{
			case ViewMode::UI:
				PreviewImage->SetPlacementMode(ImageRender::PlacementMode::UI);
				break;
			case ViewMode::Billboard:
				PreviewImage->SetPlacementMode(ImageRender::PlacementMode::Billboard);
				break;
			}

			if (bEditorMode_)// 変換情報設定
			{
				float Cor = 100.0f;// 補正値
				PreviewImage->SetOffset2D({ kf.NowTransform.Position.x * Cor , kf.NowTransform.Position.y * Cor });
				PreviewImage->SetOffset3D(DirectX::XMFLOAT3(kf.NowTransform.Position.x * Cor, kf.NowTransform.Position.y * Cor, 0.0f));
				PreviewImage->SetSize2D({ kf.NowTransform.Scale.x * Cor , kf.NowTransform.Scale.y * Cor });
				PreviewImage->SetSizeWorld({ kf.NowTransform.Scale.x * Cor , kf.NowTransform.Scale.y * Cor });
				PreviewImage->SetUVRect(DirectX::XMFLOAT4(
					kf.NowTransform.UVPosition.x,
					kf.NowTransform.UVPosition.y,
					kf.NowTransform.UVScale.x,
					kf.NowTransform.UVScale.y));
			}
			else
			{
				float Cor = 0.4f;// 補正値
				PreviewImage->SetOffset2D({ kf.NowTransform.Position.x * Cor , kf.NowTransform.Position.y * Cor });
				PreviewImage->SetOffset3D(DirectX::XMFLOAT3(kf.NowTransform.Position.x * Cor, kf.NowTransform.Position.y * Cor, 0.0f));
				PreviewImage->SetSize2D({ kf.NowTransform.Scale.x * Cor , kf.NowTransform.Scale.y * Cor });
				PreviewImage->SetSizeWorld({ kf.NowTransform.Scale.x * Cor , kf.NowTransform.Scale.y * Cor });
				PreviewImage->SetUVRect(DirectX::XMFLOAT4(
					kf.NowTransform.UVPosition.x,
					kf.NowTransform.UVPosition.y,
					kf.NowTransform.UVScale.x,
					kf.NowTransform.UVScale.y));
			}
			PreviewImage->SetLayerNumber(layer_);
			PreviewImage->SetColor(kf.NowTransform.Color);
			PreviewImage->SetTextureName(kf.Texture);
			PreviewImage->Draw(Layer);
		}
		else
		{
			MessageBox(nullptr, "Animator2D Draw Error: PreviewImage is nullptr", "Error", MB_OK | MB_ICONERROR);
		}
		DrawCount++;
	}
}

void Animator2D::Debug()
{
	ImGui::Begin("Animator2D Debug");

	ImGui::Text("Project Name: %s", Name_.c_str());
	std::string msg;
	msg = "KeyFrames Count: " + std::to_string(KeyFrames_.size());
	ImGui::Text("%s", msg.c_str());

	int Count = 0;
	for (auto kf : KeyFrames_)
	{
		msg = "KeyFrame " + std::to_string(Count) + ": ";
		ImGui::Text("%s", msg.c_str());
		msg = "Pos" + std::to_string(Count) + ": (" + std::to_string(kf.StartTransform.Position.x) + ", " + std::to_string(kf.StartTransform.Position.y) + ") -> ("
			+ std::to_string(kf.EndTransform.Position.x) + ", " + std::to_string(kf.EndTransform.Position.y) + ")";
		ImGui::Text("%s", msg.c_str());
		msg = "NowPos" + std::to_string(Count) + ": (" + std::to_string(kf.NowTransform.Position.x) + ", " + std::to_string(kf.NowTransform.Position.y) + ")";
		ImGui::Text("%s", msg.c_str());
		if (kf.Active)
		{
			ImGui::Text("Status: Active");
		}
		else
		{
			ImGui::Text("Status: Inactive");
		}
		Count++;
		ImGui::Separator();
	}
	msg = "Draw Count: " + std::to_string(DrawCount);
	ImGui::Text("%s", msg.c_str());

	ImGui::End();
}

void Animator2D::SaveFile()
{
	nlohmann::json SaveJson;
	SaveJson["ProjectName"] = Name_;
	SaveJson["TotalDuration"] = fTotalDuration_;
	SaveJson["Loop"] = bLoop_;
	SaveJson["ViewMode"] = static_cast<int>(viewMode_);
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
		kfJson["StartTransform"]["UVPosition"] = { kf.StartTransform.UVPosition.x, kf.StartTransform.UVPosition.y };
		kfJson["StartTransform"]["Color"] = { kf.StartTransform.Color.x, kf.StartTransform.Color.y, kf.StartTransform.Color.z, kf.StartTransform.Color.w };
		// EndTransform
		kfJson["EndTransform"]["Position"] = { kf.EndTransform.Position.x, kf.EndTransform.Position.y };
		kfJson["EndTransform"]["Rotation"] = { kf.EndTransform.Rotation.x, kf.EndTransform.Rotation.y };
		kfJson["EndTransform"]["Scale"] = { kf.EndTransform.Scale.x, kf.EndTransform.Scale.y };
		kfJson["EndTransform"]["UVScale"] = { kf.EndTransform.UVScale.x, kf.EndTransform.UVScale.y };
		kfJson["EndTransform"]["UVPosition"] = { kf.EndTransform.UVPosition.x, kf.EndTransform.UVPosition.y };
		kfJson["EndTransform"]["Color"] = { kf.EndTransform.Color.x, kf.EndTransform.Color.y, kf.EndTransform.Color.z, kf.EndTransform.Color.w };
		kfJson["EditorFlag"]["bPosition"] = kf.editorFlag.bPosition;
		kfJson["EditorFlag"]["bRotation"] = kf.editorFlag.bRotation;
		kfJson["EditorFlag"]["bScale"] = kf.editorFlag.bScale;
		kfJson["EditorFlag"]["bUVPosition"] = kf.editorFlag.bUVPosition;
		kfJson["EditorFlag"]["bUVScale"] = kf.editorFlag.bUVScale;
		kfJson["Texture"] = kf.Texture;
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
	// 描画モード
	if (LoadJson.contains("ViewMode")) {
		viewMode_ = static_cast<ViewMode>(LoadJson["ViewMode"].get<int>());
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
			kf.StartTransform.UVPosition = { startTransJson["UVPosition"][0].get<float>(), startTransJson["UVPosition"][1].get<float>() };
			kf.StartTransform.Color = {
				startTransJson["Color"][0].get<float>(),
				startTransJson["Color"][1].get<float>(),
				startTransJson["Color"][2].get<float>(),
				startTransJson["Color"][3].get<float>() };
			// EndTransform
			auto endTransJson = kfJson["EndTransform"];
			kf.EndTransform.Position = { endTransJson["Position"][0].get<float>(), endTransJson["Position"][1].get<float>() };
			kf.EndTransform.Rotation = { startTransJson["Rotation"][0].get<float>(), startTransJson["Rotation"][1].get<float>() };
			kf.EndTransform.Scale = { startTransJson["Scale"][0].get<float>(), startTransJson["Scale"][1].get<float>() };
			kf.EndTransform.UVScale = { startTransJson["UVScale"][0].get<float>(), startTransJson["UVScale"][1].get<float>() };
			kf.EndTransform.UVPosition = { startTransJson["UVPosition"][0].get<float>(), startTransJson["UVPosition"][1].get<float>() };
			kf.EndTransform.Color = {
				endTransJson["Color"][0].get<float>(),
				endTransJson["Color"][1].get<float>(),
				endTransJson["Color"][2].get<float>(),
				endTransJson["Color"][3].get<float>() };
			// EditorFlag
			auto editorFlagJson = kfJson["EditorFlag"];
			kf.editorFlag.bPosition = editorFlagJson["bPosition"].get<bool>();
			kf.editorFlag.bRotation = editorFlagJson["bRotation"].get<bool>();
			kf.editorFlag.bScale = editorFlagJson["bScale"].get<bool>();
			kf.editorFlag.bUVPosition = editorFlagJson["bUVPosition"].get<bool>();
			kf.editorFlag.bUVScale = editorFlagJson["bUVScale"].get<bool>();
			// Texture
			kf.Texture = kfJson["Texture"].get<std::string>();
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
		KeyFrames_.push_back(kf);
	}
}

void Animator2D::AddKeyFrame(const KeyFrame& keyframe)
{
	KeyFrames_.push_back(keyframe);
}

void Animator2D::RemoveKeyFrame(KeyFrame* ptr)
{
	KeyFrames_.erase(std::remove_if(KeyFrames_.begin(), KeyFrames_.end(),
		[ptr](const KeyFrame& kf) { return &kf == ptr; }), KeyFrames_.end());
}

void Animator2D::SetOwner(AbstractObject* owner)
{
	owner_ = owner;
	PreviewImage->SetParent(owner);
}

void Animator2D::PreviewUpdate()
{
	PreviewImage->Update();
}

Animator2D* Animator2D::Copy()
{
	Animator2D* newAnimator = new Animator2D();
	newAnimator->Name_ = this->Name_;
	newAnimator->bLoop_ = this->bLoop_;
	newAnimator->fTotalDuration_ = this->fTotalDuration_;
	for (const auto& kf : this->KeyFrames_)
	{
		KeyFrame newKf = kf;
		newAnimator->KeyFrames_.push_back(newKf);
	}
	return newAnimator;
}

void Animator2D::KeyFrameUpdate()
{
	for (auto& obj : KeyFrames_)
	{
		// アクティブ確認
		obj.Active = (fNowTime_ >= obj.StartTime && fNowTime_ <= obj.EndTime);

		if (!obj.Active) continue;

		float elapsed = fNowTime_ - obj.StartTime;
		float duration = obj.EndTime - obj.StartTime;

		float tNormalized = 0.0f;
		if (duration <= 0.0f) tNormalized = 1.0f;
		else tNormalized = std::clamp(elapsed / duration, 0.0f, 1.0f);

		// 位置
		DirectX::XMFLOAT2 Pos = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.Position, obj.EndTransform.Position, elapsed, duration);
		obj.NowTransform.Position = Pos;
		// 回転
		DirectX::XMFLOAT2 Rot = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.Rotation, obj.EndTransform.Rotation, elapsed, duration);
		obj.NowTransform.Rotation = Rot;
		// スケール
		DirectX::XMFLOAT2 Scl = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.Scale, obj.EndTransform.Scale, elapsed, duration);
		obj.NowTransform.Scale = Scl;
		// UV 位置
		DirectX::XMFLOAT2 uvp = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.UVPosition, obj.EndTransform.UVPosition, elapsed, duration);
		obj.NowTransform.UVPosition = uvp;
		// UV スケール
		DirectX::XMFLOAT2 uvs = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.UVScale, obj.EndTransform.UVScale, elapsed, duration);
		obj.NowTransform.UVScale = uvs;
		// 色
		//情報を分割
		DirectX::XMFLOAT2 ColorRB = { obj.StartTransform.Color.x, obj.StartTransform.Color.z };
		DirectX::XMFLOAT2 ColorGA = { obj.StartTransform.Color.y, obj.StartTransform.Color.w };
		DirectX::XMFLOAT2 EndColorRB = { obj.EndTransform.Color.x, obj.EndTransform.Color.z };
		DirectX::XMFLOAT2 EndColorGA = { obj.EndTransform.Color.y, obj.EndTransform.Color.w };
		DirectX::XMFLOAT2 NewColorRB = EaseByBezierCurve(obj.CurveInfo, ColorRB, EndColorRB, elapsed, duration);
		DirectX::XMFLOAT2 NewColorGA = EaseByBezierCurve(obj.CurveInfo, ColorGA, EndColorGA, elapsed, duration);
		obj.NowTransform.Color = DirectX::XMFLOAT4(NewColorRB.x, NewColorGA.x, NewColorRB.y, NewColorGA.y);
	}
}

DirectX::XMFLOAT2 Animator2D::EaseByBezierCurve(const CurveData& curve, const DirectX::XMFLOAT2& startvalue, const DirectX::XMFLOAT2& endvalue, float elapsed, float duration)
{
	float t;
	if (duration <= 0.0f) {
		t = 1.0f;
	}
	else {
		t = elapsed / duration;
	}

	if (t <= 0.0f) return startvalue;
	if (t >= 1.0f) return endvalue;

	DirectX::XMFLOAT2 authoredVec = XMFLOAT2Subtract(curve.EndPoint, curve.StartPoint);
	DirectX::XMFLOAT2 currentVec = XMFLOAT2Subtract(endvalue, startvalue);

	const float EPS = 1e-6f;

	// 軸ごとのスケールを計算（authored が 0 の場合はその軸は 0 にする）
	float scaleX = 1.0f, scaleY = 1.0f;
	if (std::fabs(authoredVec.x) > EPS) scaleX = currentVec.x / authoredVec.x;
	else scaleX = 0.0f;
	if (std::fabs(authoredVec.y) > EPS) scaleY = currentVec.y / authoredVec.y;
	else scaleY = 0.0f;

	// コントロール点のオフセット（authored 空間 -> current 空間 に適用）
	DirectX::XMFLOAT2 offset1 = XMFLOAT2Subtract(curve.ControlPoint1, curve.StartPoint);
	DirectX::XMFLOAT2 offset2 = XMFLOAT2Subtract(curve.ControlPoint2, curve.EndPoint);

	DirectX::XMFLOAT2 p0 = startvalue;
	DirectX::XMFLOAT2 p1 = XMFLOAT2Add(startvalue, DirectX::XMFLOAT2{ offset1.x * scaleX, offset1.y * scaleY });
	DirectX::XMFLOAT2 p2 = XMFLOAT2Add(endvalue, DirectX::XMFLOAT2{ offset2.x * scaleX, offset2.y * scaleY });
	DirectX::XMFLOAT2 p3 = endvalue;

	// start と end の差がほぼ 0 の軸は、その軸方向のコントロール点を強制固定して
	// 不要な軸移動を防ぐ（例: X を固定したいなら p1.x/p2.x を開始 x に固定）
	if (std::fabs(currentVec.x) < EPS) {
		p1.x = p0.x;
		p2.x = p3.x;
	}
	if (std::fabs(currentVec.y) < EPS) {
		p1.y = p0.y;
		p2.y = p3.y;
	}

	return EvalCubicBezier(p0, p1, p2, p3, std::clamp(t, 0.0f, 1.0f));
}