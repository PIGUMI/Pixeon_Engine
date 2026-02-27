#include "LightComponent.h"
#include "Object.h"
#include "Scene.h"
#include "GUI.h"
#include "IMGUI/imgui.h"
#include "_Geometry.h"
#include "CameraComponent.h"

void LightComponent::Init(AbstractObject* owner) {
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
}

void LightComponent::Draw(int Layer) {
	if (!m_debugDraw || !_Parent || !_Parent->GetParentScene()) return;

	switch (m_type) {
	case LightType::Directional:
		DrawDirectionalLight();
		break;
	case LightType::Point:
		DrawPointLight();
		break;
	case LightType::Spot:
		DrawSpotLight();
		break;
	}
}

DirectX::XMFLOAT3 LightComponent::GetWorldPosition() const {
	if (!_Parent) return { 0,0,0 };
	Transform t = _Parent->GetWorldTransform();

	// オフセットを回転に応じて適用
	DirectX::XMMATRIX rotMat = DirectX::XMMatrixRotationRollPitchYaw(
		t.rotation.x, t.rotation.y, t.rotation.z
	);
	DirectX::XMVECTOR offsetVec = DirectX::XMLoadFloat3(&m_offset);
	DirectX::XMVECTOR rotatedOffset = DirectX::XMVector3Transform(offsetVec, rotMat);

	DirectX::XMFLOAT3 rotatedOffsetF3;
	DirectX::XMStoreFloat3(&rotatedOffsetF3, rotatedOffset);

	return {
		t.position.x + rotatedOffsetF3.x,
		t.position.y + rotatedOffsetF3.y,
		t.position.z + rotatedOffsetF3.z
	};
}

DirectX::XMFLOAT3 LightComponent::GetWorldDirection() const {
	if (!_Parent) return { 0,-1,0 };
	Transform t = _Parent->GetWorldTransform();

	float totalRotX = t.rotation.x + m_rotationOffset.x;
	float totalRotY = t.rotation.y + m_rotationOffset.y;
	float totalRotZ = t.rotation.z + m_rotationOffset.z;

	DirectX::XMMATRIX rotMat = DirectX::XMMatrixRotationRollPitchYaw(
		totalRotX, totalRotY, totalRotZ
	);

	DirectX::XMVECTOR forwardVec = DirectX::XMVectorSet(0, 0, -1, 0);
	DirectX::XMVECTOR rotatedDir = DirectX::XMVector3Transform(forwardVec, rotMat);

	DirectX::XMFLOAT3 direction;
	DirectX::XMStoreFloat3(&direction, rotatedDir);

	float len = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
	if (len > 0.0001f) {
		direction.x /= len;
		direction.y /= len;
		direction.z /= len;
	}

	return direction;
}

void LightComponent::DrawInspector() {
	auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };
	std::string label = SJ(_ComponentName.c_str());
	std::string Ptr = std::to_string((uintptr_t)this);
	label += "###" + Ptr;
	if (ImGui::CollapsingHeader(label.c_str())) {
		int typeIndex = (int)m_type;
		const char* types[] = { "Directional", "Point", "Spot" };
		if (ImGui::Combo("Type", &typeIndex, types, IM_ARRAYSIZE(types))) {
			m_type = (LightType)typeIndex;
		}
		ImGui::ColorEdit3("Color", (float*)&m_color);
		ImGui::DragFloat("Intensity", &m_intensity, 0.01f, 0.0f, 100.0f);

		// 位置オフセット
		ImGui::Text(SJ("位置オフセット").c_str());
		ImGui::DragFloat3("##Offset", (float*)&m_offset, 0.1f);

		ImGui::Text(SJ("回転オフセット").c_str());
		DirectX::XMFLOAT3 rotOffsetDeg = {
			DirectX::XMConvertToDegrees(m_rotationOffset.x),
			DirectX::XMConvertToDegrees(m_rotationOffset.y),
			DirectX::XMConvertToDegrees(m_rotationOffset.z)
		};
		if (ImGui::DragFloat3("##RotationOffset", (float*)&rotOffsetDeg, 0.1f)) {
			m_rotationOffset.x = DirectX::XMConvertToRadians(rotOffsetDeg.x);
			m_rotationOffset.y = DirectX::XMConvertToRadians(rotOffsetDeg.y);
			m_rotationOffset.z = DirectX::XMConvertToRadians(rotOffsetDeg.z);
		}

		ImGui::Checkbox("Enabled", &m_enabled);
		ImGui::Checkbox(SJ("デバッグ描画").c_str(), &m_debugDraw);
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
		<< m_enabled << " " << m_debugDraw << " "
		<< m_offset.x << " " << m_offset.y << " " << m_offset.z << " "
		<< m_rotationOffset.x << " " << m_rotationOffset.y << " " << m_rotationOffset.z << " ";
}

void LightComponent::LoadFromFile(std::istream& in) {
	int t; in >> t;
	m_type = (LightType)t;
	in >> m_color.x >> m_color.y >> m_color.z;
	in >> m_intensity >> m_range;
	in >> m_spotInnerDeg >> m_spotOuterDeg;
	in >> m_enabled >> m_debugDraw;
	in >> m_offset.x >> m_offset.y >> m_offset.z;
	in >> m_rotationOffset.x >> m_rotationOffset.y >> m_rotationOffset.z;
}

void LightComponent::DrawDirectionalLight() {
	auto scene = _Parent->GetParentScene();
	auto cam = scene->GetMainCamera();
	if (!cam) return;

	DirectX::XMFLOAT4X4 proj = cam->GetProjectionMatrix();
	DirectX::XMFLOAT4X4 view = cam->GetViewMatrix();
	DirectX::XMFLOAT4X4 world;
	DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity());

	DirectX::XMFLOAT3 pos = GetWorldPosition();
	DirectX::XMFLOAT3 dir = GetWorldDirection();

	float iconSize = 0.5f;
	DirectX::XMFLOAT4 color(m_color.x, m_color.y, m_color.z, 1.0f);

	// 中心から8方向に短線を描画（太陽マーク風）
	DirectX::XMFLOAT3 axes[] = {
		{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0},
		{0, 0, 1}, {0, 0, -1}, {0.7f, 0.7f, 0}, {-0.7f, -0.7f, 0}
	};

	for (auto& axis : axes) {
		DirectX::XMFLOAT3 start = pos;
		DirectX::XMFLOAT3 end = {
			pos.x + axis.x * iconSize,
			pos.y + axis.y * iconSize,
			pos.z + axis.z * iconSize
		};
		LineRenderer::GetInstance()->DrawLine(start, end, color, world, view, proj, 0.02f);
	}

	// 光の方向を示す矢印
	float arrowLength = 2.0f;
	DirectX::XMFLOAT3 arrowEnd = {
		pos.x + dir.x * arrowLength,
		pos.y + dir.y * arrowLength,
		pos.z + dir.z * arrowLength
	};
	LineRenderer::GetInstance()->DrawLine(pos, arrowEnd, color, world, view, proj, 0.05f);

	// 矢の先端
	DirectX::XMVECTOR dirVec = DirectX::XMLoadFloat3(&dir);
	DirectX::XMVECTOR upVec = DirectX::XMVectorSet(0, 1, 0, 0);
	if (fabsf(dir.y) > 0.99f) {
		upVec = DirectX::XMVectorSet(1, 0, 0, 0);
	}
	DirectX::XMVECTOR rightVec = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(upVec, dirVec));

	float arrowSize = 0.3f;
	for (int i = 0; i < 4; i++) {
		float angle = DirectX::XM_2PI * i / 4.0f;
		DirectX::XMVECTOR offset = DirectX::XMVectorScale(rightVec, cosf(angle) * arrowSize);
		DirectX::XMVECTOR upOffset = DirectX::XMVector3Cross(dirVec, rightVec);
		upOffset = DirectX::XMVectorScale(upOffset, sinf(angle) * arrowSize);
		offset = DirectX::XMVectorAdd(offset, upOffset);

		DirectX::XMVECTOR arrowPoint = DirectX::XMLoadFloat3(&arrowEnd);
		arrowPoint = DirectX::XMVectorSubtract(arrowPoint, DirectX::XMVectorScale(dirVec, arrowSize));
		arrowPoint = DirectX::XMVectorAdd(arrowPoint, offset);

		DirectX::XMFLOAT3 arrowPointF3;
		DirectX::XMStoreFloat3(&arrowPointF3, arrowPoint);
		LineRenderer::GetInstance()->DrawLine(arrowEnd, arrowPointF3, color, world, view, proj, 0.03f);
	}
}

void LightComponent::DrawPointLight() {
	auto scene = _Parent->GetParentScene();
	auto cam = scene->GetMainCamera();
	if (!cam) return;

	DirectX::XMFLOAT4X4 proj = cam->GetProjectionMatrix();
	DirectX::XMFLOAT4X4 view = cam->GetViewMatrix();
	DirectX::XMFLOAT4X4 world;
	DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity());

	DirectX::XMFLOAT3 pos = GetWorldPosition();
	DirectX::XMFLOAT4 color(m_color.x, m_color.y, m_color.z, 1.0f);

	int segments = 16;
	float radius = m_range;

	// XY平面の円
	for (int i = 0; i < segments; i++) {
		float angle1 = DirectX::XM_2PI * i / segments;
		float angle2 = DirectX::XM_2PI * (i + 1) / segments;
		DirectX::XMFLOAT3 p1 = {
			pos.x + radius * cosf(angle1),
			pos.y + radius * sinf(angle1),
			pos.z
		};
		DirectX::XMFLOAT3 p2 = {
			pos.x + radius * cosf(angle2),
			pos.y + radius * sinf(angle2),
			pos.z
		};
		LineRenderer::GetInstance()->DrawLine(p1, p2, color, world, view, proj, 0.02f);
	}

	// XZ平面の円
	for (int i = 0; i < segments; i++) {
		float angle1 = DirectX::XM_2PI * i / segments;
		float angle2 = DirectX::XM_2PI * (i + 1) / segments;
		DirectX::XMFLOAT3 p1 = {
			pos.x + radius * cosf(angle1),
			pos.y,
			pos.z + radius * sinf(angle1)
		};
		DirectX::XMFLOAT3 p2 = {
			pos.x + radius * cosf(angle2),
			pos.y,
			pos.z + radius * sinf(angle2)
		};
		LineRenderer::GetInstance()->DrawLine(p1, p2, color, world, view, proj, 0.02f);
	}

	// YZ平面の円
	for (int i = 0; i < segments; i++) {
		float angle1 = DirectX::XM_2PI * i / segments;
		float angle2 = DirectX::XM_2PI * (i + 1) / segments;
		DirectX::XMFLOAT3 p1 = {
			pos.x,
			pos.y + radius * cosf(angle1),
			pos.z + radius * sinf(angle1)
		};
		DirectX::XMFLOAT3 p2 = {
			pos.x,
			pos.y + radius * cosf(angle2),
			pos.z + radius * sinf(angle2)
		};
		LineRenderer::GetInstance()->DrawLine(p1, p2, color, world, view, proj, 0.02f);
	}

	// 中心マーカー（十字線表示）
	float markerSize = 0.3f;
	DirectX::XMFLOAT3 axes[] = {
		{markerSize, 0, 0}, {-markerSize, 0, 0},
		{0, markerSize, 0}, {0, -markerSize, 0},
		{0, 0, markerSize}, {0, 0, -markerSize}
	};
	for (int i = 0; i < 6; i += 2) {
		DirectX::XMFLOAT3 start = { pos.x + axes[i].x, pos.y + axes[i].y, pos.z + axes[i].z };
		DirectX::XMFLOAT3 end = { pos.x + axes[i + 1].x, pos.y + axes[i + 1].y, pos.z + axes[i + 1].z };
		LineRenderer::GetInstance()->DrawLine(start, end, color, world, view, proj, 0.03f);
	}
}

void LightComponent::DrawSpotLight() {
	auto scene = _Parent->GetParentScene();
	auto cam = scene->GetMainCamera();
	if (!cam) return;

	DirectX::XMFLOAT4X4 proj = cam->GetProjectionMatrix();
	DirectX::XMFLOAT4X4 view = cam->GetViewMatrix();
	DirectX::XMFLOAT4X4 world;
	DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity());

	DirectX::XMFLOAT3 pos = GetWorldPosition();
	DirectX::XMFLOAT3 dir = GetWorldDirection();
	DirectX::XMFLOAT4 color(m_color.x, m_color.y, m_color.z, 1.0f);

	// スポットライトのコーン形状
	float outerAngle = DirectX::XMConvertToRadians(m_spotOuterDeg);
	float innerAngle = DirectX::XMConvertToRadians(m_spotInnerDeg);
	float outerRadius = m_range * tanf(outerAngle * 0.5f);
	float innerRadius = m_range * tanf(innerAngle * 0.5f);

	// 光線の終点
	DirectX::XMFLOAT3 endPos = {
		pos.x + dir.x * m_range,
		pos.y + dir.y * m_range,
		pos.z + dir.z * m_range
	};

	// DirectXMathを使用して垂直ベクトルを正確に計算
	DirectX::XMVECTOR dirVec = DirectX::XMLoadFloat3(&dir);
	DirectX::XMVECTOR upVec = DirectX::XMVectorSet(0, 1, 0, 0);

	// dirがほぼ上向きまたは下向きの場合、別の基準ベクトルを使用
	if (fabsf(dir.y) > 0.99f) {
		upVec = DirectX::XMVectorSet(1, 0, 0, 0);
	}

	// 右方向ベクトルを計算
	DirectX::XMVECTOR rightVec = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(upVec, dirVec));
	// 上方向ベクトルを再計算（直交性を保証）
	DirectX::XMVECTOR realUpVec = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(dirVec, rightVec));

	int segments = 16;

	// 外側のコーン円
	for (int i = 0; i < segments; i++) {
		float angle1 = DirectX::XM_2PI * i / segments;
		float angle2 = DirectX::XM_2PI * (i + 1) / segments;

		// 円周上の点を計算
		DirectX::XMVECTOR offset1 = DirectX::XMVectorAdd(
			DirectX::XMVectorScale(rightVec, cosf(angle1) * outerRadius),
			DirectX::XMVectorScale(realUpVec, sinf(angle1) * outerRadius)
		);
		DirectX::XMVECTOR offset2 = DirectX::XMVectorAdd(
			DirectX::XMVectorScale(rightVec, cosf(angle2) * outerRadius),
			DirectX::XMVectorScale(realUpVec, sinf(angle2) * outerRadius)
		);

		DirectX::XMVECTOR p1Vec = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&endPos), offset1);
		DirectX::XMVECTOR p2Vec = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&endPos), offset2);

		DirectX::XMFLOAT3 p1, p2;
		DirectX::XMStoreFloat3(&p1, p1Vec);
		DirectX::XMStoreFloat3(&p2, p2Vec);

		LineRenderer::GetInstance()->DrawLine(p1, p2, color, world, view, proj, 0.02f);

		if (i % 2 == 0) {
			LineRenderer::GetInstance()->DrawLine(pos, p1, color, world, view, proj, 0.02f);
		}
	}

	for (int i = 0; i < segments; i += 2) {
		float angle1 = DirectX::XM_2PI * i / segments;
		float angle2 = DirectX::XM_2PI * (i + 1) / segments;

		DirectX::XMVECTOR offset1 = DirectX::XMVectorAdd(
			DirectX::XMVectorScale(rightVec, cosf(angle1) * innerRadius),
			DirectX::XMVectorScale(realUpVec, sinf(angle1) * innerRadius)
		);
		DirectX::XMVECTOR offset2 = DirectX::XMVectorAdd(
			DirectX::XMVectorScale(rightVec, cosf(angle2) * innerRadius),
			DirectX::XMVectorScale(realUpVec, sinf(angle2) * innerRadius)
		);

		DirectX::XMVECTOR p1Vec = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&endPos), offset1);
		DirectX::XMVECTOR p2Vec = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&endPos), offset2);

		DirectX::XMFLOAT3 p1, p2;
		DirectX::XMStoreFloat3(&p1, p1Vec);
		DirectX::XMStoreFloat3(&p2, p2Vec);

		LineRenderer::GetInstance()->DrawLine(p1, p2, color, world, view, proj, 0.02f);
	}

	LineRenderer::GetInstance()->DrawLine(pos, endPos, color, world, view, proj, 0.05f);
}