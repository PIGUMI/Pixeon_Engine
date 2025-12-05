#define NOMINMAX
#include "TimelineEditor.h"
#include "Input.h"
#include <algorithm>
#include <cmath>
#include <iostream>

TimelineEditor::TimelineEditor()
{
}

TimelineEditor::~TimelineEditor()
{
}

void TimelineEditor::DrawTimeline(Animator2D* animator)
{
    ImGui::BeginChild("Timeline", ImVec2(0, 0), true);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    // 背景描画
    drawList->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        colorBackground_);

    // タイムルーラー描画
    if (animator)
    {
        DrawTimeRuler(animator->GetTotalTime());

        // レイヤー描画
        DrawLayers(animator);

        // 再生ヘッド描画（最前面）
        DrawPlayhead(animator->fNowTime_);

        // 入力処理
        HandleInput(animator);

		// 総時間更新
		std::vector<KeyFrame>*keyframes = animator->GetKeyFramePtr();
		float maxTime = 0.0f;
        for(auto & kf : *keyframes)
        {
            if(kf.EndTime > maxTime)
            {
                maxTime = kf.EndTime;
			}
		}
		animator->SetTotalTime(maxTime);
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;

    // 右クリックでコンテキストメニューオープン
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup("TimelineContextMenu");
    }

    // コンテキストメニュー
    if (ImGui::BeginPopup("TimelineContextMenu"))
    {
        if (!animator)
        {
            if (ImGui::MenuItem(EditrGUI::GetInstance()->ShiftJISToUTF8("新規プロジェクトの作成").c_str()))
            {
                Animator2D* newAnimator = new Animator2D();
                EditrGUI::GetInstance()->SetAnimator2D(newAnimator);
            }
        }
        else
        {
            if (ImGui::MenuItem(EditrGUI::GetInstance()->ShiftJISToUTF8("NewKeyFrame").c_str()))
            {
                KeyFrame newKeyFrame;
                newKeyFrame.CurveInfo.StartPoint = { 0.0f, 0.0f };
                newKeyFrame.CurveInfo.ControlPoint1 = { 0.0f, 0.0f };
                newKeyFrame.CurveInfo.ControlPoint2 = { 1.0f, 1.0f };
                newKeyFrame.CurveInfo.EndPoint = { 1.0f, 1.0f };
                newKeyFrame.EndTime = 1.0f;
                newKeyFrame.StartTime = 0.0f;
                newKeyFrame.EndTransform.Position = { 0.0f, 0.0f };
                newKeyFrame.EndTransform.Rotation = { 0.0f, 0.0f };
                newKeyFrame.EndTransform.Scale = { 1.0f, 1.0f };
                newKeyFrame.StartTransform = newKeyFrame.EndTransform;
                newKeyFrame.Layer = 0;
                newKeyFrame.Image = new ImageRender();
                animator->AddKeyFrame(newKeyFrame);
            }
        }
        ImGui::EndPopup();
    }

    static int FreezeTime = 0;

    if (animator)
    {
        if (IsKeyPress(VK_LEFT) && FreezeTime <= 0)
        {
            float MoveTime = 0.0f;
            MoveTime = animator->fNowTime_ - 0.1f;
            animator->fNowTime_ = std::clamp(MoveTime, 0.0f, animator->GetTotalTime());
            FreezeTime = 5;
        }
        if (IsKeyPress(VK_RIGHT) && FreezeTime <= 0)
        {
            float MoveTime = 0.0f;
            MoveTime = animator->fNowTime_ + 0.1f;
            animator->fNowTime_ = std::clamp(MoveTime, 0.0f, animator->GetTotalTime());
            FreezeTime = 5;
        }
    }
    if (FreezeTime > 0)
    {
        FreezeTime--;
	}

    ImGui::EndChild();

}

void TimelineEditor::DrawTimeRuler(float totalDuration)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 origin = GetTimelineOrigin();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    // ルーラー背景
    drawList->AddRectFilled(
        ImVec2(origin.x, origin.y),
        ImVec2(origin.x + canvasSize.x, origin.y + rulerHeight_),
        colorRuler_);

    // 時間メモリ描画
    float pixelWidth = totalDuration * timeScale_;
    float step = 1.0f; // 1秒刻み

    // ズームに応じてステップを調整
    if (timeScale_ < 50.0f) step = 5.0f;
    else if (timeScale_ > 200.0f) step = 0.5f;

    for (float t = 0.0f; t <= totalDuration; t += step)
    {
        float x = origin.x + leftPanelWidth_ + TimeToPixel(t);

        // 縦線
        drawList->AddLine(
            ImVec2(x, origin.y + rulerHeight_ - 10),
            ImVec2(x, origin.y + rulerHeight_),
            IM_COL32(150, 150, 150, 255));

        // 時間テキスト
        char timeText[32];
        snprintf(timeText, sizeof(timeText), "%.1fs", t);
        drawList->AddText(
            ImVec2(x + 2, origin.y + 5),
            IM_COL32(200, 200, 200, 255),
            timeText);
    }

    // グリッド線（垂直）
    for (float t = 0.0f; t <= totalDuration; t += snapInterval_)
    {
        float x = origin.x + leftPanelWidth_ + TimeToPixel(t);
        drawList->AddLine(
            ImVec2(x, origin.y + rulerHeight_),
            ImVec2(x, origin.y + canvasSize.y),
            colorGrid_);
    }
}

void TimelineEditor::DrawPlayhead(float currentTime)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 origin = GetTimelineOrigin();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    float x = origin.x + leftPanelWidth_ + TimeToPixel(currentTime);

    // 再生ヘッドライン
    drawList->AddLine(
        ImVec2(x, origin.y),
        ImVec2(x, origin.y + canvasSize.y),
        colorPlayhead_, 2.0f);

    // 再生ヘッド三角形
    ImVec2 triangle[3] = {
        ImVec2(x, origin.y + rulerHeight_),
        ImVec2(x - 6, origin.y + rulerHeight_ - 10),
        ImVec2(x + 6, origin.y + rulerHeight_ - 10)
    };
    drawList->AddTriangleFilled(triangle[0], triangle[1], triangle[2], colorPlayhead_);
}

void TimelineEditor::DrawLayers(Animator2D* animator)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 origin = GetTimelineOrigin();

    // レイヤー数を取得（最大レイヤー番号+1）
    int maxLayer = 0;
    for (auto& kf : animator->GetKeyFrames())
    {
        maxLayer = std::max(maxLayer, kf.Layer);
    }
    int layerCount = maxLayer + 1;

    // レイヤーごとに描画
    for (int i = 0; i < layerCount; ++i)
    {
        float y = origin.y + rulerHeight_ + i * layerHeight_;

        // レイヤー背景（左パネル）
        drawList->AddRectFilled(
            ImVec2(origin.x, y),
            ImVec2(origin.x + leftPanelWidth_, y + layerHeight_),
            IM_COL32(50, 50, 52, 255));

        // レイヤー境界線
        drawList->AddLine(
            ImVec2(origin.x, y + layerHeight_),
            ImVec2(origin.x + 2000, y + layerHeight_),
            colorGrid_);

        // レイヤー名
        char layerName[32];
        snprintf(layerName, sizeof(layerName), "Layer %d", i);
        drawList->AddText(
            ImVec2(origin.x + 5, y + layerHeight_ / 2 - 7),
            IM_COL32(200, 200, 200, 255),
            layerName);
    }

    // KeyFrame描画
    for (auto& kf : *animator->GetKeyFramePtr())
    {
        DrawKeyFrame(&kf, kf.Layer);
    }
}

void TimelineEditor::DrawKeyFrame(KeyFrame* keyframe, int layerIndex)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 origin = GetTimelineOrigin();

    float startX = origin.x + leftPanelWidth_ + TimeToPixel(keyframe->StartTime);
    float endX = origin.x + leftPanelWidth_ + TimeToPixel(keyframe->EndTime);
    float y = origin.y + rulerHeight_ + layerIndex * layerHeight_;

    // KeyFrame矩形
    ImVec2 rectMin(startX, y + 5);
    ImVec2 rectMax(endX, y + layerHeight_ - 5);

    ImU32 fillColor = (selectedKeyFrame_ == keyframe) ? colorKeyFrameSelected_ : colorKeyFrame_;

    drawList->AddRectFilled(rectMin, rectMax, fillColor, 4.0f);
    drawList->AddRect(rectMin, rectMax, colorKeyFrameBorder_, 4.0f, 0, 2.0f);

    // 時間表示
    char timeText[64];
    snprintf(timeText, sizeof(timeText), "%. 2f - %.2f", keyframe->StartTime, keyframe->EndTime);
    drawList->AddText(
        ImVec2(startX + 5, y + layerHeight_ / 2 - 7),
        IM_COL32(255, 255, 255, 255),
        timeText);

    // リサイズハンドル
    drawList->AddRectFilled(
        ImVec2(startX, y + 5),
        ImVec2(startX + 5, y + layerHeight_ - 5),
        IM_COL32(255, 255, 255, 200));

    drawList->AddRectFilled(
        ImVec2(endX - 5, y + 5),
        ImVec2(endX, y + layerHeight_ - 5),
        IM_COL32(255, 255, 255, 200));
}

void TimelineEditor::HandleInput(Animator2D* animator)
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;
    ImVec2 origin = GetTimelineOrigin();

    // マウスがタイムライン上にあるか
    if (!ImGui::IsWindowHovered()) return;

    // 左クリック
    if (ImGui::IsMouseClicked(0))
    {
        // KeyFrameの選択とドラッグ開始
        selectedKeyFrame_ = nullptr;

        for (auto& kf : *animator->GetKeyFramePtr())
        {
            float startX = origin.x + leftPanelWidth_ + TimeToPixel(kf.StartTime);
            float endX = origin.x + leftPanelWidth_ + TimeToPixel(kf.EndTime);
            float y = origin.y + rulerHeight_ + kf.Layer * layerHeight_;

            // マウスがKeyFrame上にあるか
            if (mousePos.x >= startX && mousePos.x <= endX &&
                mousePos.y >= y + 5 && mousePos.y <= y + layerHeight_ - 5)
            {
                selectedKeyFrame_ = &kf;

                // リサイズハンドルチェック
                if (mousePos.x <= startX + 5)
                {
                    isResizingLeft_ = true;
                    resizingKeyFrame_ = &kf;
                }
                else if (mousePos.x >= endX - 5)
                {
                    isResizingRight_ = true;
                    resizingKeyFrame_ = &kf;
                }
                else
                {
                    isDragging_ = true;
                    draggedKeyFrame_ = &kf;
                    dragStartPos_ = mousePos;
                    dragStartTime_ = kf.StartTime;
                }
                break;
            }
        }

        // 再生ヘッドのクリック
        if (mousePos.y <= origin.y + rulerHeight_)
        {
            float clickTime = PixelToTime(mousePos.x - origin.x - leftPanelWidth_);
            animator->fNowTime_ = std::clamp(clickTime, 0.0f, animator->GetTotalTime());
        }
    }

    // ドラッグ中
    if (isDragging_ && draggedKeyFrame_)
    {
        HandleKeyFrameDrag(draggedKeyFrame_);
    }

    // リサイズ中
    if ((isResizingLeft_ || isResizingRight_) && resizingKeyFrame_)
    {
        HandleKeyFrameResize(resizingKeyFrame_);
    }

    // マウスリリース
    if (ImGui::IsMouseReleased(0))
    {
        isDragging_ = false;
        isResizingLeft_ = false;
        isResizingRight_ = false;
        draggedKeyFrame_ = nullptr;
        resizingKeyFrame_ = nullptr;
    }

    // 右クリックメニュー
    if (ImGui::IsMouseClicked(1) && selectedKeyFrame_)
    {
        ImGui::OpenPopup("KeyFrameContextMenu");
    }

    HandleContextMenu(animator, selectedKeyFrame_);

    // マウスホイールでズーム
    if (io.MouseWheel != 0.0f)
    {
        timeScale_ += io.MouseWheel * 10.0f;
        timeScale_ = std::clamp(timeScale_, 10.0f, 500.0f);
    }
}

void TimelineEditor::HandleKeyFrameDrag(KeyFrame* keyframe)
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 delta = ImVec2(io.MousePos.x - dragStartPos_.x, io.MousePos.y - dragStartPos_.y);

    float timeDelta = PixelToTime(delta.x);
    float newStartTime = dragStartTime_ + timeDelta;

    if (snapToGrid_)
    {
        newStartTime = SnapToGrid(newStartTime);
    }

    float duration = keyframe->EndTime - keyframe->StartTime;
    keyframe->StartTime = std::max(0.0f, newStartTime);
    keyframe->EndTime = keyframe->StartTime + duration;

    // レイヤー変更
    int layerDelta = static_cast<int>(delta.y / layerHeight_);
    keyframe->Layer = std::max(0, keyframe->Layer + layerDelta);

    if (layerDelta != 0)
    {
        dragStartPos_.y = io.MousePos.y;
    }
}

void TimelineEditor::HandleKeyFrameResize(KeyFrame* keyframe)
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 origin = GetTimelineOrigin();

    float mouseTime = PixelToTime(io.MousePos.x - origin.x - leftPanelWidth_);

    if (snapToGrid_)
    {
        mouseTime = SnapToGrid(mouseTime);
    }

    if (isResizingLeft_)
    {
        keyframe->StartTime = std::clamp(mouseTime, 0.0f, keyframe->EndTime - 0.1f);
    }
    else if (isResizingRight_)
    {
        keyframe->EndTime = std::max(mouseTime, keyframe->StartTime + 0.1f);
    }
}

void TimelineEditor::HandleContextMenu(Animator2D* animator, KeyFrame* keyframe)
{
    if (ImGui::BeginPopup("KeyFrameContextMenu"))
    {
        if (ImGui::MenuItem("削除"))
        {
            animator->RemoveKeyFrame(keyframe);
            selectedKeyFrame_ = nullptr;
        }

        ImGui::Separator();

        ImGui::Text("開始: %.2fs", keyframe->StartTime);
        ImGui::Text("終了: %.2fs", keyframe->EndTime);
        ImGui::Text("レイヤー: %d", keyframe->Layer);

        ImGui::EndPopup();
    }
}

float TimelineEditor::TimeToPixel(float time) const
{
    return time * timeScale_ - scrollX_;
}

float TimelineEditor::PixelToTime(float pixel) const
{
    return (pixel + scrollX_) / timeScale_;
}

float TimelineEditor::SnapToGrid(float time) const
{
    return std::round(time / snapInterval_) * snapInterval_;
}

ImVec2 TimelineEditor::GetTimelineOrigin() const
{
    return ImGui::GetCursorScreenPos();
}