#include "EasingGraph.h"

void DrawEasingGraph(CurveData& curve, const char* label, ImVec2 size)
{
    ImVec2 parent_pos = ImGui::GetCursorScreenPos();
    ImGui::BeginChild(label, {451.0f,460.0f}, true);

    // Childの左上座標（BeginChild後のGetCursorScreenPosはChild内部の左上になる）
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();

    // 右下座標の計算方法を修正
    ImVec2 canvas_p1 = ImVec2(canvas_p0.x + size.x, canvas_p0.y + size.y);

    // 枠線の内側に描画するため1px分だけ内側に
    canvas_p0.x += 1; canvas_p0.y += 1;
    canvas_p1.x -= 1; canvas_p1.y -= 1;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // 枠描画
    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(200, 200, 200, 255));

    // 座標変換
    auto toScreen = [&](DirectX::XMFLOAT2 p) {
        float x = canvas_p0.x + p.x * (canvas_p1.x - canvas_p0.x);
        float y = canvas_p1.y - p.y * (canvas_p1.y - canvas_p0.y); // y反転
        return ImVec2(x, y);
        };
    auto toLogic = [&](ImVec2 p) {
        float x = (p.x - canvas_p0.x) / (canvas_p1.x - canvas_p0.x);
        float y = (canvas_p1.y - p.y) / (canvas_p1.y - canvas_p0.y);
        return DirectX::XMFLOAT2(x, y);
        };


    // 制御点
    DirectX::XMFLOAT2 pts[] = {
        curve.StartPoint,
        curve.ControlPoint1,
        curve.ControlPoint2,
        curve.EndPoint
    };

    // 線分でベジェ曲線
    ImVec2 prev = toScreen(pts[0]);
    for (int i = 1; i <= 64; ++i) {
        float t = i / 64.0f;
        // ベジェ補間
        auto bezier = [](DirectX::XMFLOAT2 p0, DirectX::XMFLOAT2 p1, DirectX::XMFLOAT2 p2, DirectX::XMFLOAT2 p3, float t) {
            float u = 1.f - t;
            float tt = t * t; float uu = u * u;
            float uuu = uu * u; float ttt = tt * t;
            DirectX::XMFLOAT2 p;
            p.x = uuu * p0.x + 3 * uu * t * p1.x + 3 * u * tt * p2.x + ttt * p3.x;
            p.y = uuu * p0.y + 3 * uu * t * p1.y + 3 * u * tt * p2.y + ttt * p3.y;
            return p;
            };
        auto cv = bezier(pts[0], pts[1], pts[2], pts[3], t);
        ImVec2 curr = toScreen(cv);
        draw_list->AddLine(prev, curr, IM_COL32(80, 180, 255, 255), 3.0f);
        prev = curr;
    }

    // 制御点同士の線
    draw_list->AddLine(toScreen(pts[0]), toScreen(pts[1]), IM_COL32(180, 180, 80, 80), 1.0f);
    draw_list->AddLine(toScreen(pts[2]), toScreen(pts[3]), IM_COL32(180, 180, 80, 80), 1.0f);

    static int drag_idx = -1;
    static bool dragging = false;

    // 制御点の描画とドラッグ
    for (int i = 0; i < 4; ++i) {
        ImVec2 p = toScreen(pts[i]);
        ImU32 color = (i == 0 || i == 3) ? IM_COL32(0, 220, 0, 255) : IM_COL32(220, 0, 0, 255);
        float radius = (i == 0 || i == 3) ? 6.0f : 8.0f;

        draw_list->AddCircleFilled(p, radius, color);

        // ドラッグ処理
        ImGui::SetCursorScreenPos(ImVec2(p.x - radius, p.y - radius));
        ImGui::InvisibleButton((std::string(label) + "##drag" + std::to_string(i)).c_str(), ImVec2(radius * 2, radius * 2));
        if (ImGui::IsItemActive() && ImGui::IsMouseDown(0)) {
            drag_idx = i;
            dragging = true;
        }
        if (dragging && drag_idx == i && ImGui::IsMouseDragging(0)) {
            ImVec2 mouse = ImGui::GetIO().MousePos;
            DirectX::XMFLOAT2 logical = toLogic(mouse);
            if (i == 0) logical = { 0,0 }; // 始点は固定
            if (i == 3) logical = { 1,1 }; // 終点は固定
            logical.x = std::clamp(logical.x, 0.f, 1.f);
            logical.y = std::clamp(logical.y, 0.f, 1.f);

            if (i == 1) curve.ControlPoint1 = logical;
            if (i == 2) curve.ControlPoint2 = logical;
        }
        if (!ImGui::IsMouseDown(0)) {
            dragging = false;
            drag_idx = -1;
        }
    }

    ImGui::EndChild();
}
