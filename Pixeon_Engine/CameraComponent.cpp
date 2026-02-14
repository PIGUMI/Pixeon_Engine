#include <fstream>
#include <sstream>
#include <iostream>
#include "CameraComponent.h"
#include "Object.h"
#include "Scene.h"
#include "GUI.h"
#include "_Geometry.h"

void CameraComponent::Init(AbstractObject* Prt) {
	_Parent = Prt;
	_ComponentName = "CameraComponent";
	_Type = ComponentManager::COMPONENT_TYPE::CAMERA;
	_PositionOffset = { 0.0f, 5.0f, -10.0f };
	_FixationOffset = { 0.0f, 0.0f, 0.0f };
	_Up = { 0.0f, 1.0f, 0.0f };
	_FOV = DirectX::XMConvertToRadians(60.0f);
	_AspectRatio = 16.0f / 9.0f;
	_NearPlane = 0.1f;
	_FarPlane = 1000.0f;
	_radius = 10.0f;
	_debugDraw = true;
}

void CameraComponent::EditUpdate() {
	// 親オブジェクトがない場合はスキップ
	if (!_Parent) return;

	auto trans = _Parent->GetWorldTransform();

	if (_IsChangeCalculation) {
		DirectX::XMFLOAT3 worldPos = GetWorldPosition();
		_FixationOffset.x = worldPos.x - cosf(_Rotation.y) * sinf(_Rotation.x) * _radius - trans.position.x;
		_FixationOffset.y = worldPos.y - sinf(_Rotation.y) * _radius - trans.position.y;
		_FixationOffset.z = worldPos.z - cosf(_Rotation.y) * cosf(_Rotation.x) * _radius - trans.position.z;
	}
	else {
		DirectX::XMFLOAT3 worldFix = GetWorldFixation();
		_PositionOffset.x = cosf(_Rotation.y) * sinf(_Rotation.x) * _radius + worldFix.x - trans.position.x;
		_PositionOffset.y = sinf(_Rotation.y) * _radius + worldFix.y - trans.position.y;
		_PositionOffset.z = cosf(_Rotation.y) * cosf(_Rotation.x) * _radius + worldFix.z - trans.position.z;
	}
}

void CameraComponent::InGameUpdate() {
	// 親オブジェクトがない場合はスキップ
	if (!_Parent) return;

	auto trans = _Parent->GetWorldTransform();

	if (_IsChangeCalculation) {
		DirectX::XMFLOAT3 worldPos = GetWorldPosition();
		_FixationOffset.x = worldPos.x - cosf(_Rotation.y) * sinf(_Rotation.x) * _radius - trans.position.x;
		_FixationOffset.y = worldPos.y - sinf(_Rotation.y) * _radius - trans.position.y;
		_FixationOffset.z = worldPos.z - cosf(_Rotation.y) * cosf(_Rotation.x) * _radius - trans.position.z;
	}
	else {
		DirectX::XMFLOAT3 worldFix = GetWorldFixation();
		_PositionOffset.x = cosf(_Rotation.y) * sinf(_Rotation.x) * _radius + worldFix.x - trans.position.x;
		_PositionOffset.y = sinf(_Rotation.y) * _radius + worldFix.y - trans.position.y;
		_PositionOffset.z = cosf(_Rotation.y) * cosf(_Rotation.x) * _radius + worldFix.z - trans.position.z;
	}
}

void CameraComponent::Draw(int Layer) {
	if (!_debugDraw || !_Parent || !_Parent->GetParentScene()) return;
	DrawCameraVisualization();
}

void CameraComponent::DrawCameraVisualization() {
	auto scene = _Parent->GetParentScene();
	auto mainCam = scene->GetMainCamera();
	if (!mainCam || mainCam == this) return;

	DirectX::XMFLOAT4X4 proj = mainCam->GetProjectionMatrix();
	DirectX::XMFLOAT4X4 view = mainCam->GetViewMatrix();
	DirectX::XMFLOAT4X4 world;
	DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity());

	DirectX::XMFLOAT3 camPos = GetWorldPosition();
	DirectX::XMFLOAT3 fixPos = GetWorldFixation();

	// 正しい前方向を計算（注視点方向）
	DirectX::XMFLOAT3 forward;
	forward.x = fixPos.x - camPos.x;  // 修正
	forward.y = fixPos.y - camPos.y;  // 修正
	forward.z = fixPos.z - camPos.z;  // 修正

	// 正規化
	float len = sqrtf(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	if (len > 0.0001f) {
		forward.x /= len;
		forward.y /= len;
		forward.z /= len;
	}

	DirectX::XMFLOAT3 right = GetRightVector();
	DirectX::XMFLOAT3 up = GetUpVector();

	DirectX::XMFLOAT4 color(0.2f, 0.8f, 1.0f, 1.0f);

	float iconSize = 0.5f;
	DirectX::XMFLOAT3 corners[8];
	for (int i = 0; i < 8; i++) {
		float sx = (i & 1) ? iconSize : -iconSize;
		float sy = (i & 2) ? iconSize : -iconSize;
		float sz = (i & 4) ? iconSize : -iconSize;

		corners[i].x = camPos.x + right.x * sx + up.x * sy + forward.x * sz;
		corners[i].y = camPos.y + right.y * sx + up.y * sy + forward.y * sz;
		corners[i].z = camPos.z + right.z * sx + up.z * sy + forward.z * sz;
	}

	int edges[12][2] = {
		{0,1},{2,3},{4,5},{6,7},
		{0,2},{1,3},{4,6},{5,7},
		{0,4},{1,5},{2,6},{3,7}
	};

	for (int i = 0; i < 12; i++) {
		LineRenderer::GetInstance()->DrawLine(
			corners[edges[i][0]],
			corners[edges[i][1]],
			color, world, view, proj, 0.03f
		);
	}

	float arrowLength = 3.0f;
	DirectX::XMFLOAT3 arrowEnd = {
		camPos.x + forward.x * arrowLength,
		camPos.y + forward.y * arrowLength,
		camPos.z + forward.z * arrowLength
	};
	LineRenderer::GetInstance()->DrawLine(camPos, arrowEnd, color, world, view, proj, 0.05f);

	DirectX::XMVECTOR fwdVec = DirectX::XMLoadFloat3(&forward);
	DirectX::XMVECTOR rightVec = DirectX::XMLoadFloat3(&right);
	DirectX::XMVECTOR upVec = DirectX::XMLoadFloat3(&up);

	float arrowSize = 0.4f;
	for (int i = 0; i < 4; i++) {
		float angle = DirectX::XM_2PI * i / 4.0f;
		DirectX::XMVECTOR offset = DirectX::XMVectorAdd(
			DirectX::XMVectorScale(rightVec, cosf(angle) * arrowSize),
			DirectX::XMVectorScale(upVec, sinf(angle) * arrowSize)
		);

		DirectX::XMVECTOR arrowPoint = DirectX::XMLoadFloat3(&arrowEnd);
		arrowPoint = DirectX::XMVectorSubtract(arrowPoint, DirectX::XMVectorScale(fwdVec, arrowSize));
		arrowPoint = DirectX::XMVectorAdd(arrowPoint, offset);

		DirectX::XMFLOAT3 arrowPointF3;
		DirectX::XMStoreFloat3(&arrowPointF3, arrowPoint);
		LineRenderer::GetInstance()->DrawLine(arrowEnd, arrowPointF3, color, world, view, proj, 0.03f);
	}

	DirectX::XMFLOAT4 fixColor(1.0f, 1.0f, 0.0f, 1.0f);
	LineRenderer::GetInstance()->DrawLine(camPos, fixPos, fixColor, world, view, proj, 0.02f);

	float markerSize = 0.2f;
	DirectX::XMFLOAT3 axes[] = {
		{markerSize, 0, 0}, {-markerSize, 0, 0},
		{0, markerSize, 0}, {0, -markerSize, 0},
		{0, 0, markerSize}, {0, 0, -markerSize}
	};
	for (int i = 0; i < 6; i += 2) {
		DirectX::XMFLOAT3 start = {
			fixPos.x + axes[i].x,
			fixPos.y + axes[i].y,
			fixPos.z + axes[i].z
		};
		DirectX::XMFLOAT3 end = {
			fixPos.x + axes[i + 1].x,
			fixPos.y + axes[i + 1].y,
			fixPos.z + axes[i + 1].z
		};
		LineRenderer::GetInstance()->DrawLine(start, end, fixColor, world, view, proj, 0.03f);
	}

	DrawFrustum();
}

void CameraComponent::DrawFrustum() {
	auto scene = _Parent->GetParentScene();
	auto mainCam = scene->GetMainCamera();
	if (!mainCam || mainCam == this) return;

	DirectX::XMFLOAT4X4 proj = mainCam->GetProjectionMatrix();
	DirectX::XMFLOAT4X4 view = mainCam->GetViewMatrix();
	DirectX::XMFLOAT4X4 world;
	DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity());

	DirectX::XMFLOAT3 camPos = GetWorldPosition();
	DirectX::XMFLOAT3 fixPos = GetWorldFixation();

	// 正しい前方向を計算（注視点方向）
	DirectX::XMFLOAT3 forward;
	forward.x = fixPos.x - camPos.x;  // 修正
	forward.y = fixPos.y - camPos.y;  // 修正
	forward.z = fixPos.z - camPos.z;  // 修正

	// 正規化
	float len = sqrtf(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	if (len > 0.0001f) {
		forward.x /= len;
		forward.y /= len;
		forward.z /= len;
	}

	DirectX::XMFLOAT3 right = GetRightVector();
	DirectX::XMFLOAT3 up = GetUpVector();

	DirectX::XMFLOAT3 nearCenter = {
		camPos.x + forward.x * _NearPlane,
		camPos.y + forward.y * _NearPlane,
		camPos.z + forward.z * _NearPlane
	};

	DirectX::XMFLOAT3 farCenter = {
		camPos.x + forward.x * _FarPlane,
		camPos.y + forward.y * _FarPlane,
		camPos.z + forward.z * _FarPlane
	};

	float nearHeight = 2.0f * tanf(_FOV * 0.5f) * _NearPlane;
	float nearWidth = nearHeight * _AspectRatio;
	float farHeight = 2.0f * tanf(_FOV * 0.5f) * _FarPlane;
	float farWidth = farHeight * _AspectRatio;

	DirectX::XMFLOAT3 nearCorners[4];
	for (int i = 0; i < 4; i++) {
		float hx = (i & 1) ? 0.5f : -0.5f;
		float hy = (i & 2) ? 0.5f : -0.5f;

		nearCorners[i].x = nearCenter.x + right.x * nearWidth * hx + up.x * nearHeight * hy;
		nearCorners[i].y = nearCenter.y + right.y * nearWidth * hx + up.y * nearHeight * hy;
		nearCorners[i].z = nearCenter.z + right.z * nearWidth * hx + up.z * nearHeight * hy;
	}

	DirectX::XMFLOAT3 farCorners[4];
	for (int i = 0; i < 4; i++) {
		float hx = (i & 1) ? 0.5f : -0.5f;
		float hy = (i & 2) ? 0.5f : -0.5f;

		farCorners[i].x = farCenter.x + right.x * farWidth * hx + up.x * farHeight * hy;
		farCorners[i].y = farCenter.y + right.y * farWidth * hx + up.y * farHeight * hy;
		farCorners[i].z = farCenter.z + right.z * farWidth * hx + up.z * farHeight * hy;
	}

	DirectX::XMFLOAT4 frustumColor(0.5f, 0.5f, 1.0f, 1.0f);

	for (int i = 0; i < 4; i++) {
		LineRenderer::GetInstance()->DrawLine(
			nearCorners[i],
			nearCorners[(i + 1) % 4],
			frustumColor, world, view, proj, 0.02f
		);
	}

	for (int i = 0; i < 4; i++) {
		LineRenderer::GetInstance()->DrawLine(
			farCorners[i],
			farCorners[(i + 1) % 4],
			frustumColor, world, view, proj, 0.02f
		);
	}

	for (int i = 0; i < 4; i++) {
		LineRenderer::GetInstance()->DrawLine(
			camPos,
			farCorners[i],
			frustumColor, world, view, proj, 0.01f
		);
	}
}

void CameraComponent::DrawInspector() {
	std::string label = GUI::GetInstance()->ShiftJISToUTF8(_ComponentName);
	std::string Ptr = std::to_string((uintptr_t)this);
	label += "###" + Ptr;

	if (ImGui::CollapsingHeader(GUI::GetInstance()->ShiftJISToUTF8(label).c_str())) {
		if (ImGui::BeginTable("CameraTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("Position Offset");
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat3("##PositionOffset", &_PositionOffset.x, 0.1f);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("Rotation");
			DirectX::XMFLOAT3 rot;
			rot.x = DirectX::XMConvertToDegrees(_Rotation.x);
			rot.y = DirectX::XMConvertToDegrees(_Rotation.y);
			rot.z = DirectX::XMConvertToDegrees(_Rotation.z);
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat3("##Rotation", &rot.x, 0.1f);
			_Rotation.x = DirectX::XMConvertToRadians(rot.x);
			_Rotation.y = DirectX::XMConvertToRadians(rot.y);
			_Rotation.z = DirectX::XMConvertToRadians(rot.z);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("Fixation Offset");
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat3("##FixationOffset", &_FixationOffset.x, 0.1f);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("Up");
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat3("##Up", &_Up.x, 0.1f);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("FOV");
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat("##FOV", &_FOV, 0.01f, DirectX::XMConvertToRadians(1.0f), DirectX::XMConvertToRadians(179.0f), "%. 3f");

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("AspectRatio");
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat("##AspectRatio", &_AspectRatio, 0.01f, 0.1f, 4.0f, "%.3f");

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("NearPlane");
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat("##NearPlane", &_NearPlane, 0.1f, 0.01f, 100.0f, "%.3f");

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("FarPlane");
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat("##FarPlane", &_FarPlane, 1.0f, 10.0f, 10000.0f, "%.3f");

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("Radius");
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat("##Radius", &_radius, 0.1f, 0.1f, 100.0f, "%.3f");

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("IsKeyMove");
			ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##IsKeyMove", &_IsKeyMove);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("IsChangeCalculation");
			ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##IsChangeCalculation", &_IsChangeCalculation);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("デバッグ描画").c_str());
			ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##DebugDraw", &_debugDraw);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("CameraNumber");
			ImGui::TableSetColumnIndex(1);
			std::stringstream ss;
			ss << _CameraNumber;
			std::string camNumStr = ss.str();
			char buf[256];
			strcpy_s(buf, camNumStr.c_str());
			if (ImGui::InputText("##CameraNumber", buf, sizeof(buf))) {
				_CameraNumber = std::stoi(std::string(buf));
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("現在のカメラに切り替える").c_str());
			ImGui::TableSetColumnIndex(1);
			if (ImGui::Button("Set Main Camera")) {
				_Parent->GetParentScene()->SetMainCamera(this);
			}
			ImGui::EndTable();
		}
	}
}

void CameraComponent::SaveToFile(std::ostream& out) {
	out << _PositionOffset.x << " " << _PositionOffset.y << " " << _PositionOffset.z << " ";
	out << _Rotation.x << " " << _Rotation.y << " " << _Rotation.z << " ";
	out << _FixationOffset.x << " " << _FixationOffset.y << " " << _FixationOffset.z << " ";
	out << _Up.x << " " << _Up.y << " " << _Up.z << " ";
	out << _FOV << " " << _AspectRatio << " " << _NearPlane << " " << _FarPlane << " ";
	out << _radius << " ";
	out << _IsKeyMove << " ";
	out << _IsChangeCalculation << " ";
	out << _CameraNumber << " ";
	out << _debugDraw << " ";
}

void CameraComponent::LoadFromFile(std::istream& in) {
	in >> _PositionOffset.x >> _PositionOffset.y >> _PositionOffset.z;
	in >> _Rotation.x >> _Rotation.y >> _Rotation.z;
	in >> _FixationOffset.x >> _FixationOffset.y >> _FixationOffset.z;
	in >> _Up.x >> _Up.y >> _Up.z;
	in >> _FOV >> _AspectRatio >> _NearPlane >> _FarPlane;
	in >> _radius;
	in >> _IsKeyMove;
	in >> _IsChangeCalculation;
	in >> _CameraNumber;
	in >> _debugDraw;
}

DirectX::XMFLOAT3 CameraComponent::GetWorldPosition() const {
	if (!_Parent) return _PositionOffset;

	Transform trans = _Parent->GetWorldTransform();

	// 親オブジェクトの回転行列を作成
	DirectX::XMMATRIX rotMat = DirectX::XMMatrixRotationRollPitchYaw(
		trans.rotation.x,
		trans.rotation.y,
		trans.rotation.z
	);

	// オフセットを親の回転に応じて変換
	DirectX::XMVECTOR offsetVec = DirectX::XMLoadFloat3(&_PositionOffset);
	DirectX::XMVECTOR rotatedOffset = DirectX::XMVector3Transform(offsetVec, rotMat);

	DirectX::XMFLOAT3 rotatedOffsetF3;
	DirectX::XMStoreFloat3(&rotatedOffsetF3, rotatedOffset);

	// 親の位置に回転済みオフセットを加算
	DirectX::XMFLOAT3 worldPos;
	worldPos.x = trans.position.x + rotatedOffsetF3.x;
	worldPos.y = trans.position.y + rotatedOffsetF3.y;
	worldPos.z = trans.position.z + rotatedOffsetF3.z;

	return worldPos;
}

DirectX::XMFLOAT3 CameraComponent::GetWorldFixation() const {
	if (!_Parent) return _FixationOffset;

	Transform trans = _Parent->GetWorldTransform();

	// 親オブジェクトの回転行列を作成
	DirectX::XMMATRIX rotMat = DirectX::XMMatrixRotationRollPitchYaw(
		trans.rotation.x,
		trans.rotation.y,
		trans.rotation.z
	);

	// オフセットを親の回転に応じて変換
	DirectX::XMVECTOR offsetVec = DirectX::XMLoadFloat3(&_FixationOffset);
	DirectX::XMVECTOR rotatedOffset = DirectX::XMVector3Transform(offsetVec, rotMat);

	DirectX::XMFLOAT3 rotatedOffsetF3;
	DirectX::XMStoreFloat3(&rotatedOffsetF3, rotatedOffset);

	// 親の位置に回転済みオフセットを加算
	DirectX::XMFLOAT3 worldFix;
	worldFix.x = trans.position.x + rotatedOffsetF3.x;
	worldFix.y = trans.position.y + rotatedOffsetF3.y;
	worldFix.z = trans.position.z + rotatedOffsetF3.z;

	return worldFix;
}

DirectX::XMFLOAT4X4 CameraComponent::GetViewMatrix(bool transpose) {
	DirectX::XMFLOAT4X4 Mat;
	DirectX::XMMATRIX View;

	DirectX::XMFLOAT3 worldPos = GetWorldPosition();
	DirectX::XMFLOAT3 worldFix = GetWorldFixation();

	DirectX::XMVECTOR Eye = DirectX::XMVectorSet(worldPos.x, worldPos.y, worldPos.z, 0.0f);
	DirectX::XMVECTOR At = DirectX::XMVectorSet(worldFix.x, worldFix.y, worldFix.z, 0.0f);
	DirectX::XMVECTOR Up = DirectX::XMVectorSet(_Up.x, _Up.y, _Up.z, 0.0f);

	View = DirectX::XMMatrixLookAtLH(Eye, At, Up);

	if (transpose) View = DirectX::XMMatrixTranspose(View);
	DirectX::XMStoreFloat4x4(&Mat, View);
	return Mat;
}

DirectX::XMFLOAT4X4 CameraComponent::GetProjectionMatrix(bool transpose) {
	DirectX::XMFLOAT4X4 Mat;
	DirectX::XMMATRIX Proj;

	Proj = DirectX::XMMatrixPerspectiveFovLH(_FOV, _AspectRatio, _NearPlane, _FarPlane);

	if (transpose) Proj = DirectX::XMMatrixTranspose(Proj);
	DirectX::XMStoreFloat4x4(&Mat, Proj);
	return Mat;
}

DirectX::XMMATRIX CameraComponent::GetView() {
	DirectX::XMFLOAT3 worldPos = GetWorldPosition();
	DirectX::XMFLOAT3 worldFix = GetWorldFixation();

	DirectX::XMVECTOR Eye = DirectX::XMVectorSet(worldPos.x, worldPos.y, worldPos.z, 0.0f);
	DirectX::XMVECTOR At = DirectX::XMVectorSet(worldFix.x, worldFix.y, worldFix.z, 0.0f);
	DirectX::XMVECTOR Up = DirectX::XMVectorSet(_Up.x, _Up.y, _Up.z, 0.0f);

	return DirectX::XMMatrixLookAtLH(Eye, At, Up);
}

DirectX::XMMATRIX CameraComponent::GetProjection() {
	DirectX::XMMATRIX Proj = DirectX::XMMatrixPerspectiveFovLH(_FOV, _AspectRatio, _NearPlane, _FarPlane);
	return Proj;
}

DirectX::XMFLOAT3 CameraComponent::GetForwardVector() {
	if (!_Parent) {
		DirectX::XMFLOAT3 forward;
		forward.x = cosf(_Rotation.y) * sinf(_Rotation.x);
		forward.y = sinf(_Rotation.y);
		forward.z = cosf(_Rotation.y) * cosf(_Rotation.x);

		float len = sqrtf(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
		if (len != 0.0f) {
			forward.x /= len;
			forward.y /= len;
			forward.z /= len;
		}
		return forward;
	}

	Transform trans = _Parent->GetWorldTransform();

	DirectX::XMFLOAT3 localForward;
	localForward.x = cosf(_Rotation.y) * sinf(_Rotation.x);
	localForward.y = sinf(_Rotation.y);
	localForward.z = cosf(_Rotation.y) * cosf(_Rotation.x);

	DirectX::XMMATRIX rotMat = DirectX::XMMatrixRotationRollPitchYaw(
		trans.rotation.x,
		trans.rotation.y,
		trans.rotation.z
	);

	DirectX::XMVECTOR localFwdVec = DirectX::XMLoadFloat3(&localForward);
	DirectX::XMVECTOR worldFwdVec = DirectX::XMVector3Transform(localFwdVec, rotMat);

	DirectX::XMFLOAT3 worldForward;
	DirectX::XMStoreFloat3(&worldForward, worldFwdVec);

	float len = sqrtf(worldForward.x * worldForward.x + worldForward.y * worldForward.y + worldForward.z * worldForward.z);
	if (len != 0.0f) {
		worldForward.x /= len;
		worldForward.y /= len;
		worldForward.z /= len;
	}

	return worldForward;
}

DirectX::XMFLOAT3 CameraComponent::GetRightVector() {
	DirectX::XMFLOAT3 up = _Up;
	DirectX::XMFLOAT3 forward = GetForwardVector();

	DirectX::XMFLOAT3 right;
	right.x = up.y * forward.z - up.z * forward.y;
	right.y = up.z * forward.x - up.x * forward.z;
	right.z = up.x * forward.y - up.y * forward.x;

	float len = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
	if (len != 0.0f) {
		right.x /= len;
		right.y /= len;
		right.z /= len;
	}

	return right;
}

DirectX::XMFLOAT3 CameraComponent::GetUpVector() {
	DirectX::XMFLOAT3 forward = GetForwardVector();
	DirectX::XMFLOAT3 right = GetRightVector();

	DirectX::XMFLOAT3 up;
	up.x = forward.y * right.z - forward.z * right.y;
	up.y = forward.z * right.x - forward.x * right.z;
	up.z = forward.x * right.y - forward.y * right.x;

	float len = sqrtf(up.x * up.x + up.y * up.y + up.z * up.z);
	if (len != 0.0f) {
		up.x /= len;
		up.y /= len;
		up.z /= len;
	}

	return up;
}