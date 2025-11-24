#pragma once
#include "Component.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include <vector>

class Geometry : public Component
{
public:
	Geometry();
	~Geometry();

	void Init(Object* Prt) override;
	void DrawInspector() override;
	void Draw() override;

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

	float m_gridSize = 1.0f;
	int   m_gridCount = 10;
	DirectX::XMFLOAT4 m_gridColor = { 0.3f, 0.8f, 0.3f, 1.0f };
	DirectX::XMFLOAT3 m_cubeCenter = { 0, 0.5f, 0 };
	float m_cubeSize = 1.0f;
	DirectX::XMFLOAT4 m_cubeColor = { 1, 1, 0, 1 };

	std::string m_selectedVS;
	std::string m_selectedPS;
	std::string _selectNowVS;
	std::string _selectNowPS;

	ID3D11VertexShader* m_vsShader = nullptr;
	ID3D11PixelShader* m_psShader = nullptr;

private:
	struct LineVertex {
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
	};
	struct CameraCB {
		DirectX::XMFLOAT4X4 world;
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 proj;
	};

	std::vector<LineVertex> m_lineVertices;
	ID3D11Buffer* m_vertexBuffer = nullptr;
	ID3D11InputLayout* m_inputLayout = nullptr;
	bool m_needsUpdate = true;

	void UpdateGridVertices();
	void UpdateCubeVertices();
	void UpdateVertexBuffer();

	bool RebuildInputLayoutForCurrentVS();
};