#pragma once
#include "Component.h"
#include<DirectXMath.h>

class LightComponent : public AbstractComponent
{
public:
	enum class LightType : int {
		Directional = 0,
		Point = 1,
		Spot = 2
	};

	LightComponent() {}
	~LightComponent() {}

	void Init(AbstractObject* Prt) override;
	void UInit() override;
	void EditUpdate() override;
	void Draw(int Layer) override;
	void DrawInspector() override;

	// ライトの基本パラメータ
	void SetType(LightType t) { m_type = t; }
	LightType GetType() const { return m_type; }

	void SetColor(const DirectX::XMFLOAT3& c) { m_color = c; }
	DirectX::XMFLOAT3 GetColor() const { return m_color; }

	void SetIntensity(float v) { m_intensity = v; }
	float GetIntensity() const { return m_intensity; }

	void SetRange(float r) { m_range = r; }
	float GetRange() const { return m_range; }

	void SetSpotInner(float deg) { m_spotInnerDeg = deg; }
	void SetSpotOuter(float deg) { m_spotOuterDeg = deg; }
	float GetSpotInner() const { return m_spotInnerDeg; }
	float GetSpotOuter() const { return m_spotOuterDeg; }

	void SetEnabled(bool e) { m_enabled = e; }
	bool IsEnabled() const { return m_enabled; }

	void SetDebugDraw(bool draw) { m_debugDraw = draw; }
	bool IsDebugDraw() const { return m_debugDraw; }

	void SetOffset(const DirectX::XMFLOAT3& offset) { m_offset = offset; }
	DirectX::XMFLOAT3 GetOffset() const { return m_offset; }

	// 計算補助
	DirectX::XMFLOAT3 GetWorldPosition() const;
	DirectX::XMFLOAT3 GetWorldDirection() const;

	// 保存 / 読込
	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

private:
	void DrawDirectionalLight();
	void DrawPointLight();
	void DrawSpotLight();

	LightType           m_type = LightType::Directional;
	DirectX::XMFLOAT3   m_color{ 1,1,1 };
	float               m_intensity = 1.0f;
	float               m_range = 10.0f;
	float               m_spotInnerDeg = 20.0f;
	float               m_spotOuterDeg = 35.0f;
	bool                m_enabled = true;
	bool                m_debugDraw = true;
	DirectX::XMFLOAT3   m_offset{ 0, 0, 0 };
};