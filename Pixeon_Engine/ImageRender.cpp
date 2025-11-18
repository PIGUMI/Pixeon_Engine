#define NOMINMAX
#include "ImageRender.h"

using namespace DirectX;

// static
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ImageRender::s_whiteTexSRV;
Microsoft::WRL::ComPtr<ID3D11SamplerState>       ImageRender::s_linearSmp;

ImageRender::ImageRender() {
    _ComponentName = "ImageRender";
    _Type = ComponentManager::COMPONENT_TYPE::IMAGE; // 必要に応じてComponentManagerへ定義追加
}

ImageRender::~ImageRender() {
    UInit();
}

void ImageRender::UInit() {
    m_layout.Reset();
    m_vs.Reset();
    m_ps.Reset();
    m_cbVS.Reset();
    m_vb.Reset();
    m_ib.Reset();
}

void ImageRender::SaveToFile(std::ostream& out){
    out << m_textureName << std::endl;
    out << (int)m_mode << std::endl;
    out << m_size2D.x << " " << m_size2D.y << std::endl;
    out << m_sizeWorld.x << " " << m_sizeWorld.y << std::endl;
    out << m_offset2D.x << " " << m_offset2D.y << std::endl;
    out << m_offset3D.x << " " << m_offset3D.y << " " << m_offset3D.z << std::endl;
    out << m_uvRect.x << " " << m_uvRect.y << " " << m_uvRect.z << " " << m_uvRect.w << std::endl;
	out << m_color.x << " " << m_color.y << " " << m_color.z << " " << m_color.w << std::endl;
}

void ImageRender::LoadFromFile(std::istream& in){
    int mode = 0;
    in >> m_textureName;
    in >> mode; m_mode = (PlacementMode)mode;
    in >> m_size2D.x >> m_size2D.y;
    in >> m_sizeWorld.x >> m_sizeWorld.y;
    in >> m_offset2D.x >> m_offset2D.y;
    in >> m_offset3D.x >> m_offset3D.y >> m_offset3D.z;
    in >> m_uvRect.x >> m_uvRect.y >> m_uvRect.z >> m_uvRect.w;
    in >> m_color.x >> m_color.y >> m_color.z >> m_color.w;
    if (!m_textureName.empty()) {
        m_texture = ResourceService::Instance().GetTexture(m_textureName);
	}
}

void ImageRender::Init(Object* owner) {
    _Parent = owner;
    _ComponentName = "ImageRender";
    _Type = ComponentManager::COMPONENT_TYPE::IMAGE;

    if (!m_textureName.empty()) {
        m_texture = ResourceService::Instance().GetTexture(m_textureName);
    }
    EnsureFallbackTextures();
    EnsureShaders(true);
    EnsureConstantBuffer();
    EnsureBuffers();
    m_ready = true;
}

void ImageRender::SetTextureName(const std::string& name) {
    m_textureName = name;
    m_texture = ResourceService::Instance().GetTexture(m_textureName);
}

bool ImageRender::EnsureFallbackTextures() {
    auto dev = DirectX11::GetInstance()->GetDevice();
    if (!dev) return false;
    if (!s_whiteTexSRV) {
        uint32_t pix = 0xFFFFFFFF;
        D3D11_TEXTURE2D_DESC td{};
        td.Width = td.Height = 1;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init{ &pix, 4, 4 };
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        if (SUCCEEDED(dev->CreateTexture2D(&td, &init, tex.GetAddressOf()))) {
            dev->CreateShaderResourceView(tex.Get(), nullptr, s_whiteTexSRV.GetAddressOf());
        }
    }
    if (!s_linearSmp) {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sd.MinLOD = 0; sd.MaxLOD = D3D11_FLOAT32_MAX;
        dev->CreateSamplerState(&sd, s_linearSmp.GetAddressOf());
    }
    return s_whiteTexSRV != nullptr;
}

bool ImageRender::EnsureShaders(bool forceRecreateLayout) {
    auto* sm = ShaderManager::GetInstance();
    ID3D11VertexShader* vs = sm->GetVertexShader(m_vsName);
    ID3D11PixelShader* ps = sm->GetPixelShader(m_psName);
    if (!vs || !ps) {
        sm->UpdateAndCompileShaders();
        vs = sm->GetVertexShader(m_vsName);
        ps = sm->GetPixelShader(m_psName);
        if (!vs || !ps) {
            OutputDebugStringA(("[ImageRender] Shader missing: " + m_vsName + ", " + m_psName + "\n").c_str());
            return false;
        }
    }
    bool vsChanged = (m_vs.Get() != vs);
    bool needLayout = forceRecreateLayout || vsChanged || !m_layout;

    m_vs = vs;
    m_ps = ps;

    if (needLayout) RecreateInputLayout();
    return true;
}

void ImageRender::RecreateInputLayout() {
    m_layout.Reset();
    const void* bc = nullptr; size_t bcSize = 0;
    if (!ShaderManager::GetInstance()->GetVSBytecode(m_vsName, &bc, &bcSize)) {
        OutputDebugStringA("[ImageRender] GetVSBytecode failed\n");
        return;
    }
    EnsureInputLayout(bc, bcSize);
}

bool ImageRender::EnsureInputLayout(const void* vsBytecode, size_t size) {
    if (m_layout) return true;
    D3D11_INPUT_ELEMENT_DESC desc[] = {
        { "POSITION",0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex,pos), D3D11_INPUT_PER_VERTEX_DATA,0 },
        { "TEXCOORD",0, DXGI_FORMAT_R32G32_FLOAT,    0, (UINT)offsetof(Vertex,uv),  D3D11_INPUT_PER_VERTEX_DATA,0 },
    };
    auto dev = DirectX11::GetInstance()->GetDevice();
    HRESULT hr = dev->CreateInputLayout(desc, _countof(desc), vsBytecode, size, m_layout.GetAddressOf());
    if (FAILED(hr)) {
        OutputDebugStringA("[ImageRender] CreateInputLayout failed\n");
        return false;
    }
    return true;
}

bool ImageRender::EnsureConstantBuffer() {
    if (m_cbVS) return true;
    D3D11_BUFFER_DESC bd{};
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = sizeof(CBVS);
    bd.Usage = D3D11_USAGE_DEFAULT;
    auto dev = DirectX11::GetInstance()->GetDevice();
    HRESULT hr = dev->CreateBuffer(&bd, nullptr, m_cbVS.GetAddressOf());
    if (FAILED(hr)) {
        OutputDebugStringA("[ImageRender] Create CB failed\n");
        return false;
    }
    return true;
}

bool ImageRender::EnsureBuffers() {
    auto dev = DirectX11::GetInstance()->GetDevice();
    if (!m_vb) {
        D3D11_BUFFER_DESC bd{};
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.ByteWidth = sizeof(Vertex) * 4;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        HRESULT hr = dev->CreateBuffer(&bd, nullptr, m_vb.GetAddressOf());
        if (FAILED(hr)) return false;
    }
    if (!m_ib) {
        uint16_t idx[6] = { 0,1,2, 0,2,3 };
        D3D11_BUFFER_DESC bd{};
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bd.ByteWidth = sizeof(idx);
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        D3D11_SUBRESOURCE_DATA init{ idx,0,0 };
        HRESULT hr = dev->CreateBuffer(&bd, &init, m_ib.GetAddressOf());
        if (FAILED(hr)) return false;
    }
    return true;
}

void ImageRender::UpdateVB(const Vertex v[4]) {
    auto ctx = DirectX11::GetInstance()->GetContext();
    D3D11_MAPPED_SUBRESOURCE mp{};
    if (SUCCEEDED(ctx->Map(m_vb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp))) {
        memcpy(mp.pData, v, sizeof(Vertex) * 4);
        ctx->Unmap(m_vb.Get(), 0);
    }
}

void ImageRender::UpdateVertices2D(Vertex outV[4], float& outZClip) {
    // 親のワールド位置をスクリーンへ投影し、ピクセル単位で矩形を組む
    Scene* scene = _Parent ? _Parent->GetParentScene() : nullptr;
    CameraComponent* cam = scene ? scene->GetMainCamera() : nullptr;
    if (!cam) {
        // カメラが無い場合は画面中央基準
        float W = (float)DirectX11::GetInstance()->GetDefaultRTV()->GetWidth();
        float H = (float)DirectX11::GetInstance()->GetDefaultRTV()->GetHeight();
        float cx = W * 0.5f + m_offset2D.x;
        float cy = H * 0.5f + m_offset2D.y;
        float hw = m_size2D.x * 0.5f;
        float hh = m_size2D.y * 0.5f;
        auto toNDC = [&](float x, float y)->XMFLOAT2 {
            float ndcX = (x / W) * 2.0f - 1.0f;
            float ndcY = 1.0f - (y / H) * 2.0f;
            return { ndcX, ndcY };
            };
        XMFLOAT2 tl = toNDC(cx - hw, cy - hh);
        XMFLOAT2 tr = toNDC(cx + hw, cy - hh);
        XMFLOAT2 br = toNDC(cx + hw, cy + hh);
        XMFLOAT2 bl = toNDC(cx - hw, cy + hh);
        outZClip = 0.0f;
        outV[0] = { {tl.x, tl.y, outZClip}, {m_uvRect.x, m_uvRect.y} };
        outV[1] = { {tr.x, tr.y, outZClip}, {m_uvRect.z, m_uvRect.y} };
        outV[2] = { {br.x, br.y, outZClip}, {m_uvRect.z, m_uvRect.w} };
        outV[3] = { {bl.x, bl.y, outZClip}, {m_uvRect.x, m_uvRect.w} };
        return;
    }

    XMMATRIX V = cam->GetView();
    XMMATRIX P = cam->GetProjection();

    Transform t = _Parent->GetTransform();
    XMVECTOR posW = XMVectorSet(t.position.x, t.position.y, t.position.z, 1.0f);

    XMVECTOR clip = XMVector4Transform(XMVector4Transform(posW, V), P);
    XMFLOAT4 clipF; XMStoreFloat4(&clipF, clip);
    float W = (float)DirectX11::GetInstance()->GetDefaultRTV()->GetWidth();
    float H = (float)DirectX11::GetInstance()->GetDefaultRTV()->GetHeight();

    float ndcX = clipF.x / clipF.w;
    float ndcY = clipF.y / clipF.w;
    outZClip = clipF.z / clipF.w;

    float px = (ndcX * 0.5f + 0.5f) * W;
    float py = (-(ndcY) * 0.5f + 0.5f) * H;

    px += m_offset2D.x;
    py += m_offset2D.y;

    float hw = m_size2D.x * 0.5f;
    float hh = m_size2D.y * 0.5f;

    auto toNDC = [&](float x, float y)->XMFLOAT2 {
        float ndcX2 = (x / W) * 2.0f - 1.0f;
        float ndcY2 = 1.0f - (y / H) * 2.0f;
        return { ndcX2, ndcY2 };
        };

    XMFLOAT2 tl = toNDC(px - hw, py - hh);
    XMFLOAT2 tr = toNDC(px + hw, py - hh);
    XMFLOAT2 br = toNDC(px + hw, py + hh);
    XMFLOAT2 bl = toNDC(px - hw, py + hh);

    outV[0] = { {tl.x, tl.y, outZClip}, {m_uvRect.x, m_uvRect.y} };
    outV[1] = { {tr.x, tr.y, outZClip}, {m_uvRect.z, m_uvRect.y} };
    outV[2] = { {br.x, br.y, outZClip}, {m_uvRect.z, m_uvRect.w} };
    outV[3] = { {bl.x, bl.y, outZClip}, {m_uvRect.x, m_uvRect.w} };
}

void ImageRender::UpdateVerticesBillboard(Vertex outV[4]) {
    Scene* scene = _Parent ? _Parent->GetParentScene() : nullptr;
    CameraComponent* cam = scene ? scene->GetMainCamera() : nullptr;
    if (!cam) {
        float dummyZ = 0.0f;
        UpdateVertices2D(outV, dummyZ);
        return;
    }
    XMMATRIX V = cam->GetView();
    XMMATRIX invV = XMMatrixInverse(nullptr, V);

    // カメラの Right/Up ベクトル
    XMFLOAT3 right, up;
    right = XMFLOAT3(invV.r[0].m128_f32[0], invV.r[0].m128_f32[1], invV.r[0].m128_f32[2]);
    up = XMFLOAT3(invV.r[1].m128_f32[0], invV.r[1].m128_f32[1], invV.r[1].m128_f32[2]);

    XMVECTOR vRight = XMVector3Normalize(XMLoadFloat3(&right));
    XMVECTOR vUp = XMVector3Normalize(XMLoadFloat3(&up));

    Transform t = _Parent->GetTransform();
    // オフセットは親の回転を適用
    XMMATRIX R = XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z);
    XMVECTOR off = XMVector3Transform(XMLoadFloat3(&m_offset3D), R);
    XMVECTOR center = XMVectorAdd(XMLoadFloat3(&t.position), off);

    float hw = m_sizeWorld.x * 0.5f;
    float hh = m_sizeWorld.y * 0.5f;

    XMVECTOR tl = center - vRight * hw + vUp * hh;
    XMVECTOR tr = center + vRight * hw + vUp * hh;
    XMVECTOR br = center + vRight * hw - vUp * hh;
    XMVECTOR bl = center - vRight * hw - vUp * hh;

    XMFLOAT3 f;
    XMStoreFloat3(&f, tl); outV[0].pos = f; outV[0].uv = { m_uvRect.x, m_uvRect.y };
    XMStoreFloat3(&f, tr); outV[1].pos = f; outV[1].uv = { m_uvRect.z, m_uvRect.y };
    XMStoreFloat3(&f, br); outV[2].pos = f; outV[2].uv = { m_uvRect.z, m_uvRect.w };
    XMStoreFloat3(&f, bl); outV[3].pos = f; outV[3].uv = { m_uvRect.x, m_uvRect.w };
}

void ImageRender::UpdateVerticesWorld3D(Vertex outV[4]) {
    Transform t = _Parent->GetTransform();
    XMMATRIX R = XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z);
    XMVECTOR vRight = XMVector3Normalize(R.r[0]);
    XMVECTOR vUp = XMVector3Normalize(R.r[1]);

    // オフセットは親の回転を適用（ローカル→ワールド）
    XMVECTOR off = XMVector3Transform(XMLoadFloat3(&m_offset3D), R);
    XMVECTOR center = XMVectorAdd(XMLoadFloat3(&t.position), off);

    float hw = m_sizeWorld.x * 0.5f;
    float hh = m_sizeWorld.y * 0.5f;

    XMVECTOR tl = center - vRight * hw + vUp * hh;
    XMVECTOR tr = center + vRight * hw + vUp * hh;
    XMVECTOR br = center + vRight * hw - vUp * hh;
    XMVECTOR bl = center - vRight * hw - vUp * hh;

    XMFLOAT3 f;
    XMStoreFloat3(&f, tl); outV[0].pos = f; outV[0].uv = { m_uvRect.x, m_uvRect.y };
    XMStoreFloat3(&f, tr); outV[1].pos = f; outV[1].uv = { m_uvRect.z, m_uvRect.y };
    XMStoreFloat3(&f, br); outV[2].pos = f; outV[2].uv = { m_uvRect.z, m_uvRect.w };
    XMStoreFloat3(&f, bl); outV[3].pos = f; outV[3].uv = { m_uvRect.x, m_uvRect.w };
}

void ImageRender::Draw() {
    if (!m_ready) return;

    auto* dx = DirectX11::GetInstance();
    auto ctx = dx->GetContext();
    if (!ctx) return;

    if (!EnsureShaders(false)) return;
    if (!EnsureConstantBuffer()) return;
    if (!EnsureBuffers()) return;

    // 頂点生成
    Vertex v[4]{};
    int mode2DFlag = 0;
    float zClip2D = 0.0f;

    if (m_mode == PlacementMode::Screen2D) {
        UpdateVertices2D(v, zClip2D);
        mode2DFlag = 1;
    }
    else if (m_mode == PlacementMode::Billboard) {
        UpdateVerticesBillboard(v);
        mode2DFlag = 0;
    }
    else { // World3D
        UpdateVerticesWorld3D(v);
        mode2DFlag = 0;
    }

    UpdateVB(v);

    // 定数バッファ更新（Sceneからカメラ取得）
    Scene* scene = _Parent ? _Parent->GetParentScene() : nullptr;
    CameraComponent* cam = scene ? scene->GetMainCamera() : nullptr;

    CBVS cb{};
    if (cam) {
        cb.View = XMMatrixTranspose(cam->GetView());
        cb.Proj = XMMatrixTranspose(cam->GetProjection());
    }
    else {
        cb.View = XMMatrixIdentity();
        cb.Proj = XMMatrixIdentity();
    }
    cb.Color = m_color;
    cb.mode2D = mode2DFlag;

    ctx->UpdateSubresource(m_cbVS.Get(), 0, nullptr, &cb, 0, 0);

    // SRV
    ID3D11ShaderResourceView* srv = (m_texture && m_texture->srv) ? m_texture->srv.Get() : s_whiteTexSRV.Get();

    // バインド
    UINT stride = sizeof(Vertex), offset = 0;
    ID3D11Buffer* vb = m_vb.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(m_ib.Get(), DXGI_FORMAT_R16_UINT, 0);
    ctx->IASetInputLayout(m_layout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);
    ID3D11Buffer* cbs[] = { m_cbVS.Get() };
    ctx->VSSetConstantBuffers(0, 1, cbs);
    ctx->PSSetConstantBuffers(0, 1, cbs);

    ID3D11ShaderResourceView* srvs[] = { srv };
    ctx->PSSetShaderResources(0, 1, srvs);

    ID3D11SamplerState* smp = s_linearSmp.Get();
    ctx->PSSetSamplers(0, 1, &smp);

    // 透明合成が必要な場合は外部で BLEND_ALPHA に設定してください
    ctx->DrawIndexed(6, 0, 0);
}

void ImageRender::DrawInspector() {
    auto SJ = [](const char* s)->std::string { return EditrGUI::GetInstance()->ShiftJISToUTF8(s); };
    if (!ImGui::CollapsingHeader(SJ("ImageRender").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // テクスチャ
    ImGui::Text("%s", SJ("テクスチャ:").c_str());
    ImGui::SameLine();
    ImGui::Text("%s", m_textureName.empty() ? "(none)" : m_textureName.c_str());
    ImGui::SameLine();
    if (ImGui::Button(SJ("選択...").c_str())) {
        ImGui::OpenPopup("ImgTexSelectPopup");
    }
    if (ImGui::BeginPopupModal("ImgTexSelectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char filter[128] = "";
        ImGui::InputText(SJ("フィルタ").c_str(), filter, sizeof(filter));
        auto texList = AssetManager::Instance()->GetCachedTextureNames();
        ImGui::BeginChild("ImgTexList", ImVec2(420, 260), true);
        for (int i = 0; i < (int)texList.size(); ++i) {
            const std::string& n = texList[i];
            if (filter[0] && n.find(filter) == std::string::npos) continue;
            if (ImGui::Selectable(n.c_str(), false)) {
                SetTextureName(n);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndChild();
        if (ImGui::Button(SJ("閉じる").c_str())) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (m_texture) {
        ImGui::Text("(%d x %d)", (int)m_texture->width, (int)m_texture->height);
    }

    // 配置モード（日本語ラベルはSJで個別変換）
    std::string modeLabels[3] = { SJ("2D配置"), SJ("ビルボード"), SJ("3D配置") };
    int modeIdx = (int)m_mode;
    if (ImGui::BeginCombo(SJ("配置モード").c_str(), modeLabels[modeIdx].c_str())) {
        for (int i = 0; i < 3; ++i) {
            bool sel = (i == modeIdx);
            if (ImGui::Selectable(modeLabels[i].c_str(), sel)) {
                m_mode = (PlacementMode)i;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // オフセット・サイズ
    if (m_mode == PlacementMode::Screen2D) {
        ImGui::InputFloat2(SJ("2Dオフセット(px)").c_str(), (float*)&m_offset2D);
        ImGui::InputFloat2(SJ("サイズ(px)").c_str(), (float*)&m_size2D);
    }
    else {
        ImGui::InputFloat3(SJ("3Dオフセット").c_str(), (float*)&m_offset3D);
        ImGui::InputFloat2(SJ("サイズ(ワールド)").c_str(), (float*)&m_sizeWorld);
    }

    // UV
    ImGui::InputFloat4("UV(u0,v0,u1,v1)", (float*)&m_uvRect);
    m_uvRect.x = Clamp(m_uvRect.x, 0.0f, 1.0f);
    m_uvRect.y = Clamp(m_uvRect.y, 0.0f, 1.0f);
    m_uvRect.z = Clamp(m_uvRect.z, 0.0f, 1.0f);
    m_uvRect.w = Clamp(m_uvRect.w, 0.0f, 1.0f);

    // 色
    ImGui::ColorEdit4(SJ("カラー").c_str(), (float*)&m_color);

    // シェーダ設定
    if (ImGui::TreeNode(SJ("シェーダ設定").c_str())) {
        auto* sm = ShaderManager::GetInstance();
        static std::vector<std::string> vsList;
        static std::vector<std::string> psList;
        if (ImGui::Button(SJ("リスト更新").c_str())) {
            vsList = sm->GetShaderList("VS");
            psList = sm->GetShaderList("PS");
        }
        if (vsList.empty()) vsList = sm->GetShaderList("VS");
        if (psList.empty()) psList = sm->GetShaderList("PS");

        if (ImGui::BeginCombo("VS", m_vsName.c_str())) {
            for (auto& n : vsList) {
                bool sel = (n == m_vsName);
                if (ImGui::Selectable(n.c_str(), sel)) {
                    m_vsName = n;
                    EnsureShaders(true);
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::BeginCombo("PS", m_psName.c_str())) {
            for (auto& n : psList) {
                bool sel = (n == m_psName);
                if (ImGui::Selectable(n.c_str(), sel)) {
                    m_psName = n;
                    EnsureShaders(false);
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::TreePop();
    }
}