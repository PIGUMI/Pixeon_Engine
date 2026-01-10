#pragma once
#include "Component.h"

class CameraComponent : public AbstractComponent
{
public:
	CameraComponent() {}
	~CameraComponent() {}

	void Init(AbstractObject* Prt)	override;
	void EditUpdate()		override;
	void InGameUpdate()		override;
	void Draw(int Layer)	override;  // 追加

	void DrawInspector() override;

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

	DirectX::XMFLOAT4X4 GetViewMatrix(bool transpose = true);
	DirectX::XMFLOAT4X4 GetProjectionMatrix(bool transpose = true);
	DirectX::XMMATRIX GetView();
	DirectX::XMMATRIX GetProjection();
	DirectX::XMFLOAT3 GetForwardVector();
	DirectX::XMFLOAT3 GetRightVector();
	DirectX::XMFLOAT3 GetUpVector();  // 修正：戻り値の型を変更

	// ワールド座標の取得
	DirectX::XMFLOAT3 GetWorldPosition() const;
	DirectX::XMFLOAT3 GetWorldFixation() const;

	// オフセット値の取得・設定
	DirectX::XMFLOAT3 GetPosition() const { return _PositionOffset; }
	DirectX::XMFLOAT3 GetRotation() const { return _Rotation; }
	void SetPosition(DirectX::XMFLOAT3 pos) { _PositionOffset = pos; }
	void SetRotation(DirectX::XMFLOAT3 rot) { _Rotation = rot; }
	DirectX::XMFLOAT3 GetFixation() const { return _FixationOffset; }
	void SetFixation(DirectX::XMFLOAT3 fixation) { _FixationOffset = fixation; }

	// Setter
	void SetFov(float fov) { _FOV = fov; }
	void SetAspect(float aspect) { _AspectRatio = aspect; }
	void SetNear(float nearPlane) { _NearPlane = nearPlane; }
	void SetFar(float farPlane) { _FarPlane = farPlane; }
	float GetFov() const { return _FOV; }
	float GetAspect() const { return _AspectRatio; }
	float GetNear() const { return _NearPlane; }
	float GetFar() const { return _FarPlane; }
	bool IsMove() const { return _IsKeyMove; }
	void SetIsMove(bool isMove) { _IsKeyMove = isMove; }
	bool IsChangeCalculation() const { return _IsChangeCalculation; }
	void SetIsChangeCalculation(bool isChange) { _IsChangeCalculation = isChange; }
	int GetCameraNumber() const { return _CameraNumber; }
	void SetCameraNumber(int num) { _CameraNumber = num; }
	DirectX::XMFLOAT3 GetUpVectorValue() const { return _Up; }  // 名前変更

	void SetDebugDraw(bool draw) { _debugDraw = draw; }  // 追加
	bool IsDebugDraw() const { return _debugDraw; }      // 追加

private:
	// デバッグ描画用メソッド
	void DrawCameraVisualization();  // 追加
	void DrawFrustum();              // 追加

	AbstractObject* _Parent;
	DirectX::XMFLOAT3 _PositionOffset;      // カメラ位置のオフセット
	DirectX::XMFLOAT3 _Rotation;
	DirectX::XMFLOAT3 _FixationOffset;      // 注視点のオフセット
	DirectX::XMFLOAT3 _Up;
	float _FOV;
	float _AspectRatio;
	float _NearPlane;
	float _FarPlane;
	float _radius;
	bool _IsKeyMove = false;
	bool _IsChangeCalculation = false;
	int _CameraNumber = -1;
	bool _debugDraw = true;  // 追加
};