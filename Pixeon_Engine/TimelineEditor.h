#pragma once
#include "Animator2D.h"
#include "IMGUI/imgui.h"
#include <vector>
#include <string>

class TimelineEditor
{
public:
	TimelineEditor();
	~TimelineEditor();

	// メインの描画関数
	void DrawTimeline(Animator2D* animator);

	// 設定
	void SetZoom(float zoom) { timeScale_ = zoom; }
	float GetZoom() const { return timeScale_; }
	void DrawProjectLoadPopup();
	void RequestOpenProjectPopup() { wantOpenProjectPopup_ = true; }
	// 要求を取得してクリア（呼んだ側が OpenPopup を実行する）
	bool ConsumeOpenProjectPopupRequest() { return wantOpenProjectPopup_; }
	void ClearOpenProjectPopupRequest() { wantOpenProjectPopup_ = false; }
private:
	// タイムライン要素の描画
	void DrawTimeRuler(float totalDuration);
	void DrawPlayhead(float currentTime);
	void DrawLayers(Animator2D* animator);
	void DrawKeyFrame(KeyFrame* keyframe, int layerIndex);

	// 入力処理
	void HandleInput(Animator2D* animator);
	void HandleKeyFrameDrag(KeyFrame* keyframe);
	void HandleKeyFrameResize(KeyFrame* keyframe);
	void HandleContextMenu(Animator2D* animator, KeyFrame* keyframe);

	// ユーティリティ
	float TimeToPixel(float time) const;
	float PixelToTime(float pixel) const;
	float SnapToGrid(float time) const;
	ImVec2 GetTimelineOrigin() const;

private:
	std::vector<std::string> projectFiles_;
	int selectedProjectIndex_ = -1;
	// タイムライン設定
	float timeScale_ = 100.0f;          // ピクセル/秒
	float scrollX_ = 0.0f;              // 横スクロール量
	float layerHeight_ = 40.0f;         // レイヤーの高さ
	float rulerHeight_ = 30.0f;         // ルーラーの高さ
	float leftPanelWidth_ = 100.0f;     // 左パネル幅
	bool snapToGrid_ = true;            // グリッドスナップ
	float snapInterval_ = 0.1f;         // スナップ間隔（秒）

	// 操作状態
	KeyFrame* selectedKeyFrame_ = nullptr;
	KeyFrame* draggedKeyFrame_ = nullptr;
	KeyFrame* resizingKeyFrame_ = nullptr;
	bool isDragging_ = false;
	bool isResizingLeft_ = false;
	bool isResizingRight_ = false;
	bool wantOpenProjectPopup_ = false;
	ImVec2 dragStartPos_;
	float dragStartTime_ = 0.0f;

	// 色設定
	ImU32 colorBackground_ = IM_COL32(45, 45, 48, 255);
	ImU32 colorGrid_ = IM_COL32(60, 60, 63, 255);
	ImU32 colorRuler_ = IM_COL32(30, 30, 32, 255);
	ImU32 colorPlayhead_ = IM_COL32(255, 100, 100, 255);
	ImU32 colorKeyFrame_ = IM_COL32(100, 150, 255, 255);
	ImU32 colorKeyFrameSelected_ = IM_COL32(255, 200, 100, 255);
	ImU32 colorKeyFrameBorder_ = IM_COL32(200, 200, 200, 255);
};
