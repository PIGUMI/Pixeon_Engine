#include <fstream>
#include <sstream>
#include <iostream>
#include "CameraComponent.h"
#include "Object.h"
#include "Scene.h"
#include "GUI.h"

void CameraComponent::Init(Object* Prt) {
	_Parent = Prt;
	_ComponentName = "CameraComponent";
	_Type = ComponentManager::COMPONENT_TYPE::CAMERA;
	_Position = { 0.0f, 5.0f, -10.0f };  // カメラを後方に配置
	_Fixation = { 0.0f, 0.0f, 0.0f };    // 原点を見る
	_Up = { 0.0f, 1.0f, 0.0f };
	_FOV = DirectX::XMConvertToRadians(60.0f);
	_AspectRatio = 16.0f / 9.0f;
	_NearPlane = 0.1f;  // 近クリップ面を小さく
	_FarPlane = 1000.0f;
	_radius = 10.0f;  // 半径を大きく
}

void CameraComponent::EditUpdate() {
	if (_IsKeyMove) {
		// カメラ操作
	}

	if (_IsChangeCalculation) {
		//　カメラの位置から注視点を計算
		_Fixation.x = _Position.x - cosf(_Rotation.y) * sinf(_Rotation.x) * _radius;
		_Fixation.y = _Position.y - sinf(_Rotation.y) * _radius;
		_Fixation.z = _Position.z - cosf(_Rotation.y) * cosf(_Rotation.x) * _radius;
	}
	else {
		//　注視点からカメラの位置を計算
		_Position.x = cosf(_Rotation.y) * sinf(_Rotation.x) * _radius + _Fixation.x;
		_Position.y = sinf(_Rotation.y) * _radius + _Fixation.y;
		_Position.z = cosf(_Rotation.y) * cosf(_Rotation.x) * _radius + _Fixation.z;
	}
	if (_Parent)
	{
		auto trans = _Parent->GetTransform();
		trans.position = _Fixation;
		_Parent->SetTransform(trans);
	}
}

void CameraComponent::InGameUpdate() {
	if (_IsKeyMove) {
		// カメラ操作
	}

	if (_IsChangeCalculation) {
		//　カメラの位置から注視点を計算
		_Fixation.x = _Position.x - cosf(_Rotation.y) * sinf(_Rotation.x) * _radius;
		_Fixation.y = _Position.y - sinf(_Rotation.y) * _radius;
		_Fixation.z = _Position.z - cosf(_Rotation.y) * cosf(_Rotation.x) * _radius;
	}
	else {
		//　注視点からカメラの位置を計算
		_Position.x = cosf(_Rotation.y) * sinf(_Rotation.x) * _radius + _Fixation.x;
		_Position.y = sinf(_Rotation.y) * _radius + _Fixation.y;
		_Position.z = cosf(_Rotation.y) * cosf(_Rotation.x) * _radius + _Fixation.z;
	}
	if (_Parent)
	{
		auto trans = _Parent->GetTransform();
		trans.position = _Position;
		_Parent->SetTransform(trans);
	}
}

void CameraComponent::DrawInspector() {
	std::string label = GUI::GetInstance()->ShiftJISToUTF8(_ComponentName);
	std::string Ptr = std::to_string((uintptr_t)this);
	label += "###" + Ptr;

	if (ImGui::CollapsingHeader(GUI::GetInstance()->ShiftJISToUTF8(label).c_str())) {
		if (ImGui::BeginTable("CameraTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("Position");
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat3("##Position", &_Position.x, 0.1f);

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
			ImGui::TableSetColumnIndex(0); ImGui::Text("Fixation");
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat3("##Fixation", &_Fixation.x, 0.1f);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("Up");
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat3("##Up", &_Up.x, 0.1f);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("FOV");
			ImGui::TableSetColumnIndex(1); ImGui::DragFloat("##FOV", &_FOV, 0.01f, DirectX::XMConvertToRadians(1.0f), DirectX::XMConvertToRadians(179.0f), "%.3f");

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
			ImGui::TableSetColumnIndex(0); ImGui::Text(GUI::GetInstance()->ShiftJISToUTF8("現在のカメラを切り替える").c_str());
			ImGui::TableSetColumnIndex(1);
			if (ImGui::Button("Set Main Camera")) {
				_Parent->GetParentScene()->SetMainCamera(this);
			}
			ImGui::EndTable();
		}
	}
}

void CameraComponent::SaveToFile(std::ostream& out) {
	out << _Position.x << " " << _Position.y << " " << _Position.z << " ";
	out << _Rotation.x << " " << _Rotation.y << " " << _Rotation.z << " ";
	out << _Fixation.x << " " << _Fixation.y << " " << _Fixation.z << " ";
	out << _Up.x << " " << _Up.y << " " << _Up.z << " ";
	out << _FOV << " " << _AspectRatio << " " << _NearPlane << " " << _FarPlane << " ";
	out << _radius << " ";
	out << _IsKeyMove << " ";
	out << _IsChangeCalculation << " ";
	out << _CameraNumber << " ";
}

void CameraComponent::LoadFromFile(std::istream& in) {
	in >> _Position.x >> _Position.y >> _Position.z;
	in >> _Rotation.x >> _Rotation.y >> _Rotation.z;
	in >> _Fixation.x >> _Fixation.y >> _Fixation.z;
	in >> _Up.x >> _Up.y >> _Up.z;
	in >> _FOV >> _AspectRatio >> _NearPlane >> _FarPlane;
	in >> _radius;
	in >> _IsKeyMove;
	in >> _IsChangeCalculation;
	in >> _CameraNumber;
}

DirectX::XMFLOAT4X4 CameraComponent::GetViewMatrix(bool transpose) {
	DirectX::XMFLOAT4X4 Mat;
	DirectX::XMMATRIX View;

	DirectX::XMVECTOR Eye = DirectX::XMVectorSet(_Position.x, _Position.y, _Position.z, 0.0f);
	DirectX::XMVECTOR At = DirectX::XMVectorSet(_Fixation.x, _Fixation.y, _Fixation.z, 0.0f);
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
	DirectX::XMVECTOR Eye = DirectX::XMVectorSet(_Position.x, _Position.y, _Position.z, 0.0f);
	DirectX::XMVECTOR At = DirectX::XMVectorSet(_Fixation.x, _Fixation.y, _Fixation.z, 0.0f);
	DirectX::XMVECTOR Up = DirectX::XMVectorSet(_Up.x, _Up.y, _Up.z, 0.0f);

	return DirectX::XMMatrixLookAtLH(Eye, At, Up);
}

DirectX::XMMATRIX CameraComponent::GetProjection() {
	DirectX::XMMATRIX Proj = DirectX::XMMatrixPerspectiveFovLH(_FOV, _AspectRatio, _NearPlane, _FarPlane);
	return Proj;
}

DirectX::XMFLOAT3 CameraComponent::GetForwardVector() {
	DirectX::XMFLOAT3 forward;
	forward.x = cosf(_Rotation.y) * sinf(_Rotation.x);
	forward.y = sinf(_Rotation.y);
	forward.z = cosf(_Rotation.y) * cosf(_Rotation.x);

	float len = sqrtf(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	if (len != 0.0f)
	{
		forward.x /= len;
		forward.y /= len;
		forward.z /= len;
	}
	return forward;
}

DirectX::XMFLOAT3 CameraComponent::GetRightVector() {
	// 上方向はワールドのY軸
	DirectX::XMFLOAT3 up = _Up;
	DirectX::XMFLOAT3 forward = GetForwardVector();

	// 外積 right = up × forward
	DirectX::XMFLOAT3 right;
	right.x = up.y * forward.z - up.z * forward.y;
	right.y = up.z * forward.x - up.x * forward.z;
	right.z = up.x * forward.y - up.y * forward.x;

	// 正規化
	float len = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
	if (len != 0.0f)
	{
		right.x /= len;
		right.y /= len;
		right.z /= len;
	}

	return right;
}