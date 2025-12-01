#include "_Geometry.h"
#include "ShaderManager.h"
#include "System.h"
#include "SceneManger.h"
#include "Scene.h"
#include <DirectXMath.h>

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

    // CameraCB 用の CB は ShaderManager が ReflectShader で作成済み
    // →ここで独自の m_matrixCB はもう使わないので生成不要
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

    // 頂点バッファ
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = sizeof(verts);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vdat = {};
    vdat.pSysMem = verts;
    ID3D11Buffer* vb = nullptr;
    if (FAILED(device->CreateBuffer(&vbDesc, &vdat, &vb))) return;

    // CameraCB へ書き込み（ShaderManager 方式）
    struct MatCB { DirectX::XMFLOAT4X4 world, view, proj; } cb;
    cb.world = world;
    cb.view = view;
    cb.proj = proj;

    ShaderManager* sm = ShaderManager::GetInstance();
    // CameraCB という cbuffer 名を想定
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