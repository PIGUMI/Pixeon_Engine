#include "LightComponent.h"
#include "Object.h"
#include "Scene.h"
#include "EditrGUI.h"
#include "IMGUI/imgui.h"

void LightComponent::Init(Object* owner) {
    _Parent = owner;
    _ComponentName = "Light";
    _Type = ComponentManager::COMPONENT_TYPE::LIGHT;
    // Scene へ登録
    if (owner && owner->GetParentScene()) {
        owner->GetParentScene()->RegisterLight(this);
    }
}

void LightComponent::UInit() {
    // Scene から除去
    if (_Parent && _Parent->GetParentScene())
        _Parent->GetParentScene()->UnregisterLight(this);
}

void LightComponent::EditUpdate() {
    // 必要なら動的処理（アニメライト等）。今は特別なし。
}

DirectX::XMFLOAT3 LightComponent::GetWorldPosition() const {
    if (!_Parent) return { 0,0,0 };
    Transform t = _Parent->GetTransform();
    return t.position;
}

DirectX::XMFLOAT3 LightComponent::GetWorldDirection() const {
    if (!_Parent) return { 0,-1,0 };
    Transform t = _Parent->GetTransform();
    float cy = cosf(t.rotation.y);
    float sy = sinf(t.rotation.y);
    float cx = cosf(t.rotation.x);
    float sx = sinf(t.rotation.x);
    // 左手系: forward ( +Z 前 ) の場合
    DirectX::XMFLOAT3 f{ sy * cx, -sx, cy * cx };
    // 方向は正規化
    float len = sqrtf(f.x * f.x + f.y * f.y + f.z * f.z);
    if (len > 0.0001f) { f.x /= len; f.y /= len; f.z /= len; }
    return f;
}

void LightComponent::DrawInspector() {
    auto SJ = [](const char* s)->std::string { return EditrGUI::GetInstance()->ShiftJISToUTF8(s); };
    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        int typeIndex = (int)m_type;
        const char* types[] = { "Directional", "Point", "Spot" };
        if (ImGui::Combo("Type", &typeIndex, types, IM_ARRAYSIZE(types))) {
            m_type = (LightType)typeIndex;
        }
        ImGui::ColorEdit3("Color", (float*)&m_color);
        ImGui::DragFloat("Intensity", &m_intensity, 0.01f, 0.0f, 100.0f);
        ImGui::Checkbox("Enabled", &m_enabled);
        if (m_type != LightType::Directional) {
            ImGui::DragFloat("Range", &m_range, 0.1f, 0.1f, 1000.0f);
        }
        if (m_type == LightType::Spot) {
            ImGui::DragFloat("SpotInnerDeg", &m_spotInnerDeg, 0.1f, 0.1f, 89.0f);
            ImGui::DragFloat("SpotOuterDeg", &m_spotOuterDeg, 0.1f, m_spotInnerDeg, 90.0f);
        }
    }
}

void LightComponent::SaveToFile(std::ostream& out) {
    out << (int)m_type << " "
        << m_color.x << " " << m_color.y << " " << m_color.z << " "
        << m_intensity << " " << m_range << " "
        << m_spotInnerDeg << " " << m_spotOuterDeg << " "
        << m_enabled << " ";
}

void LightComponent::LoadFromFile(std::istream& in) {
    int t; in >> t;
    m_type = (LightType)t;
    in >> m_color.x >> m_color.y >> m_color.z;
    in >> m_intensity >> m_range;
    in >> m_spotInnerDeg >> m_spotOuterDeg;
    in >> m_enabled;
}