#pragma once
#include "Component.h"

class CameraComponent : public Component
{
public:
	CameraComponent() {}
	~CameraComponent() {}

	void Init(Object* Prt)	override;
	void EditUpdate()		override;
	void InGameUpdate()		override;

	void DrawInspector() override;

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

	DirectX::XMFLOAT4X4 GetViewMatrix(bool transpose = true);
	DirectX::XMFLOAT4X4 GetProjectionMatrix(bool transpose = true);
	DirectX::XMMATRIX GetView();
	DirectX::XMMATRIX GetProjection();
	DirectX::XMFLOAT3 GetForwardVector();
	DirectX::XMFLOAT3 GetRightVector();

	DirectX::XMFLOAT3 GetPosition() const { return _Position; }
	DirectX::XMFLOAT3 GetRotation() const { return _Rotation; }
	void SetPosition(DirectX::XMFLOAT3 pos) { _Position = pos; }
	void SetRotation(DirectX::XMFLOAT3 rot) { _Rotation = rot; }
	DirectX::XMFLOAT3 GetFixation() const { return _Fixation; }
	void SetFixation(DirectX::XMFLOAT3 fixation) { _Fixation = fixation; }

	// Setter
	void SetFov(float fov) { _FOV = fov; }
	void SetAspect(float aspect) { _AspectRatio = aspect; }
	void SetNear(float nearPlane) { _NearPlane = nearPlane; }
	void SetFar(float farPlane) { _FarPlane = farPlane; }
	float GetNear() const { return _NearPlane; }
	float GetFar() const { return _FarPlane; }
	bool IsMove() const { return _IsKeyMove; }
	void SetIsMove(bool isMove) { _IsKeyMove = isMove; }
	bool IsChangeCalculation() const { return _IsChangeCalculation; }
	void SetIsChangeCalculation(bool isChange) { _IsChangeCalculation = isChange; }
	int GetCameraNumber() const { return _CameraNumber; }
	void SetCameraNumber(int num) { _CameraNumber = num; }
	DirectX::XMFLOAT3 GetUpVector() const { return _Up; }
private:
	Object* _Parent;
	DirectX::XMFLOAT3 _Position;
	DirectX::XMFLOAT3 _Rotation;
	DirectX::XMFLOAT3 _Fixation;
	DirectX::XMFLOAT3 _Up;
	float _FOV;
	float _AspectRatio;
	float _NearPlane;
	float _FarPlane;
	float _radius;
	bool _IsKeyMove = false;
	bool _IsChangeCalculation = false;
	int _CameraNumber = -1;
};
