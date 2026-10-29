#define NOMINMAX
#include "_Geometry.h"
#include "ShaderManager.h"
#include "System.h"
#include "SceneManger.h"
#include "Scene.h"
#include <DirectXMath.h>
#include <algorithm>

LineRenderer* LineRenderer::GetInstance() { static LineRenderer inst; return &inst; }

void LineRenderer::Initialize()
{
	RHI::Shader* vs = ShaderManager::GetInstance()->GetVertexShader("VS_Line");
	if (!vs) {
		MessageBoxA(nullptr, "VS_Line not found", "LineRenderer", MB_OK);
		return;
	}
	const RHI::InputElement layout[] = {
		{ "POSITION", 0, RHI::Format::R32G32B32_Float,    0, 0 },
		{ "COLOR",    0, RHI::Format::R32G32B32A32_Float, 0, 12 },
	};
	m_cls_InputLayout = GraphicsSystem::GetInstance()->GetDevice()->InputLayoutCreate(layout, 2, vs);
	if (!m_cls_InputLayout) {
		MessageBoxA(nullptr, "CreateInputLayout failed", "LineRenderer", MB_OK);
	}
}

void LineRenderer::Finalize()
{
	m_cls_InputLayout.reset();
	m_cls_VertexBuffer.reset();
	m_i_Capacity = 0;
}

/*
* 関数名　VertexBufferEnsure
* 引　数　vertexCount：必要な頂点数
* 戻り値　成功なら true
* 説　明　Dynamic 頂点バッファを使い回し、足りなければ倍々で拡張する
*         （以前は線 1 本ごとに頂点バッファを生成・破棄していた）
*/
bool LineRenderer::VertexBufferEnsure(uint32_t vertexCount)
{
	if (m_cls_VertexBuffer && vertexCount <= m_i_Capacity) return true;
	uint32_t capacity = std::max<uint32_t>(m_i_Capacity, 256);
	while (capacity < vertexCount) capacity *= 2;
	RHI::BufferDesc bd;
	bd.i_Size = capacity * sizeof(LineVertex);
	bd.i_BindFlags = RHI::Bind_VertexBuffer;
	bd.e_Usage = RHI::Usage::Dynamic;
	bd.i_Stride = sizeof(LineVertex);
	m_cls_VertexBuffer = GraphicsSystem::GetInstance()->GetDevice()->BufferCreate(bd);
	m_i_Capacity = m_cls_VertexBuffer ? capacity : 0;
	return m_cls_VertexBuffer != nullptr;
}

void LineRenderer::DrawLines(const LineVertex* vertices, uint32_t vertexCount,
	const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj)
{
	if (!vertices || vertexCount < 2 || !m_cls_InputLayout) return;
	ShaderManager* sm = ShaderManager::GetInstance();
	RHI::Shader* vs = sm->GetVertexShader("VS_Line");
	RHI::Shader* ps = sm->GetPixelShader("PS_Line");
	if (!vs || !ps || !VertexBufferEnsure(vertexCount)) return;

	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	dev->BufferUpdate(m_cls_VertexBuffer.get(), vertices, vertexCount * (uint32_t)sizeof(LineVertex));

	struct MatCB { DirectX::XMFLOAT4X4 world, view, proj; } cb{ world, view, proj };
	sm->SetCBufferRaw(ShaderStage::VS, "VS_Line", "CameraCB", &cb, sizeof(cb));
	sm->CommitAndBind(ShaderStage::VS, "VS_Line");

	dev->VertexBufferSet(0, m_cls_VertexBuffer.get(), sizeof(LineVertex), 0);
	dev->InputLayoutSet(m_cls_InputLayout.get());
	dev->TopologySet(RHI::Topology::LineList);
	dev->ShaderSet(RHI::ShaderStage::Vertex, vs);
	dev->ShaderSet(RHI::ShaderStage::Pixel, ps);
	dev->Draw(vertexCount, 0);
}

void LineRenderer::DrawLine(const DirectX::XMFLOAT3& s, const DirectX::XMFLOAT3& e, const DirectX::XMFLOAT4& color,
	const DirectX::XMFLOAT4X4& world,
	const DirectX::XMFLOAT4X4& view,
	const DirectX::XMFLOAT4X4& proj,
	float /*thickness*/)
{
	const LineVertex verts[2] = {
		{ { s.x, s.y, s.z }, { color.x, color.y, color.z, color.w } },
		{ { e.x, e.y, e.z }, { color.x, color.y, color.z, color.w } },
	};
	DrawLines(verts, 2, world, view, proj);
}

/*
* 関数名　Draw1mGrid
* 引　数　size：グリッドの一辺　view / proj：カメラ行列　cameraPosXZ：カメラ位置
* 戻り値　なし
* 説　明　カメラ周辺の 1m グリッドを描く。全線分を 1 回の描画にまとめる
*/
void Draw1mGrid(float size, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj, const DirectX::XMFLOAT3& cameraPosXZ)
{
	DirectX::XMFLOAT4X4 identity;
	DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());

	float half = size * 0.5f;
	int start = static_cast<int>(cameraPosXZ.x - half);
	int end_x = static_cast<int>(cameraPosXZ.x + half);
	int start_z = static_cast<int>(cameraPosXZ.z - half);
	int end_z = static_cast<int>(cameraPosXZ.z + half);
	float maxDistance = half * 1.5f;

	std::vector<LineRenderer::LineVertex> verts;
	verts.reserve((size_t)(end_x - start + 2) * (end_z - start_z + 2) * 4);
	auto segment = [&](float x0, float z0, float x1, float z1, bool axis) {
		const float midX = (x0 + x1) * 0.5f, midZ = (z0 + z1) * 0.5f;
		const float dist = std::sqrt((midX - cameraPosXZ.x) * (midX - cameraPosXZ.x) + (midZ - cameraPosXZ.z) * (midZ - cameraPosXZ.z));
		const float alpha = 1.0f - std::max(0.0f, std::min(dist / maxDistance, 1.0f));
		const float c = axis ? 1.0f : 0.5f;
		verts.push_back({ { x0, 0.0f, z0 }, { c, c, c, alpha } });
		verts.push_back({ { x1, 0.0f, z1 }, { c, c, c, alpha } });
	};

	for (int i = start; i <= end_x; ++i) {
		const float x = static_cast<float>(i);
		for (int j = start_z; j < end_z; ++j) segment(x, (float)j, x, (float)j + 1.0f, std::abs(x) < 0.01f);
	}
	for (int j = start_z; j <= end_z; ++j) {
		const float z = static_cast<float>(j);
		for (int i = start; i < end_x; ++i) segment((float)i, z, (float)i + 1.0f, z, std::abs(z) < 0.01f);
	}
	LineRenderer::GetInstance()->DrawLines(verts.data(), (uint32_t)verts.size(), identity, view, proj);
}
