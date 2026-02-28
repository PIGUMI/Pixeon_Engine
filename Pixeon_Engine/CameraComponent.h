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
	void Draw(int Layer)	override;

	void DrawInspector() override;

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

	DirectX::XMFLOAT4X4 GetViewMatrix(bool transpose = true);
	DirectX::XMFLOAT4X4 GetProjectionMatrix(bool transpose = true);
	DirectX::XMMATRIX GetView();
	DirectX::XMMATRIX GetProjection();
	DirectX::XMFLOAT3 GetForwardVector();
	DirectX::XMFLOAT3 GetRightVector();
	DirectX::XMFLOAT3 GetUpVector();

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
	DirectX::XMFLOAT3 GetUpVectorValue() const { return _Up; }

	void SetDebugDraw(bool draw) { _debugDraw = draw; }
	bool IsDebugDraw() const { return _debugDraw; }

	// プロジェクション設定用の便利メソッド（追加）
	void SetProjectionValues(float fov, float aspect, float nearPlane, float farPlane) {
		_FOV = fov;
		_AspectRatio = aspect;
		_NearPlane = nearPlane;
		_FarPlane = farPlane;
	}

	// Transform設定用メソッド（追加）
	void SetTransform(const Transform& transform) {
		_PositionOffset = transform.position;
		_Rotation = transform.rotation;
	}

	// ビュー行列更新用メソッド（追加）
	void UpdateViewMatrix() {
		// 必要に応じて内部状態を更新
		// 現在の実装では特に何もする必要はない
	}

private:
	// デバッグ描画用メソッド
	void DrawCameraVisualization();
	void DrawFrustum();

	AbstractObject* _Parent = nullptr;  // 初期化を追加
	DirectX::XMFLOAT3 _PositionOffset;
	DirectX::XMFLOAT3 _Rotation = { 0.0f, 0.0f, 0.0f };  // 初期化を追加
	DirectX::XMFLOAT3 _FixationOffset;
	DirectX::XMFLOAT3 _Up;
	float _FOV;
	float _AspectRatio;
	float _NearPlane;
	float _FarPlane;
	float _radius;
	bool _IsKeyMove = false;
	bool _IsChangeCalculation = false;
	int _CameraNumber = -1;
	bool _debugDraw = true;
};