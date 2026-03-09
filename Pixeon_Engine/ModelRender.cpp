#define NOMINMAX
#include "ModelRender.h"
#include "System.h"
#include "Scene.h"
#include "AssetManager.h"
#include "CameraComponent.h"
#include "SettingManager.h"
#include "GUI.h"
#include "IMGUI/imgui.h"

using namespace DirectX;

Microsoft::WRL::ComPtr<ID3D11SamplerState>       ModelRenderComponent::s_linearSmp;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ModelRenderComponent::s_whiteTexSRV;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ModelRenderComponent::s_magentaTexSRV;
Microsoft::WRL::ComPtr<ID3D11RasterizerState>    ModelRenderComponent::s_rasterizerCullBack;
Microsoft::WRL::ComPtr<ID3D11RasterizerState>    ModelRenderComponent::s_rasterizerCullFront;
Microsoft::WRL::ComPtr<ID3D11RasterizerState>    ModelRenderComponent::s_rasterizerCullNone;

void ModelRenderComponent::Init(AbstractObject* owner) {
    _Parent = owner;
    _ComponentName = "ModelRender";
    _Type = ComponentManager::COMPONENT_TYPE::MODEL;
}

bool ModelRenderComponent::SetModel(const std::string& logicalPath) {
    m_modelPath = logicalPath;
    m_model = ModelManager::Instance()->LoadOrGet(logicalPath);
    if (!m_model) {
        m_ready = false;
        return false;
    }

    if (m_model->hasSkin) {
        if (!m_model->restPoseBones.empty()) {
            m_boneMatrices = m_model->restPoseBones;
            m_useBoneMatrices = true;
        }
        else {
            m_boneMatrices.clear();
            m_useBoneMatrices = false;
        }
    }
    else {
        m_boneMatrices.clear();
        m_useBoneMatrices = false;
    }

    RefreshMaterialCache();
    if (!EnsureShaders(true)) return false;
    if (!EnsureConstantBuffer()) return false;
    if (!EnsureRasterizerStates()) return false;
    m_texIssueReported.assign(m_model->submeshes.size(), 0);
    m_ready = true;
    return true;
}

void ModelRenderComponent::RefreshMaterialCache() {
    m_materials.clear();
    if (!m_model) return;

    m_materials.reserve(m_model->materials.size());

    for (size_t idx = 0; idx < m_model->materials.size(); ++idx) {
        const auto& modelMat = m_model->materials[idx];
        MaterialRuntime rt;

        rt.texName = modelMat.baseColorTex;
        rt.color = modelMat.baseColor;
        rt.meshOffset = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
        rt.meshScale = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
        rt.meshRotation = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
        rt.cullMode = CullMode::Back;

        if (!modelMat.baseColorTex.empty()) {
            if (modelMat.isEmbedded && modelMat.baseColorTex[0] == '*') {
                auto srv = ModelManager::Instance()->GetEmbeddedTexture(m_modelPath, modelMat.baseColorTex);
                if (srv) {
                    auto texRes = std::make_shared<TextureResource>();
                    texRes->srv = srv;
                    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
                    srv->GetResource(resource.GetAddressOf());
                    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
                    if (SUCCEEDED(resource.As(&tex2D))) {
                        D3D11_TEXTURE2D_DESC desc;
                        tex2D->GetDesc(&desc);
                        texRes->width = desc.Width;
                        texRes->height = desc.Height;
                    }
                    rt.tex = texRes;
                }
            }
            else {
                rt.tex = TextureManager::Instance()->LoadOrGet(modelMat.baseColorTex);
            }
        }

        m_materials.push_back(rt);
    }
}

bool ModelRenderComponent::EnsureShaders(bool forceRecreateLayout) {
    auto* sm = ShaderManager::GetInstance();

    ID3D11VertexShader* vs = sm->GetVertexShader(m_vsName);
    ID3D11PixelShader* ps = sm->GetPixelShader(m_psName);

    if (!vs || !ps) {
        sm->UpdateAndCompileShaders();
        vs = sm->GetVertexShader(m_vsName);
        ps = sm->GetPixelShader(m_psName);
        if (!vs || !ps) return false;
    }

    bool vsChanged = (m_vs.Get() != vs);
    bool needLayout = forceRecreateLayout || vsChanged || !m_layout;

    m_vs = vs;
    m_ps = ps;

    if (needLayout) RecreateInputLayout();

    if (!s_linearSmp) {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sd.MinLOD = 0;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        auto dev = DirectX11::GetInstance()->GetDevice();
        dev->CreateSamplerState(&sd, s_linearSmp.GetAddressOf());
    }
    EnsureDebugFallbackTextures();
    EnsureRasterizerStates();

    RebuildShaderTextureSlots();

    return true;
}

void ModelRenderComponent::RebuildShaderTextureSlots() {
    std::unordered_map<std::string, std::string> prevAssign;
    for (auto& s : m_shaderTexSlots)
        prevAssign[s.varName] = s.assignedTexName;

    m_shaderTexSlots.clear();

    auto* sm = ShaderManager::GetInstance();
    auto* psRef = sm->GetReflection(ShaderStage::PS, m_psName);
    if (psRef) {
        for (auto& td : psRef->textures) {
            ShaderTextureSlot slot;
            slot.varName = td.name;
            slot.bindPoint = td.bindPoint;
            auto it = prevAssign.find(td.name);
            if (it != prevAssign.end() && !it->second.empty()) {
                slot.assignedTexName = it->second;
                slot.tex = TextureManager::Instance()->LoadOrGet(it->second);
            }
            m_shaderTexSlots.push_back(std::move(slot));
        }
    }
}

void ModelRenderComponent::RecreateInputLayout() {
    m_layout.Reset();
    const void* bc = nullptr;
    size_t      bcSize = 0;
    if (!ShaderManager::GetInstance()->GetVSBytecode(m_vsName, &bc, &bcSize)) return;
    EnsureInputLayout(bc, bcSize);
}

bool ModelRenderComponent::EnsureInputLayout(const void* vsBytecode, size_t size) {
    if (m_layout) return true;
    D3D11_INPUT_ELEMENT_DESC desc[] = {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, (UINT)offsetof(ModelVertex, position),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, (UINT)offsetof(ModelVertex, normal),      D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, (UINT)offsetof(ModelVertex, tangent),     D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, (UINT)offsetof(ModelVertex, uv),          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, (UINT)offsetof(ModelVertex, boneIndices), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, (UINT)offsetof(ModelVertex, boneWeights), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    auto    dev = DirectX11::GetInstance()->GetDevice();
    HRESULT hr = dev->CreateInputLayout(desc, _countof(desc), vsBytecode, size, m_layout.GetAddressOf());
    return SUCCEEDED(hr);
}

bool ModelRenderComponent::EnsureConstantBuffer() {
    auto dev = DirectX11::GetInstance()->GetDevice();

    if (!m_cb) {
        D3D11_BUFFER_DESC bd{};
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.ByteWidth = sizeof(CBData);
        bd.Usage = D3D11_USAGE_DEFAULT;
        if (FAILED(dev->CreateBuffer(&bd, nullptr, m_cb.GetAddressOf()))) return false;
    }

    if (!m_cameraCb) {
        D3D11_BUFFER_DESC bd{};
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.ByteWidth = sizeof(CameraCBData);
        bd.Usage = D3D11_USAGE_DEFAULT;
        if (FAILED(dev->CreateBuffer(&bd, nullptr, m_cameraCb.GetAddressOf()))) return false;
    }

    return true;
}

DirectX::XMMATRIX ModelRenderComponent::BuildWorldMatrix() const {
    Transform t = _Parent->GetWorldTransform();
    XMMATRIX S = XMMatrixScaling(t.scale.x, t.scale.y, t.scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z);
    XMMATRIX T = XMMatrixTranslation(t.position.x, t.position.y, t.position.z);
    return S * R * T;
}

bool ModelRenderComponent::EnsureWhiteTexture() {
    if (s_whiteTexSRV) return true;
    auto dev = DirectX11::GetInstance()->GetDevice();
    if (!dev) return false;
    uint32_t pixel = 0xFFFFFFFF;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = 1; td.Height = 1;
    td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = &pixel; init.SysMemPitch = 4;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    if (FAILED(dev->CreateTexture2D(&td, &init, tex.GetAddressOf()))) return false;
    if (FAILED(dev->CreateShaderResourceView(tex.Get(), nullptr, s_whiteTexSRV.GetAddressOf()))) return false;
    return true;
}

bool ModelRenderComponent::EnsureDebugFallbackTextures() {
    auto dev = DirectX11::GetInstance()->GetDevice();
    if (!dev) return false;
    if (!s_whiteTexSRV) {
        uint32_t pixel = 0xFFFFFFFF;
        D3D11_TEXTURE2D_DESC td{};
        td.Width = td.Height = 1;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init{ &pixel, 4, 4 };
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        if (SUCCEEDED(dev->CreateTexture2D(&td, &init, tex.GetAddressOf())))
            dev->CreateShaderResourceView(tex.Get(), nullptr, s_whiteTexSRV.GetAddressOf());
    }
    if (!s_magentaTexSRV) {
        uint8_t pix[4] = { 255, 255, 255, 255 };
        D3D11_TEXTURE2D_DESC td{};
        td.Width = td.Height = 1;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init{ pix, 4, 4 };
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        if (SUCCEEDED(dev->CreateTexture2D(&td, &init, tex.GetAddressOf())))
            dev->CreateShaderResourceView(tex.Get(), nullptr, s_magentaTexSRV.GetAddressOf());
    }
    return (s_whiteTexSRV && s_magentaTexSRV);
}

bool ModelRenderComponent::EnsureRasterizerStates() {
    auto dev = DirectX11::GetInstance()->GetDevice();
    if (!dev) return false;

    if (!s_rasterizerCullBack) {
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_BACK;
        rd.FrontCounterClockwise = FALSE;
        rd.DepthClipEnable = TRUE;
        if (FAILED(dev->CreateRasterizerState(&rd, s_rasterizerCullBack.GetAddressOf()))) return false;
    }
    if (!s_rasterizerCullFront) {
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_FRONT;
        rd.FrontCounterClockwise = FALSE;
        rd.DepthClipEnable = TRUE;
        if (FAILED(dev->CreateRasterizerState(&rd, s_rasterizerCullFront.GetAddressOf()))) return false;
    }
    if (!s_rasterizerCullNone) {
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.FrontCounterClockwise = FALSE;
        rd.DepthClipEnable = TRUE;
        if (FAILED(dev->CreateRasterizerState(&rd, s_rasterizerCullNone.GetAddressOf()))) return false;
    }
    return true;
}

void ModelRenderComponent::DiagnoseAndReportTextureIssue(size_t submeshIdx,
    const SubMesh& sm, const MaterialRuntime* mat,
    ID3D11ShaderResourceView* chosenSRV, bool usedMagentaFallback, bool usedWhiteFallback)
{
    if (submeshIdx >= m_texIssueReported.size()) return;
    if (m_texIssueReported[submeshIdx]) return;

    TextureIssue issue = TextureIssue::None;
    std::string  detail;

    if (sm.materialIndex >= m_materials.size()) {
        issue = TextureIssue::MaterialIndexOutOfRange;
        detail = "submesh.materialIndex=" + std::to_string(sm.materialIndex);
    }
    else {
        const MaterialRuntime* mrt = mat;
        if (!mrt) {
            issue = TextureIssue::TextureSRVNull;
            detail = "MaterialRuntime null";
        }
        else {
            if (mrt->texName.empty()) {
                issue = TextureIssue::MaterialNoPath;
                detail = "Material has no texture path";
            }
            else if (!mrt->tex) {
                issue = TextureIssue::TextureLoadFailed;
                detail = "LoadOrGet null " + mrt->texName;
            }
            else if (mrt->tex && !mrt->tex->srv) {
                issue = TextureIssue::TextureSRVNull;
                detail = "SRV null " + mrt->texName;
            }
            if (issue == TextureIssue::None) {
                if (!sm.hasUV) { issue = TextureIssue::NoUVChannel; detail = "No UV"; }
                else if (sm.uvAllZero) { issue = TextureIssue::UVAllZero;   detail = "All UV zero"; }
            }
        }
    }
    if (issue == TextureIssue::None && !s_linearSmp) {
        issue = TextureIssue::SamplerMissing; detail = "Sampler missing";
    }
    if (issue == TextureIssue::None) {
        if (usedMagentaFallback) { issue = TextureIssue::StillFallbackMagenta; detail = "Magenta fallback"; }
        else if (usedWhiteFallback) { issue = TextureIssue::StillFallbackWhite;   detail = "White fallback"; }
    }

    if (issue != TextureIssue::None) {
        m_texIssueReported[submeshIdx] = 1;
    }
}

DirectX::XMMATRIX ModelRenderComponent::BuildMeshWorldMatrix(
    const DirectX::XMFLOAT3& offset,
    const DirectX::XMFLOAT3& scale,
    const DirectX::XMFLOAT3& rotation) const
{
    Transform t = _Parent->GetWorldTransform();

    XMMATRIX meshLocalScale = XMMatrixScaling(
        scale.x * m_globalScale.x,
        scale.y * m_globalScale.y,
        scale.z * m_globalScale.z);

    XMMATRIX meshLocalRotation = XMMatrixRotationRollPitchYaw(
        rotation.x + m_globalRotation.x,
        rotation.y + m_globalRotation.y,
        rotation.z + m_globalRotation.z);

    XMMATRIX meshLocalTranslation = XMMatrixTranslation(
        offset.x + m_globalOffset.x,
        offset.y + m_globalOffset.y,
        offset.z + m_globalOffset.z);

    XMMATRIX meshLocal = meshLocalScale * meshLocalRotation * meshLocalTranslation;

    XMMATRIX objectWorld =
        XMMatrixScaling(t.scale.x, t.scale.y, t.scale.z) *
        XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z) *
        XMMatrixTranslation(t.position.x, t.position.y, t.position.z);

    return meshLocal * objectWorld;
}

void ModelRenderComponent::SetMeshOffset(size_t meshIndex, const DirectX::XMFLOAT3& offset) {
    if (meshIndex >= m_materials.size()) return;
    m_materials[meshIndex].meshOffset = offset;
}

DirectX::XMFLOAT3 ModelRenderComponent::GetMeshOffset(size_t meshIndex) const {
    if (meshIndex >= m_materials.size()) return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    return m_materials[meshIndex].meshOffset;
}

void ModelRenderComponent::SetMeshScale(size_t meshIndex, const DirectX::XMFLOAT3& scale) {
    if (meshIndex >= m_materials.size()) return;
    m_materials[meshIndex].meshScale = scale;
}

DirectX::XMFLOAT3 ModelRenderComponent::GetMeshScale(size_t meshIndex) const {
    if (meshIndex >= m_materials.size()) return DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
    return m_materials[meshIndex].meshScale;
}

void ModelRenderComponent::SetMeshRotation(size_t meshIndex, const DirectX::XMFLOAT3& rotation) {
    if (meshIndex >= m_materials.size()) return;
    m_materials[meshIndex].meshRotation = rotation;
}

DirectX::XMFLOAT3 ModelRenderComponent::GetMeshRotation(size_t meshIndex) const {
    if (meshIndex >= m_materials.size()) return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    return m_materials[meshIndex].meshRotation;
}

void ModelRenderComponent::SetMeshCullMode(size_t meshIndex, CullMode mode) {
    if (meshIndex >= m_materials.size()) return;
    m_materials[meshIndex].cullMode = mode;
}

ModelRenderComponent::CullMode ModelRenderComponent::GetMeshCullMode(size_t meshIndex) const {
    if (meshIndex >= m_materials.size()) return CullMode::Back;
    return m_materials[meshIndex].cullMode;
}

int ModelRenderComponent::GetBoneIndexByName(const std::string& boneName) const {
    if (!m_model) return -1;
    for (size_t i = 0; i < m_model->bones.size(); ++i)
        if (m_model->bones[i].name == boneName) return static_cast<int>(i);
    return -1;
}

std::string ModelRenderComponent::GetBoneNameByIndex(int boneIndex) const {
    if (!m_model || boneIndex < 0 || boneIndex >= static_cast<int>(m_model->bones.size())) return "";
    return m_model->bones[boneIndex].name;
}

DirectX::XMMATRIX ModelRenderComponent::GetBoneWorldMatrix(int boneIndex) const {
    if (!m_model || boneIndex < 0 || boneIndex >= static_cast<int>(m_boneMatrices.size()))
        return XMMatrixIdentity();
    XMMATRIX boneMatrix = XMLoadFloat4x4(&m_boneMatrices[boneIndex]);
    XMMATRIX modelWorld = BuildWorldMatrix();
    return boneMatrix * modelWorld;
}

DirectX::XMFLOAT3 ModelRenderComponent::GetBoneWorldPosition(int boneIndex) const {
    XMMATRIX  worldMatrix = GetBoneWorldMatrix(boneIndex);
    XMFLOAT3  position;
    position.x = XMVectorGetX(worldMatrix.r[3]);
    position.y = XMVectorGetY(worldMatrix.r[3]);
    position.z = XMVectorGetZ(worldMatrix.r[3]);
    return position;
}

DirectX::XMFLOAT4 ModelRenderComponent::GetBoneWorldRotationQuaternion(int boneIndex) const {
    using namespace DirectX;
    XMMATRIX worldMatrix = GetBoneWorldMatrix(boneIndex);
    XMVECTOR scale, rotation, translation;
    if (!XMMatrixDecompose(&scale, &rotation, &translation, worldMatrix))
        return XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    XMFLOAT4 quat;
    XMStoreFloat4(&quat, rotation);
    return quat;
}

DirectX::XMFLOAT3 ModelRenderComponent::GetBoneWorldRotation(int boneIndex) const {
    using namespace DirectX;
    XMMATRIX worldMatrix = GetBoneWorldMatrix(boneIndex);
    XMVECTOR scale, rotation, translation;
    if (!XMMatrixDecompose(&scale, &rotation, &translation, worldMatrix))
        return XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT4 quat;
    XMStoreFloat4(&quat, rotation);

    float sinp = 2.0f * (quat.w * quat.x + quat.y * quat.z);
    float cosp = 1.0f - 2.0f * (quat.x * quat.x + quat.y * quat.y);
    float pitch = std::atan2(sinp, cosp);

    float siny = 2.0f * (quat.w * quat.y - quat.z * quat.x);
    float yaw = std::abs(siny) >= 1.0f ? std::copysign(XM_PI / 2.0f, siny) : std::asin(siny);

    float sinr = 2.0f * (quat.w * quat.z + quat.x * quat.y);
    float cosr = 1.0f - 2.0f * (quat.y * quat.y + quat.z * quat.z);
    float roll = std::atan2(sinr, cosr);

    return XMFLOAT3(pitch, yaw, roll);
}

DirectX::XMFLOAT3 ModelRenderComponent::GetBoneWorldRotationDegrees(int boneIndex) const {
    XMFLOAT3 radians = GetBoneWorldRotation(boneIndex);
    return XMFLOAT3(
        XMConvertToDegrees(radians.x),
        XMConvertToDegrees(radians.y),
        XMConvertToDegrees(radians.z));
}

DirectX::XMFLOAT3 ModelRenderComponent::GetBoneLocalPosition(int boneIndex) const {
    if (!m_model || boneIndex < 0 || boneIndex >= static_cast<int>(m_boneMatrices.size()))
        return XMFLOAT3(0.0f, 0.0f, 0.0f);
    Transform objTransform = _Parent->GetWorldTransform();
    XMFLOAT3 localPos;
    localPos.x = m_boneMatrices[boneIndex]._41 * objTransform.scale.x;
    localPos.y = m_boneMatrices[boneIndex]._42 * objTransform.scale.y;
    localPos.z = m_boneMatrices[boneIndex]._43 * objTransform.scale.z;
    return localPos;
}

DirectX::XMFLOAT3 ModelRenderComponent::GetBoneLocalRotation(int boneIndex) const {
    using namespace DirectX;
    if (!m_model || boneIndex < 0 || boneIndex >= static_cast<int>(m_boneMatrices.size()))
        return XMFLOAT3(0.0f, 0.0f, 0.0f);

    XMMATRIX  boneMatrix = XMLoadFloat4x4(&m_boneMatrices[boneIndex]);
    XMVECTOR  scale, rotation, translation;
    if (!XMMatrixDecompose(&scale, &rotation, &translation, boneMatrix))
        return XMFLOAT3(0.0f, 0.0f, 0.0f);

    XMFLOAT4 quat;
    XMStoreFloat4(&quat, rotation);

    float sinp = 2.0f * (quat.w * quat.x + quat.y * quat.z);
    float cosp = 1.0f - 2.0f * (quat.x * quat.x + quat.y * quat.y);
    float pitch = std::atan2(sinp, cosp);

    float siny = 2.0f * (quat.w * quat.y - quat.z * quat.x);
    float yaw = std::abs(siny) >= 1.0f ? std::copysign(XM_PI / 2.0f, siny) : std::asin(siny);

    float sinr = 2.0f * (quat.w * quat.z + quat.x * quat.y);
    float cosr = 1.0f - 2.0f * (quat.y * quat.y + quat.z * quat.z);
    float roll = std::atan2(sinr, cosr);

    return XMFLOAT3(pitch, yaw, roll);
}

DirectX::XMFLOAT3 ModelRenderComponent::GetBoneLocalRotationDegrees(int boneIndex) const {
    XMFLOAT3 radians = GetBoneLocalRotation(boneIndex);
    return XMFLOAT3(
        XMConvertToDegrees(radians.x),
        XMConvertToDegrees(radians.y),
        XMConvertToDegrees(radians.z));
}

int ModelRenderComponent::GetBoneParentIndex(int boneIndex) const {
    if (!m_model || boneIndex < 0 || boneIndex >= static_cast<int>(m_model->bones.size())) return -1;
    return m_model->bones[boneIndex].parentIndex;
}

std::vector<int> ModelRenderComponent::GetBoneChildren(int boneIndex) const {
    std::vector<int> children;
    if (!m_model || boneIndex < 0 || boneIndex >= static_cast<int>(m_model->bones.size())) return children;
    for (size_t i = 0; i < m_model->bones.size(); ++i)
        if (m_model->bones[i].parentIndex == boneIndex) children.push_back(static_cast<int>(i));
    return children;
}

void ModelRenderComponent::EnsureDefaultBoneMatrices() {
    if (!m_model) return;
    if (!m_model->hasSkin) return;
    size_t required = m_model->bones.size();
    if (required == 0) required = 1;
    if (m_boneMatrices.size() != required) {
        m_boneMatrices.assign(required, DirectX::XMFLOAT4X4());
        for (auto& m : m_boneMatrices)
            XMStoreFloat4x4(&m, XMMatrixIdentity());
    }
    m_useBoneMatrices = true;
}

void ModelRenderComponent::Draw(int Layer) {
    if (Layer != _LayerNumber) return;
    if (!m_ready || !m_model) return;
    AbstractScene* AbstractScene = _Parent->GetParentScene();
    if (!AbstractScene) return;
    CameraComponent* cam = AbstractScene->GetMainCamera();
    if (!cam) return;

    if (!m_vs || !m_ps) {
        if (!EnsureShaders(false)) return;
    }
    if (m_model->hasSkin && m_boneMatrices.empty()) EnsureDefaultBoneMatrices();

    XMMATRIX view = cam->GetView();
    XMMATRIX proj = cam->GetProjection();
    XMFLOAT3 camPos = cam->GetPosition();

    auto ctx = DirectX11::GetInstance()->GetContext();

    UINT stride = sizeof(ModelVertex);
    UINT offset = 0;
    ID3D11Buffer* vb = m_model->vb.Get();
    ID3D11Buffer* ib = m_model->ib.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetInputLayout(m_layout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);

    ID3D11SamplerState* smp = s_linearSmp.Get();
    ctx->PSSetSamplers(0, 1, &smp);

    if (m_model->hasSkin && m_useBoneMatrices && !m_boneMatrices.empty())
        SetupBoneMatricesForShader(ctx);

    EnsureDebugFallbackTextures();

    auto* sm = ShaderManager::GetInstance();

    for (size_t i = 0; i < m_model->submeshes.size(); ++i) {
        const SubMesh& smesh = m_model->submeshes[i];
        size_t matIndex = smesh.materialIndex;

        ID3D11ShaderResourceView* srv = s_whiteTexSRV.Get();
        XMFLOAT4 materialColor = m_color;
        XMFLOAT3 meshOffset(0.0f, 0.0f, 0.0f);
        XMFLOAT3 meshScale(1.0f, 1.0f, 1.0f);
        XMFLOAT3 meshRotation(0.0f, 0.0f, 0.0f);
        CullMode  cullMode = CullMode::Back;
        bool usedWhite = true;
        bool usedMagenta = false;
        MaterialRuntime* matPtr = nullptr;

        if (matIndex < m_materials.size()) {
            matPtr = &m_materials[matIndex];
            auto& mat = *matPtr;

            materialColor.x *= mat.color.x;
            materialColor.y *= mat.color.y;
            materialColor.z *= mat.color.z;
            materialColor.w *= mat.color.w;

            meshOffset = mat.meshOffset;
            meshScale = mat.meshScale;
            meshRotation = mat.meshRotation;
            cullMode = mat.cullMode;

            if (mat.tex && mat.tex->srv) {
                srv = mat.tex->srv.Get();
                usedWhite = false;
            }
            else if (mat.color.x != 1.0f || mat.color.y != 1.0f ||
                mat.color.z != 1.0f || mat.color.w != 1.0f) {
                srv = s_whiteTexSRV.Get();
                usedWhite = false;
            }
            else if (!mat.texName.empty()) {
                srv = s_magentaTexSRV.Get();
                usedMagenta = true;
                usedWhite = false;
            }
        }
        else {
            srv = s_magentaTexSRV.Get();
            usedMagenta = true;
            usedWhite = false;
        }

        ID3D11RasterizerState* rasterizerState = nullptr;
        switch (cullMode) {
        case CullMode::Back:  rasterizerState = s_rasterizerCullBack.Get();  break;
        case CullMode::Front: rasterizerState = s_rasterizerCullFront.Get(); break;
        case CullMode::None:  rasterizerState = s_rasterizerCullNone.Get();  break;
        }
        if (rasterizerState) ctx->RSSetState(rasterizerState);

        XMMATRIX world = BuildMeshWorldMatrix(meshOffset, meshScale, meshRotation);

        CBData cbd;
        cbd.World = XMMatrixTranspose(world);
        cbd.View = XMMatrixTranspose(view);
        cbd.Proj = XMMatrixTranspose(proj);
        cbd.BaseColor = materialColor;

        ctx->UpdateSubresource(m_cb.Get(), 0, nullptr, &cbd, 0, 0);
        ID3D11Buffer* cbs[] = { m_cb.Get() };
        ctx->VSSetConstantBuffers(0, 1, cbs);
        ctx->PSSetConstantBuffers(0, 1, cbs);

        CameraCBData camCbd;
        camCbd.CameraPos = camPos;
        camCbd._pad = 0.0f;
        ctx->UpdateSubresource(m_cameraCb.Get(), 0, nullptr, &camCbd, 0, 0);
        ID3D11Buffer* camCbs[] = { m_cameraCb.Get() };
        ctx->PSSetConstantBuffers(4, 1, camCbs);

        ctx->PSSetShaderResources(0, 1, &srv);

        for (auto& slot : m_shaderTexSlots) {
            ID3D11ShaderResourceView* slotSRV =
                (slot.tex && slot.tex->srv) ? slot.tex->srv.Get() : s_whiteTexSRV.Get();
            sm->BindSRV(ShaderStage::PS, m_psName, slot.varName, slotSRV);
        }

        ctx->DrawIndexed(smesh.indexCount, smesh.indexOffset, 0);

        if (usedWhite || usedMagenta)
            DiagnoseAndReportTextureIssue(i, smesh, matPtr, srv, usedMagenta, usedWhite);
    }

    ctx->RSSetState(s_rasterizerCullBack.Get());
}

void ModelRenderComponent::SetupBoneMatricesForShader(ID3D11DeviceContext* ctx) {
    static Microsoft::WRL::ComPtr<ID3D11Buffer> s_boneCB;
    if (!s_boneCB) {
        D3D11_BUFFER_DESC bd{};
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.ByteWidth = sizeof(DirectX::XMFLOAT4X4) * 256;
        bd.Usage = D3D11_USAGE_DEFAULT;
        auto dev = DirectX11::GetInstance()->GetDevice();
        if (FAILED(dev->CreateBuffer(&bd, nullptr, s_boneCB.GetAddressOf()))) return;
    }
    struct BoneCB { DirectX::XMFLOAT4X4 m[256]; } data;
    size_t count = std::min(m_boneMatrices.size(), size_t(256));
    for (size_t i = 0; i < count; ++i) {
        DirectX::XMMATRIX M = DirectX::XMLoadFloat4x4(&m_boneMatrices[i]);
        DirectX::XMStoreFloat4x4(&data.m[i], M);
    }
    for (size_t i = count; i < 256; ++i)
        DirectX::XMStoreFloat4x4(&data.m[i], DirectX::XMMatrixIdentity());

    ctx->UpdateSubresource(s_boneCB.Get(), 0, nullptr, &data, 0, 0);
    ID3D11Buffer* cbs[] = { s_boneCB.Get() };
    ctx->VSSetConstantBuffers(1, 1, cbs);
}

bool ModelRenderComponent::SetMaterialTexture(int materialIndex, const std::string& texLogicalPath) {
    if (materialIndex < 0 || materialIndex >= static_cast<int>(m_materials.size())) return false;
    auto& mat = m_materials[materialIndex];
    mat.texName = texLogicalPath;
    if (!texLogicalPath.empty()) {
        mat.tex = TextureManager::Instance()->LoadOrGet(texLogicalPath);
        return (mat.tex != nullptr);
    }
    else {
        mat.tex.reset();
        return true;
    }
}

void ModelRenderComponent::SaveToFile(std::ostream& out) {
    out << m_modelPath << "\n";
    out << m_color.x << " " << m_color.y << " " << m_color.z << " " << m_color.w << "\n";
    out << m_vsName << "\n" << m_psName << "\n";
    out << _LayerNumber << "\n";

    out << m_globalOffset.x << " " << m_globalOffset.y << " " << m_globalOffset.z << "\n";
    out << m_globalScale.x << " " << m_globalScale.y << " " << m_globalScale.z << "\n";
    out << m_globalRotation.x << " " << m_globalRotation.y << " " << m_globalRotation.z << "\n";

    out << m_materials.size() << "\n";
    for (const auto& mat : m_materials) {
        out << mat.texName << "\n";
        out << mat.meshOffset.x << " " << mat.meshOffset.y << " " << mat.meshOffset.z << "\n";
        out << mat.meshScale.x << " " << mat.meshScale.y << " " << mat.meshScale.z << "\n";
        out << mat.meshRotation.x << " " << mat.meshRotation.y << " " << mat.meshRotation.z << "\n";
        out << static_cast<int>(mat.cullMode) << "\n";
    }

    out << m_shaderTexSlots.size() << "\n";
    for (const auto& slot : m_shaderTexSlots) {
        out << slot.varName << "\n";
        out << slot.assignedTexName << "\n";
    }
}

void ModelRenderComponent::LoadFromFile(std::istream& in) {
    std::getline(in, m_modelPath);
    in >> m_color.x >> m_color.y >> m_color.z >> m_color.w;
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::getline(in, m_vsName);
    std::getline(in, m_psName);
    in >> _LayerNumber;
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    in >> m_globalOffset.x >> m_globalOffset.y >> m_globalOffset.z;
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    in >> m_globalScale.x >> m_globalScale.y >> m_globalScale.z;
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    in >> m_globalRotation.x >> m_globalRotation.y >> m_globalRotation.z;
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    SetModel(m_modelPath);

    size_t matCount = 0;
    in >> matCount;
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    for (size_t i = 0; i < matCount && i < m_materials.size(); ++i) {
        std::string texName;
        std::getline(in, texName);

        XMFLOAT3 offset;
        in >> offset.x >> offset.y >> offset.z;
        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        XMFLOAT3 scale;
        in >> scale.x >> scale.y >> scale.z;
        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        XMFLOAT3 rotation;
        in >> rotation.x >> rotation.y >> rotation.z;
        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        int cullModeInt;
        in >> cullModeInt;
        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (!texName.empty() && texName != m_materials[i].texName)
            SetMaterialTexture(i, texName);

        m_materials[i].meshOffset = offset;
        m_materials[i].meshScale = scale;
        m_materials[i].meshRotation = rotation;
        m_materials[i].cullMode = static_cast<CullMode>(cullModeInt);
    }

    size_t slotCount = 0;
    if (in >> slotCount) {
        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::unordered_map<std::string, std::string> savedSlots;
        for (size_t i = 0; i < slotCount; ++i) {
            std::string varName, texName;
            std::getline(in, varName);
            std::getline(in, texName);
            savedSlots[varName] = texName;
        }
        for (auto& slot : m_shaderTexSlots) {
            auto it = savedSlots.find(slot.varName);
            if (it != savedSlots.end() && !it->second.empty()) {
                slot.assignedTexName = it->second;
                slot.tex = TextureManager::Instance()->LoadOrGet(it->second);
            }
        }
    }
}

void ModelRenderComponent::SetBoneMatrices(const std::vector<DirectX::XMFLOAT4X4>& matrices) {
    m_boneMatrices = matrices;
    m_useBoneMatrices = !matrices.empty();
}

void ModelRenderComponent::DrawInspector() {
    auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };
    std::string title;
    title = _ComponentName + "##" + std::to_string(reinterpret_cast<uintptr_t>(this));
    if (!ImGui::CollapsingHeader(title.c_str())) return;
    title = "ModelTable##" + std::to_string(reinterpret_cast<uintptr_t>(this));
    if (!ImGui::BeginTable(title.c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) return;

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("レイヤー番号: ").c_str());
    ImGui::TableSetColumnIndex(1); ImGui::InputInt("Layer", &_LayerNumber);
    if (0 > _LayerNumber)  _LayerNumber = 0;
    if (10 <= _LayerNumber) _LayerNumber = 9;

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("モデル名").c_str());
    ImGui::TableSetColumnIndex(1); ImGui::Text("%s", m_modelPath.c_str());

    ImGui::TableNextRow();
    static char pathBuf[256];
    std::snprintf(pathBuf, sizeof(pathBuf), "%s", m_modelPath.c_str());
    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("モデルの設定").c_str());
    ImGui::TableSetColumnIndex(1);
    if (ImGui::Button(SJ("再読み込み/適用").c_str())) SetModel(pathBuf);
    ImGui::SameLine();
    if (ImGui::Button(SJ("モデル選択").c_str())) ImGui::OpenPopup("ModelSelectPopup");
    ShowModelSelectPopup();

    ImGui::TableNextRow();

    auto* sm = ShaderManager::GetInstance();
    static std::vector<std::string> vsList;
    static std::vector<std::string> psList;
    if (vsList.empty()) vsList = sm->GetShaderList("VS");
    if (psList.empty()) psList = sm->GetShaderList("PS");

    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("シェーダーの再読み込み").c_str());
    ImGui::TableSetColumnIndex(1);
    if (ImGui::Button(SJ("更新(一覧)").c_str())) {
        vsList = sm->GetShaderList("VS");
        psList = sm->GetShaderList("PS");
    }

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("頂点シェーダー").c_str());
    ImGui::TableSetColumnIndex(1);
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

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("ピクセルシェーダー").c_str());
    ImGui::TableSetColumnIndex(1);
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

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("ベース色設定").c_str());
    ImGui::TableSetColumnIndex(1); ImGui::ColorEdit4("Color", (float*)&m_color);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("全体オフセット").c_str());
    ImGui::TableSetColumnIndex(1);
    float globalOffset[3] = { m_globalOffset.x, m_globalOffset.y, m_globalOffset.z };
    if (ImGui::DragFloat3("GlobalOffset", globalOffset, 0.01f, -100.0f, 100.0f)) {
        m_globalOffset.x = globalOffset[0];
        m_globalOffset.y = globalOffset[1];
        m_globalOffset.z = globalOffset[2];
    }

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("全体スケール").c_str());
    ImGui::TableSetColumnIndex(1);
    float globalScale[3] = { m_globalScale.x, m_globalScale.y, m_globalScale.z };
    if (ImGui::DragFloat3("GlobalScale", globalScale, 0.01f, 0.01f, 10.0f)) {
        m_globalScale.x = globalScale[0];
        m_globalScale.y = globalScale[1];
        m_globalScale.z = globalScale[2];
    }

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("全体の回転").c_str());
    ImGui::TableSetColumnIndex(1);
    XMFLOAT3 globalRotationDeg;
    globalRotationDeg.x = XMConvertToDegrees(m_globalRotation.x);
    globalRotationDeg.y = XMConvertToDegrees(m_globalRotation.y);
    globalRotationDeg.z = XMConvertToDegrees(m_globalRotation.z);
    float globalRot[3] = { globalRotationDeg.x, globalRotationDeg.y, globalRotationDeg.z };
    if (ImGui::DragFloat3("GlobalRotation", globalRot, 0.5f, -180.0f, 180.0f)) {
        m_globalRotation.x = XMConvertToRadians(globalRot[0]);
        m_globalRotation.y = XMConvertToRadians(globalRot[1]);
        m_globalRotation.z = XMConvertToRadians(globalRot[2]);
    }

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::EndTable();

    DrawShaderTextureSlotInspector();

    title = "マテリアル一覧##" + std::to_string(reinterpret_cast<uintptr_t>(this));
    if (ImGui::TreeNode(SJ(title.c_str()).c_str())) {
        ImGui::Text("%s %zu", SJ("マテリアル数:").c_str(), m_materials.size());
        for (size_t i = 0; i < m_materials.size(); ++i) {
            ImGui::PushID((int)i);
            std::string nodeLabel = "Mat " + std::to_string(i);
            if (ImGui::TreeNode(nodeLabel.c_str())) {
                std::string shown = m_materials[i].texName.empty() ? SJ("(なし)") : m_materials[i].texName;
                ImGui::Text("tex=%s", shown.c_str());
                ImGui::SameLine();
                if (ImGui::Button(SJ("テクスチャ変更").c_str())) {
                    m_openTexPopup = true;
                    m_texPopupMatIndex = (int)i;
                    ImGui::OpenPopup("TextureSelectPopup");
                }

                ImGui::Separator();
                ImGui::Text("%s", SJ("メッシュオフセット").c_str());
                float offset[3] = { m_materials[i].meshOffset.x, m_materials[i].meshOffset.y, m_materials[i].meshOffset.z };
                if (ImGui::DragFloat3("Offset", offset, 0.01f, -100.0f, 100.0f)) {
                    m_materials[i].meshOffset.x = offset[0];
                    m_materials[i].meshOffset.y = offset[1];
                    m_materials[i].meshOffset.z = offset[2];
                }

                ImGui::Text("%s", SJ("メッシュスケール").c_str());
                float scale[3] = { m_materials[i].meshScale.x, m_materials[i].meshScale.y, m_materials[i].meshScale.z };
                if (ImGui::DragFloat3("Scale", scale, 0.01f, 0.01f, 10.0f)) {
                    m_materials[i].meshScale.x = scale[0];
                    m_materials[i].meshScale.y = scale[1];
                    m_materials[i].meshScale.z = scale[2];
                }

                ImGui::Text("%s", SJ("メッシュ回転(度)").c_str());
                float rotDeg[3] = {
                    XMConvertToDegrees(m_materials[i].meshRotation.x),
                    XMConvertToDegrees(m_materials[i].meshRotation.y),
                    XMConvertToDegrees(m_materials[i].meshRotation.z)
                };
                if (ImGui::DragFloat3("Rotation", rotDeg, 0.5f, -180.0f, 180.0f)) {
                    m_materials[i].meshRotation.x = XMConvertToRadians(rotDeg[0]);
                    m_materials[i].meshRotation.y = XMConvertToRadians(rotDeg[1]);
                    m_materials[i].meshRotation.z = XMConvertToRadians(rotDeg[2]);
                }

                ImGui::Separator();
                ImGui::Text("%s", SJ("カリングモード").c_str());
                std::string cullBack = SJ("裏面カリング(通常)");
                std::string cullFront = SJ("表面カリング");
                std::string cullNone = SJ("両面描画");
                const char* cullModeNames[] = { cullBack.c_str(), cullFront.c_str(), cullNone.c_str() };
                int currentCullMode = static_cast<int>(m_materials[i].cullMode);
                if (ImGui::Combo("CullMode", &currentCullMode, cullModeNames, 3))
                    m_materials[i].cullMode = static_cast<CullMode>(currentCullMode);

                if (ImGui::Button(SJ("オフセットリセット").c_str()))
                    m_materials[i].meshOffset = XMFLOAT3(0.0f, 0.0f, 0.0f);
                ImGui::SameLine();
                if (ImGui::Button(SJ("スケールリセット").c_str()))
                    m_materials[i].meshScale = XMFLOAT3(1.0f, 1.0f, 1.0f);
                ImGui::SameLine();
                if (ImGui::Button(SJ("回転リセット").c_str()))
                    m_materials[i].meshRotation = XMFLOAT3(0.0f, 0.0f, 0.0f);

                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    if (m_model && m_model->hasSkin && !m_model->bones.empty()) {
        ImGui::Separator();
        title = SJ("ボーン表示") + " (" + std::to_string(m_model->bones.size()) + ")##" +
            std::to_string(reinterpret_cast<uintptr_t>(this));
        if (ImGui::TreeNode(title.c_str())) {
            ImGui::InputText(SJ("フィルタ").c_str(), m_boneFilterBuffer, sizeof(m_boneFilterBuffer));
            ImGui::SameLine();
            if (ImGui::Button(SJ("すべて展開").c_str())) {
                for (size_t i = 0; i < m_model->bones.size(); ++i) m_boneTreeOpenState[i] = true;
            }
            ImGui::SameLine();
            if (ImGui::Button(SJ("すべて畳む").c_str())) m_boneTreeOpenState.clear();
            ImGui::Separator();
            ImGui::BeginChild("BoneHierarchy", ImVec2(0, 300), true);
            for (size_t i = 0; i < m_model->bones.size(); ++i)
                if (m_model->bones[i].parentIndex < 0) DrawBoneHierarchyRecursive((int)i, 0);
            ImGui::EndChild();
            if (m_selectedBoneIndex >= 0 && m_selectedBoneIndex < (int)m_model->bones.size()) {
                ImGui::Separator();
                DrawBoneDetails(m_selectedBoneIndex);
            }
            ImGui::TreePop();
        }
    }

    if (m_openTexPopup) ImGui::OpenPopup("TextureSelectPopup");

    if (ImGui::BeginPopupModal("TextureSelectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char filter[128] = "";
        ImGui::InputText(SJ("フィルタ").c_str(), filter, sizeof(filter));
        auto list = AssetManager::Instance()->GetCachedTextureNames();
        ImGui::Text("%s:  %zu", SJ("件数").c_str(), list.size());
        ImGui::Separator();
        ImGui::BeginChild("TextureSelectList", ImVec2(420, 320), true);
        static int highlight = -1;
        for (int i = 0; i < (int)list.size(); ++i) {
            const std::string& rawName = list[i];
            if (filter[0] && rawName.find(filter) == std::string::npos) continue;
            std::string dispName = GUI::GetInstance()->ShiftJISToUTF8(rawName.c_str());
            bool selected = (highlight == i);
            if (ImGui::Selectable(dispName.c_str(), selected)) {
                highlight = i;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    m_materials[m_texPopupMatIndex].texName = rawName;
                    m_materials[m_texPopupMatIndex].tex = TextureManager::Instance()->LoadOrGet(rawName);
                    ImGui::CloseCurrentPopup();
                    m_openTexPopup = false;
                    m_texPopupMatIndex = -1;
                }
            }
            if (m_texPopupMatIndex >= 0 && m_texPopupMatIndex < (int)m_materials.size()
                && m_materials[m_texPopupMatIndex].texName == rawName) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), SJ("[使用中]").c_str());
            }
        }
        ImGui::EndChild();
        ImGui::Separator();
        if (ImGui::Button(SJ("適用").c_str())) {
            if (highlight >= 0 && highlight < (int)list.size() && m_texPopupMatIndex >= 0) {
                const std::string& sel = list[highlight];
                m_materials[m_texPopupMatIndex].texName = sel;
                m_materials[m_texPopupMatIndex].tex = TextureManager::Instance()->LoadOrGet(sel);
            }
            ImGui::CloseCurrentPopup();
            m_openTexPopup = false;
            m_texPopupMatIndex = -1;
        }
        ImGui::SameLine();
        if (ImGui::Button(SJ("キャンセル").c_str())) {
            ImGui::CloseCurrentPopup();
            m_openTexPopup = false;
            m_texPopupMatIndex = -1;
        }
        ImGui::EndPopup();
    }
}

void ModelRenderComponent::DrawShaderTextureSlotInspector() {
    auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };

    ImGui::Separator();
    std::string title = SJ("シェーダーテクスチャスロット##") + std::to_string(reinterpret_cast<uintptr_t>(this));
    if (!ImGui::TreeNode(title.c_str())) return;

    if (m_shaderTexSlots.empty()) {
        ImGui::TextDisabled("%s", SJ("(スロットなし / PSリフレクション未取得)").c_str());
        ImGui::TreePop();
        return;
    }

    for (int i = 0; i < (int)m_shaderTexSlots.size(); ++i) {
        auto& slot = m_shaderTexSlots[i];
        ImGui::PushID(i);

        ImGui::Text("t%u: %s", slot.bindPoint, slot.varName.c_str());
        ImGui::SameLine();

        if (slot.assignedTexName.empty()) {
            ImGui::TextDisabled("[未設定]");
        }
        else {
            bool loaded = slot.tex && slot.tex->srv;
            ImGui::TextColored(
                loaded ? ImVec4(0.5f, 0.9f, 0.5f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                "%s", slot.assignedTexName.c_str());
        }

        ImGui::SameLine();
        if (ImGui::SmallButton(SJ("選択").c_str())) {
            m_openShaderTexPopup = true;
            m_shaderTexPopupSlotIndex = i;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) {
            slot.assignedTexName.clear();
            slot.tex.reset();
        }

        if (slot.tex && slot.tex->srv) {
            ImGui::Image((ImTextureID)slot.tex->srv.Get(), ImVec2(48, 48));
        }

        ImGui::PopID();
    }

    if (m_openShaderTexPopup) {
        ImGui::OpenPopup("ShaderTexSelect");
        m_openShaderTexPopup = false;
    }

    if (ImGui::BeginPopup("ShaderTexSelect")) {
        ImGui::TextUnformatted(SJ("テクスチャを選択").c_str());
        ImGui::Separator();
        static char filter[128] = "";
        ImGui::InputText(SJ("フィルター").c_str(), filter, sizeof(filter));
        auto texList = AssetManager::Instance()->GetCachedTextureNames();
        ImGui::BeginChild("ShaderTexList", ImVec2(300, 300), true);
        for (auto& name : texList) {
            if (filter[0] && name.find(filter) == std::string::npos) continue;
            std::string dispName = GUI::GetInstance()->ShiftJISToUTF8(name.c_str());
            if (ImGui::Selectable(dispName.c_str())) {
                if (m_shaderTexPopupSlotIndex >= 0 &&
                    m_shaderTexPopupSlotIndex < (int)m_shaderTexSlots.size()) {
                    auto& slot = m_shaderTexSlots[m_shaderTexPopupSlotIndex];
                    slot.assignedTexName = name;
                    slot.tex = TextureManager::Instance()->LoadOrGet(name);
                }
                ImGui::CloseCurrentPopup();
                m_shaderTexPopupSlotIndex = -1;
            }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }

    ImGui::TreePop();
}

void ModelRenderComponent::ShowModelSelectPopup() {
    if (ImGui::BeginPopupModal("ModelSelectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };
        static char filter[128] = "";
        ImGui::InputText(SJ("フィルタ(部分一致)").c_str(), filter, sizeof(filter));
        auto list = AssetManager::Instance()->GetCachedAssetNames(true);
        {
            std::string label = SJ("登録モデル数:  ") + std::to_string(list.size());
            ImGui::Text("%s", label.c_str());
        }
        ImGui::Separator();
        ImGui::BeginChild("ModelSelectList", ImVec2(420, 320), true);
        static int currentHighlight = -1;
        for (int i = 0; i < (int)list.size(); ++i) {
            const std::string& rawName = list[i];
            if (filter[0] && rawName.find(filter) == std::string::npos) continue;
            std::string dispName = GUI::GetInstance()->ShiftJISToUTF8(rawName.c_str());
            bool selected = (currentHighlight == i);
            if (ImGui::Selectable(dispName.c_str(), selected)) {
                currentHighlight = i;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (SetModel(rawName)) m_modelPath = rawName;
                    ImGui::CloseCurrentPopup();
                }
            }
            if (m_modelPath == rawName) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), SJ("[使用中]").c_str());
            }
        }
        ImGui::EndChild();
        ImGui::Separator();
        if (ImGui::Button(SJ("適用").c_str())) {
            if (currentHighlight >= 0 && currentHighlight < (int)list.size()) {
                const std::string& sel = list[currentHighlight];
                if (SetModel(sel)) m_modelPath = sel;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(SJ("キャンセル").c_str())) ImGui::CloseCurrentPopup();
        ImGui::SameLine();
        if (ImGui::Button(SJ("再ロード").c_str())) {
            if (!m_modelPath.empty()) SetModel(m_modelPath);
        }
        ImGui::EndPopup();
    }
}

void ModelRenderComponent::ShowTextureSelectPopup(int materialIndex) {
    if (!m_openTexPopup || materialIndex < 0 || materialIndex >= (int)m_materials.size()) return;
    if (ImGui::BeginPopupModal("TextureSelectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };
        static char filter[128] = "";
        ImGui::InputText(SJ("フィルタ").c_str(), filter, sizeof(filter));
        auto list = AssetManager::Instance()->GetCachedTextureNames();
        ImGui::Text("%s: %zu", SJ("件数").c_str(), list.size());
        ImGui::Separator();
        ImGui::BeginChild("TextureSelectList", ImVec2(420, 320), true);
        static int highlight = -1;
        for (int i = 0; i < (int)list.size(); ++i) {
            const std::string& rawName = list[i];
            if (filter[0] && rawName.find(filter) == std::string::npos) continue;
            std::string dispName = GUI::GetInstance()->ShiftJISToUTF8(rawName.c_str());
            bool selected = (highlight == i);
            if (ImGui::Selectable(dispName.c_str(), selected)) {
                highlight = i;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    m_materials[materialIndex].texName = rawName;
                    m_materials[materialIndex].tex = TextureManager::Instance()->LoadOrGet(rawName);
                    ImGui::CloseCurrentPopup();
                    m_openTexPopup = false;
                    m_texPopupMatIndex = -1;
                }
            }
            if (m_materials[materialIndex].texName == rawName) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), SJ("[使用中]").c_str());
            }
        }
        ImGui::EndChild();
        ImGui::Separator();
        if (ImGui::Button(SJ("適用").c_str())) {
            if (highlight >= 0 && highlight < (int)list.size()) {
                const std::string& sel = list[highlight];
                m_materials[materialIndex].texName = sel;
                m_materials[materialIndex].tex = TextureManager::Instance()->LoadOrGet(sel);
            }
            ImGui::CloseCurrentPopup();
            m_openTexPopup = false;
            m_texPopupMatIndex = -1;
        }
        ImGui::SameLine();
        if (ImGui::Button(SJ("キャンセル").c_str())) {
            ImGui::CloseCurrentPopup();
            m_openTexPopup = false;
            m_texPopupMatIndex = -1;
        }
        ImGui::EndPopup();
    }
}

void ModelRenderComponent::DrawBoneHierarchyRecursive(int boneIndex, int depth) {
    if (!m_model || boneIndex < 0 || boneIndex >= (int)m_model->bones.size()) return;

    auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };
    const auto& bone = m_model->bones[boneIndex];

    if (m_boneFilterBuffer[0] != '\0') {
        std::string filter(m_boneFilterBuffer);
        if (bone.name.find(filter) == std::string::npos) {
            bool hasMatchingChild = false;
            for (size_t i = 0; i < m_model->bones.size(); ++i) {
                if (m_model->bones[i].parentIndex == boneIndex) { hasMatchingChild = true; break; }
            }
            if (!hasMatchingChild) return;
        }
    }

    std::vector<int> children;
    for (size_t i = 0; i < m_model->bones.size(); ++i)
        if (m_model->bones[i].parentIndex == boneIndex) children.push_back((int)i);

    for (int i = 0; i < depth; ++i) ImGui::Indent(16.0f);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (m_selectedBoneIndex == boneIndex) flags |= ImGuiTreeNodeFlags_Selected;
    if (m_boneTreeOpenState.find(boneIndex) != m_boneTreeOpenState.end() && m_boneTreeOpenState[boneIndex])
        ImGui::SetNextItemOpen(true);

    std::string label = bone.name + " [" + std::to_string(boneIndex) + "]";
    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)boneIndex, flags, "%s", label.c_str());

    if (ImGui::IsItemClicked()) m_selectedBoneIndex = boneIndex;

    if (ImGui::BeginPopupContextItem()) {
        ImGui::Text("%s", SJ("ボーン: ").c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", bone.name.c_str());
        ImGui::Separator();
        if (ImGui::MenuItem(SJ("名前をコピー").c_str())) ImGui::SetClipboardText(bone.name.c_str());
        if (ImGui::MenuItem(SJ("ワールド位置をログ出力").c_str())) {
            XMFLOAT3 pos = GetBoneWorldPosition(boneIndex);
            char buf[256];
            sprintf_s(buf, "[Bone] %s: (%.3f, %.3f, %.3f)\n", bone.name.c_str(), pos.x, pos.y, pos.z);
            OutputDebugStringA(buf);
        }
        ImGui::EndPopup();
    }

    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s: %s", SJ("ボーン名").c_str(), bone.name.c_str());
        ImGui::Text("%s: %d", SJ("インデックス").c_str(), boneIndex);
        ImGui::Text("%s: %d", SJ("親インデックス").c_str(), bone.parentIndex);
        ImGui::Text("%s: %d", SJ("ノードインデックス").c_str(), bone.nodeIndex);
        ImGui::Text("%s: %zu", SJ("子の数").c_str(), children.size());
        ImGui::EndTooltip();
    }

    if (nodeOpen) {
        m_boneTreeOpenState[boneIndex] = true;
        if (!children.empty()) {
            for (int childIndex : children) DrawBoneHierarchyRecursive(childIndex, depth + 1);
            ImGui::TreePop();
        }
    }
    else {
        m_boneTreeOpenState[boneIndex] = false;
    }

    for (int i = 0; i < depth; ++i) ImGui::Unindent(16.0f);
}

void ModelRenderComponent::DrawBoneDetails(int boneIndex) {
    if (!m_model || boneIndex < 0 || boneIndex >= (int)m_model->bones.size()) return;

    auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };
    const auto& bone = m_model->bones[boneIndex];

    ImGui::Text("%s", SJ("選択中のボーン詳細:").c_str());
    ImGui::Separator();

    if (ImGui::BeginTable("BoneDetailsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("ボーン名").c_str());
        ImGui::TableSetColumnIndex(1); ImGui::Text("%s", bone.name.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("インデックス").c_str());
        ImGui::TableSetColumnIndex(1); ImGui::Text("%d", boneIndex);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("親インデックス").c_str());
        ImGui::TableSetColumnIndex(1);
        if (bone.parentIndex >= 0)
            ImGui::Text("%d (%s)", bone.parentIndex, m_model->bones[bone.parentIndex].name.c_str());
        else
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", SJ("なし(ルートボーン)").c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("ノードインデックス").c_str());
        ImGui::TableSetColumnIndex(1); ImGui::Text("%d", bone.nodeIndex);

        XMFLOAT3 worldPos = GetBoneWorldPosition(boneIndex);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("ワールド位置").c_str());
        ImGui::TableSetColumnIndex(1); ImGui::Text("(%.3f, %.3f, %.3f)", worldPos.x, worldPos.y, worldPos.z);

        XMFLOAT4 worldRot = GetBoneWorldRotationQuaternion(boneIndex);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("ワールド回転").c_str());
        ImGui::TableSetColumnIndex(1); ImGui::Text("(%.3f, %.3f, %.3f, %.3f)", worldRot.x, worldRot.y, worldRot.z, worldRot.w);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("オフセット行列").c_str());
        ImGui::TableSetColumnIndex(1);
        if (ImGui::TreeNode("OffsetMatrix")) {
            XMFLOAT4X4 offset;
            XMStoreFloat4x4(&offset, bone.offset);
            for (int row = 0; row < 4; ++row)
                ImGui::Text("[%.3f, %.3f, %.3f, %.3f]", offset.m[row][0], offset.m[row][1], offset.m[row][2], offset.m[row][3]);
            ImGui::TreePop();
        }

        if (boneIndex < (int)m_boneMatrices.size()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("現在のボーン行列").c_str());
            ImGui::TableSetColumnIndex(1);
            if (ImGui::TreeNode("CurrentMatrix")) {
                const auto& mat = m_boneMatrices[boneIndex];
                for (int row = 0; row < 4; ++row)
                    ImGui::Text("[%.3f, %.3f, %.3f, %.3f]", mat.m[row][0], mat.m[row][1], mat.m[row][2], mat.m[row][3]);
                ImGui::TreePop();
            }
        }

        ImGui::EndTable();
    }

    std::vector<int> children = GetBoneChildren(boneIndex);
    if (!children.empty()) {
        ImGui::Separator();
        auto SJ2 = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };
        std::string childLabel = SJ2("子ボーン") + " (" + std::to_string(children.size()) + ")";
        if (ImGui::TreeNode(childLabel.c_str())) {
            for (int childIdx : children) {
                std::string lbl = m_model->bones[childIdx].name + " [" + std::to_string(childIdx) + "]";
                if (ImGui::Selectable(lbl.c_str())) m_selectedBoneIndex = childIdx;
            }
            ImGui::TreePop();
        }
    }
}

void ModelRenderComponent::DrawForGBuffer(int Layer) {
    if (Layer != _LayerNumber) return;
    if (!m_ready || !m_model) return;

    AbstractScene* scene = _Parent->GetParentScene();
    if (!scene) return;
    CameraComponent* cam = scene->GetMainCamera();
    if (!cam) return;

    auto ctx = DirectX11::GetInstance()->GetContext();
    auto* sm = ShaderManager::GetInstance();

    ID3D11VertexShader* vs = sm->GetVertexShader(m_vsName);
    ID3D11PixelShader* ps = sm->GetPixelShader("PS_GBuffer");
    if (!vs || !ps) return;

    if (m_model->hasSkin && m_boneMatrices.empty()) EnsureDefaultBoneMatrices();

    XMMATRIX view = cam->GetView();
    XMMATRIX proj = cam->GetProjection();

    UINT stride = sizeof(ModelVertex);
    UINT offset = 0;
    ID3D11Buffer* vb = m_model->vb.Get();
    ID3D11Buffer* ib = m_model->ib.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetInputLayout(m_layout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->VSSetShader(vs, nullptr, 0);
    ctx->PSSetShader(ps, nullptr, 0);

    ID3D11SamplerState* smp = s_linearSmp.Get();
    ctx->PSSetSamplers(0, 1, &smp);

    if (m_model->hasSkin && m_useBoneMatrices && !m_boneMatrices.empty())
        SetupBoneMatricesForShader(ctx);

    EnsureDebugFallbackTextures();

    for (size_t i = 0; i < m_model->submeshes.size(); ++i) {
        const SubMesh& sm2 = m_model->submeshes[i];
        size_t matIndex = sm2.materialIndex;

        ID3D11ShaderResourceView* srv = s_whiteTexSRV.Get();
        XMFLOAT4 materialColor = m_color;
        XMFLOAT3 meshOffset(0.0f, 0.0f, 0.0f);
        XMFLOAT3 meshScale(1.0f, 1.0f, 1.0f);
        XMFLOAT3 meshRotation(0.0f, 0.0f, 0.0f);
        CullMode  cullMode = CullMode::Back;

        if (matIndex < m_materials.size()) {
            auto& mat = m_materials[matIndex];
            materialColor.x *= mat.color.x;
            materialColor.y *= mat.color.y;
            materialColor.z *= mat.color.z;
            materialColor.w *= mat.color.w;
            meshOffset = mat.meshOffset;
            meshScale = mat.meshScale;
            meshRotation = mat.meshRotation;
            cullMode = mat.cullMode;
            if (mat.tex && mat.tex->srv) srv = mat.tex->srv.Get();
            else if (!mat.texName.empty()) srv = s_magentaTexSRV.Get();
        }

        ID3D11RasterizerState* rs = nullptr;
        switch (cullMode) {
        case CullMode::Back:  rs = s_rasterizerCullBack.Get();  break;
        case CullMode::Front: rs = s_rasterizerCullFront.Get(); break;
        case CullMode::None:  rs = s_rasterizerCullNone.Get();  break;
        }
        if (rs) ctx->RSSetState(rs);

        XMMATRIX world = BuildMeshWorldMatrix(meshOffset, meshScale, meshRotation);
        CBData cbd;
        cbd.World = XMMatrixTranspose(world);
        cbd.View = XMMatrixTranspose(view);
        cbd.Proj = XMMatrixTranspose(proj);
        cbd.BaseColor = materialColor;

        ctx->UpdateSubresource(m_cb.Get(), 0, nullptr, &cbd, 0, 0);
        ID3D11Buffer* cbs[] = { m_cb.Get() };
        ctx->VSSetConstantBuffers(0, 1, cbs);
        ctx->PSSetConstantBuffers(0, 1, cbs);

        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->DrawIndexed(sm2.indexCount, sm2.indexOffset, 0);
    }

    ctx->RSSetState(s_rasterizerCullBack.Get());
}