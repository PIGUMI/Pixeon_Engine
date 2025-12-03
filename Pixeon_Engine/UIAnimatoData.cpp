#include "UIAnimatoData.h"

void UIAnimatoData::Update()
{

}

vec2 UIAnimatoData::EaseByBezierCurve(const CurveData& curve, const vec2& startPos, const vec2& endPos, float elapsed, float duration)
{
    float t;
    if (duration <= 0.0f) {
        t = elapsed;
    }
    else {
        t = elapsed / duration;
    }
    if (t <= 0.0f) return startPos;
    if (t >= 1.0f) return endPos;

    vec2 authoredVec = curve.EndPoint - curve.StartPoint;
    vec2 currentVec = endPos - startPos;
    float authoredLen = Length(authoredVec);
    float currentLen = Length(currentVec);

    float scale = 1.0f;
    const float EPS = 1e-6f;
    if (authoredLen > EPS) {
        scale = currentLen / authoredLen;
    }

    vec2 offset1 = curve.ControlPoint1 - curve.StartPoint;
    vec2 offset2 = curve.ControlPoint2 - curve.EndPoint;

    vec2 p0 = startPos;
    vec2 p1 = startPos + offset1 * scale;
    vec2 p2 = endPos + offset2 * scale;
    vec2 p3 = endPos;

    return EvalCubicBezier(p0, p1, p2, p3, t);
}
