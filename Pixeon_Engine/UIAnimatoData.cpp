#include "UIAnimatoData.h"

void UIAnimatoData::Update()
{
    if (bFirst_)
    {
        bFirst_ = false;
        fStartTime_ = timeGetTime();
    }

    // 経過時間取得
    fNowTime_ = timeGetTime() - fStartTime_;

    //経過時間を元にアクティブ設定、非アクティブ化
    for (auto obj : KeyFrames_)
    {
        if (obj.StartTime >= fNowTime_ && obj.EndTime <= fNowTime_)
        {
            obj.Active = true;
        }
        else
        {
            obj.Active = false;
        }
    }


    for (auto obj : KeyFrames_)
    {
        if (obj.Active)
        {
            float Time;
            Time = obj.StartTime - fNowTime_;
            float TotalTime = obj.EndTime - obj.StartTime;
            // 位置の比較
            if (obj.StartTransform.Position != obj.EndTransform.Position)
            {
                vec2 Temp = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.Position, obj.EndTransform.Position, TotalTime);
                DirectX::XMFLOAT2 Pos;
                Pos.x = Temp.x;
                Pos.y = Temp.y;
                obj.Image->SetOffset2D(Pos);
            }
            // 回転の比較
            if (obj.StartTransform.Rotation != obj.EndTransform.Rotation)
            {
                vec2 Temp = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.Rotation, obj.EndTransform.Rotation, TotalTime);
                DirectX::XMFLOAT2 Rot;
                Rot.x = Temp.x;
                Rot.y = Temp.y;
            }
            // サイズの比較
            if (obj.StartTransform.Scale != obj.EndTransform.Scale)
            {
                vec2 Temp = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.Scale, obj.EndTransform.Scale, TotalTime);
                DirectX::XMFLOAT2 Scl;
                Scl.x = Temp.x;
                Scl.y = Temp.y;
                obj.Image->SetSize2D(Scl);
            }
            // UV位置の比較
            if (obj.StartTransform.UVPostion != obj.EndTransform.UVPostion)
            {
                vec2 Temp = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.UVPostion, obj.EndTransform.UVPostion, TotalTime);
                DirectX::XMFLOAT4 temp = obj.Image->GetUVRect();
                temp.x = Temp.x;
                temp.y = Temp.y;
                obj.Image->SetUVRect(temp);
            }
            // UVサイズの比較
            if (obj.StartTransform.UVScale != obj.EndTransform.UVScale)
            {
                vec2 Temp = EaseByBezierCurve(obj.CurveInfo, obj.StartTransform.UVScale, obj.EndTransform.UVScale, TotalTime);
                DirectX::XMFLOAT4 UVS = obj.Image->GetUVRect();
                UVS.z = Temp.x;
                UVS.w = Temp.y;
                obj.Image->SetUVRect(UVS);
            }
        }
    }



    if (timeGetTime() - fStartTime_ >= fTotalDuration_ && bLoop_)
    {
        bFirst_ = true;
    }
}

vec2 UIAnimatoData::EaseByBezierCurve(const CurveData& curve, const vec2& startvalue, const vec2& endvalue, float elapsed, float duration)
{
    float t;
    if (duration <= 0.0f) {
        t = elapsed;
    }
    else {
        t = elapsed / duration;
    }
    if (t <= 0.0f) return startvalue;
    if (t >= 1.0f) return endvalue;

    vec2 authoredVec = curve.EndPoint - curve.StartPoint;
    vec2 currentVec = endvalue - startvalue;
    float authoredLen = Length(authoredVec);
    float currentLen = Length(currentVec);

    float scale = 1.0f;
    const float EPS = 1e-6f;
    if (authoredLen > EPS) {
        scale = currentLen / authoredLen;
    }

    vec2 offset1 = curve.ControlPoint1 - curve.StartPoint;
    vec2 offset2 = curve.ControlPoint2 - curve.EndPoint;

    vec2 p0 = startvalue;
    vec2 p1 = startvalue + offset1 * scale;
    vec2 p2 = endvalue + offset2 * scale;
    vec2 p3 = endvalue;

    return EvalCubicBezier(p0, p1, p2, p3, t);
}
