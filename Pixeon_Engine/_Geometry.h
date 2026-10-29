#pragma once
#include <DirectXMath.h>
#include <vector>
#include "RHI/RHI.h"

class LineRenderer
{
public:
	struct LineVertex { float pos[3]; float color[4]; };

	static LineRenderer* GetInstance();
	void Initialize();
	void Finalize();

	void DrawLine(const DirectX::XMFLOAT3& s, const DirectX::XMFLOAT3& e, const DirectX::XMFLOAT4& color,
		const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj,
		float thickness
	);

	// 複数の線分 (頂点 2 個で 1 本) をまとめて 1 回の描画で描く
	void DrawLines(const LineVertex* vertices, uint32_t vertexCount,
		const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj);

private:
	bool VertexBufferEnsure(uint32_t vertexCount);

	RHI::InputLayoutRef m_cls_InputLayout;
	RHI::BufferRef m_cls_VertexBuffer;
	uint32_t m_i_Capacity = 0;
};

void Draw1mGrid(float size, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj, const DirectX::XMFLOAT3& cameraPosXZ);
