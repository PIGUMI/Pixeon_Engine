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
	auto device = DirectX11::GetInstance()->GetDevice();

	const void* vsBytecode = nullptr; size_t vsByteSize = 0;
	if (!ShaderManager::GetInstance()->GetVSBytecode("VS_Line", &vsBytecode, &vsByteSize)) {
		MessageBoxA(nullptr, "GetVSBytecode(VS_Line) failed", "LineRenderer", MB_OK);
		return;
	}

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,     0, 0 , D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	HRESULT hr = device->CreateInputLayout(layout, 2, vsBytecode, vsByteSize, &m_inputLayout);
	if (FAILED(hr)) {
		MessageBoxA(nullptr, "CreateInputLayout failed", "LineRenderer", MB_OK);
	}
}

void LineRenderer::Finalize()
{
	SAFE_RELEASE(m_inputLayout);
}

void LineRenderer::DrawLine(const DirectX::XMFLOAT3& s, const DirectX::XMFLOAT3& e, const DirectX::XMFLOAT4& color,
	const DirectX::XMFLOAT4X4& world,
	const DirectX::XMFLOAT4X4& view,
	const DirectX::XMFLOAT4X4& proj,
	float /*thickness*/)
{
	struct LineVertex { float pos[3]; float color[4]; };

	LineVertex verts[2];
	verts[0] = { { s.x, s.y, s.z }, { color.x, color.y, color.z, color.w } };
	verts[1] = { { e.x, e.y, e.z }, { color.x, color.y, color.z, color.w } };

	auto device = DirectX11::GetInstance()->GetDevice();
	auto context = DirectX11::GetInstance()->GetContext();

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	vbDesc.ByteWidth = sizeof(verts);
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vdat = {};
	vdat.pSysMem = verts;
	ID3D11Buffer* vb = nullptr;
	if (FAILED(device->CreateBuffer(&vbDesc, &vdat, &vb))) return;

	struct MatCB { DirectX::XMFLOAT4X4 world, view, proj; } cb;
	cb.world = world;
	cb.view = view;
	cb.proj = proj;

	ShaderManager* sm = ShaderManager::GetInstance();

	sm->SetCBufferRaw(ShaderStage::VS, "VS_Line", "CameraCB", &cb, sizeof(cb));
	sm->CommitAndBind(ShaderStage::VS, "VS_Line");

	auto vs = sm->GetVertexShader("VS_Line");
	auto ps = sm->GetPixelShader("PS_Line");
	if (!vs || !ps) {
		MessageBoxA(nullptr, "VS_Line/PS_Line not found", "LineRenderer", MB_OK);
		SAFE_RELEASE(vb);
		return;
	}

	UINT stride = sizeof(LineVertex), offset = 0;
	context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
	context->IASetInputLayout(m_inputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	context->VSSetShader(vs, nullptr, 0);
	context->PSSetShader(ps, nullptr, 0);

	context->Draw(2, 0);

	SAFE_RELEASE(vb);
}

void Draw1mGrid(float size, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj, const DirectX::XMFLOAT3& cameraPosXZ)
{
	auto lr = LineRenderer::GetInstance();
	DirectX::XMFLOAT4X4 identity;
	DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());

	float half = size * 0.5f;
	int start = static_cast<int>(cameraPosXZ.x - half);
	int end_x = static_cast<int>(cameraPosXZ.x + half);
	int start_z = static_cast<int>(cameraPosXZ.z - half);
	int end_z = static_cast<int>(cameraPosXZ.z + half);

	float maxDistance = half * 1.5f;

	for (int i = start; i <= end_x; ++i) {
		float x = static_cast<float>(i);
		for (int j = start_z; j < end_z; ++j) {
			float z0 = static_cast<float>(j), z1 = z0 + 1.0f;

			float midX = x, midZ = (z0 + z1) * 0.5f;
			float dist = std::sqrt((midX - cameraPosXZ.x) * (midX - cameraPosXZ.x) + (midZ - cameraPosXZ.z) * (midZ - cameraPosXZ.z));
			float alpha = 1.0f - std::max(0.0f, std::min(dist / maxDistance, 1.0f));

			DirectX::XMFLOAT4 color = (std::abs(x) < 0.01f) ? DirectX::XMFLOAT4(1, 1, 1, alpha) : DirectX::XMFLOAT4(0.5f, 0.5f, 0.5f, alpha);

			lr->DrawLine(
				DirectX::XMFLOAT3(x, 0, z0),
				DirectX::XMFLOAT3(x, 0, z1),
				color, identity, view, proj, 1.0f
			);
		}
	}

	for (int j = start_z; j <= end_z; ++j) {
		float z = static_cast<float>(j);
		for (int i = start; i < end_x; ++i) {
			float x0 = static_cast<float>(i), x1 = x0 + 1.0f;
			float midX = (x0 + x1) * 0.5f, midZ = z;
			float dist = std::sqrt((midX - cameraPosXZ.x) * (midX - cameraPosXZ.x) + (midZ - cameraPosXZ.z) * (midZ - cameraPosXZ.z));
			float alpha = 1.0f - std::max(0.0f, std::min(dist / maxDistance, 1.0f));
			DirectX::XMFLOAT4 color = (std::abs(z) < 0.01f) ? DirectX::XMFLOAT4(1, 1, 1, alpha) : DirectX::XMFLOAT4(0.5f, 0.5f, 0.5f, alpha);

			lr->DrawLine(
				DirectX::XMFLOAT3(x0, 0, z),
				DirectX::XMFLOAT3(x1, 0, z),
				color, identity, view, proj, 1.0f
			);
		}
	}
}