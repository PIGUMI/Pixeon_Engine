#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
class LineRenderer
{
public:
	static LineRenderer* GetInstance();
	void Initialize();
	void Finalize();

	void DrawLine(const DirectX::XMFLOAT3& s, const DirectX::XMFLOAT3& e, const DirectX::XMFLOAT4& color,
		const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj,
		float thickness
	);

private:
	ID3D11InputLayout* m_inputLayout = nullptr;
	ID3D11Buffer* m_matrixCB = nullptr;
};


void Draw1mGrid(float size, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj, const DirectX::XMFLOAT3& cameraPosXZ);