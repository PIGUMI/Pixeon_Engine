#pragma once
#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include "Struct.h"
#include "ImageRender.h"

struct CurveData
{
	DirectX::XMFLOAT2 StartPoint = { 0.0f,0.0f };
	DirectX::XMFLOAT2 ControlPoint1 = { 0.0f,0.0f };
	DirectX::XMFLOAT2 ControlPoint2 = { 1.0f,1.0f };
	DirectX::XMFLOAT2 EndPoint = { 1.0f,1.0f };
};

struct UITransform
{
	DirectX::XMFLOAT2 Position = { 0.0f,0.0f };
	DirectX::XMFLOAT2 Rotation = { 0.0f,0.0f };
	DirectX::XMFLOAT2 Scale = { 1.0f,1.0f };
	DirectX::XMFLOAT2 UVScale = { 1.0f,1.0f };
	DirectX::XMFLOAT2 UVPosition = { 0.0f,0.0f };
	DirectX::XMFLOAT4 Color = { 1.0f,1.0f,1.0f,1.0f };
};

struct EditorFlag
{
	bool bPosition = false;
	bool bRotation = false;
	bool bScale = false;
	bool bUVPosition = false;
	bool bUVScale = false;
	bool bColor = false;
};

struct VertexOffset
{
	float Up = 50.0f;
	float Down = 50.0f;
	float Left = 50.0f;
	float Right = 50.0f;
};

enum ViewMode
{
	UI,
	Billboard,
};

struct KeyFrame
{
	std::string KeyFrameName = "KeyFrame";
	bool Active = false;
	int Layer = 0;
	float StartTime = 0.0f;
	float EndTime = 1.0f;
	CurveData CurveInfo;
	UITransform StartTransform;
	UITransform EndTransform;
	UITransform NowTransform;
	std::string Texture;
	EditorFlag editorFlag;
	VertexOffset vertexOffset;
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

class Animator2D
{
public:
	Animator2D();
	~Animator2D();
	void Update();
	void EditorUpdate();
	void Draw(int Layer = 0);
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

	void SetOwner(AbstractObject* owner);

	void SetEditorMode(bool editor) { bEditorMode_ = editor; }
	void PreviewUpdate();

	void SetLayer(int layer) { layer_ = layer; }

	void Stop();
	void Start();

	Animator2D* Copy();
private:
	void KeyFrameUpdate();

	DirectX::XMFLOAT2 EaseByBezierCurve(const CurveData& curve, const DirectX::XMFLOAT2& startvalue, const DirectX::XMFLOAT2& endvalue, float elapsed, float duration = 1.0f);

public:
	std::string Name_ = "Animator2D";
	bool bLoop_ = false;
	bool bFirst_ = true; 
	bool bEnded_ = false;
	float fStartTime_ = 0.0f;
	float fNowTime_ = 0.0f;
	float fTotalDuration_ = 0.0f;
	std::vector<KeyFrame> KeyFrames_;
	ViewMode viewMode_ = ViewMode::UI;
	ImageRender* PreviewImage = nullptr;
	int DrawCount = 0;
	AbstractObject* owner_ = nullptr;
	bool bEditorMode_ = false;
	int layer_ = 0;
};