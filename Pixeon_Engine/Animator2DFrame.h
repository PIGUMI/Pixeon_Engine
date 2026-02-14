#ifndef ANIMATOR2DFRAME_H
#define ANIMATOR2DFRAME_H

#include "CameraComponent.h"

class Animator2D;
struct KeyFrame;
class TimelineEditor;

class Animator2DFrame
{
public:
	static Animator2DFrame* GetInstance();
	static void DestroyInstance();
public:
	void Init();
	void Update();
	void Draw();
	void UnInit();
	void DrawGUI();
public:
	Animator2D* GetAnimator() const { return animator_; }
	void SetAnimator(Animator2D* animator) { animator_ = animator; }
	KeyFrame* GetSelectedKeyFrame() const { return selectedKeyFrame_; }
	void SetSelectedKeyFrame(KeyFrame* keyframe) { selectedKeyFrame_ = keyframe; }
	CameraComponent* GetEditorCamera() const { return editorCamera_; }
private:
	void DrawTimeline();
	void DrawView();
	void DrawKeyFrameEditor();
	void DrawAnimatorControl();
	void DrawTextureLoadPopup();
private:
	Animator2D* animator_ = nullptr;
	KeyFrame* selectedKeyFrame_ = nullptr;
	TimelineEditor* timelineEditor_ = nullptr;
	CameraComponent* editorCamera_ = nullptr;  // ’Ç‰Á
	bool isPlaying_ = false;
	bool wantOpenTexturePopup_ = false;
private:
	static Animator2DFrame* instance;
};

#endif // ANIMATOR2DFRAME_H