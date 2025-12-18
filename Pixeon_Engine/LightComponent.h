#pragma once
#include "Component.h"
#include <DirectXMath.h>

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
	void EditUpdate() override;      // 必要であれば回転から方向を更新
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

	// 計算補助
	DirectX::XMFLOAT3 GetWorldPosition() const;
	DirectX::XMFLOAT3 GetWorldDirection() const; // 前方（-Z or +Z）設計に合わせる

	// 保存 / 読込
	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

private:
	LightType           m_type = LightType::Directional;
	DirectX::XMFLOAT3   m_color{ 1,1,1 };
	float               m_intensity = 1.0f;
	float               m_range = 10.0f;         // Point/Spot
	float               m_spotInnerDeg = 20.0f;  // Spot
	float               m_spotOuterDeg = 35.0f;  // Spot (外側)
	bool                m_enabled = true;
};