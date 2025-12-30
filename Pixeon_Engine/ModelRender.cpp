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
	for (auto& m : m_model->materials) {
		MaterialRuntime rt;
		rt.texName = m.baseColorTex;
		rt.color = m.baseColor;
		rt.meshOffset = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		rt.meshScale = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
		rt.cullMode = CullMode::Back; // デフォルトは裏面カリング

		if (!m.baseColorTex.empty()) {
			if (m.isEmbedded && m.baseColorTex[0] == '*') {
				auto srv = ModelManager::Instance()->GetEmbeddedTexture(m_modelPath, m.baseColorTex);
				if (srv) {
					auto texRes = std::make_shared<TextureResource>();
					texRes->srv = srv;
					texRes->width = 0;
					texRes->height = 0;
					rt.tex = texRes;
				}
			}
			else {
				rt.tex = TextureManager::Instance()->LoadOrGet(m.baseColorTex);
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
		if (!vs || !ps) {
			std::string msg = "[ModelRenderComponent] シェーダ取得失敗: " + m_vsName + ", " + m_psName + "\n";
			OutputDebugStringA(msg.c_str());
			return false;
		}
	}

	bool vsChanged = (m_vs.Get() != vs);
	bool needLayout = forceRecreateLayout || vsChanged || !m_layout;

	m_vs = vs;
	m_ps = ps;

	if (needLayout) {
		RecreateInputLayout();
	}

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
	return true;
}

void ModelRenderComponent::RecreateInputLayout() {
	m_layout.Reset();
	const void* bc = nullptr;
	size_t bcSize = 0;
	if (!ShaderManager::GetInstance()->GetVSBytecode(m_vsName, &bc, &bcSize)) {
		OutputDebugStringA("[ModelRenderComponent] VS bytecode 取得失敗(InputLayout)\n");
		return;
	}
	EnsureInputLayout(bc, bcSize);
}

bool ModelRenderComponent::EnsureInputLayout(const void* vsBytecode, size_t size) {
	if (m_layout) return true;
	D3D11_INPUT_ELEMENT_DESC desc[] = {
		{ "POSITION",0, DXGI_FORMAT_R32G32B32_FLOAT,    0,(UINT)offsetof(ModelVertex,position),    D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "NORMAL",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0,(UINT)offsetof(ModelVertex,normal),      D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,(UINT)offsetof(ModelVertex,tangent),     D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "TEXCOORD",0, DXGI_FORMAT_R32G32_FLOAT,       0,(UINT)offsetof(ModelVertex,uv),          D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "BLENDINDICES",0, DXGI_FORMAT_R32G32B32A32_UINT, 0,(UINT)offsetof(ModelVertex,boneIndices), D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,0,(UINT)offsetof(ModelVertex,boneWeights), D3D11_INPUT_PER_VERTEX_DATA,0 },
	};
	auto dev = DirectX11::GetInstance()->GetDevice();
	HRESULT hr = dev->CreateInputLayout(desc, _countof(desc), vsBytecode, size, m_layout.GetAddressOf());
	if (FAILED(hr)) {
		OutputDebugStringA("[ModelRenderComponent] InputLayout 作成失敗\n");
		return false;
	}
	return true;
}

bool ModelRenderComponent::EnsureConstantBuffer() {
	if (m_cb) return true;
	auto dev = DirectX11::GetInstance()->GetDevice();
	D3D11_BUFFER_DESC bd{};
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = sizeof(CBData);
	bd.Usage = D3D11_USAGE_DEFAULT;
	HRESULT hr = dev->CreateBuffer(&bd, nullptr, m_cb.GetAddressOf());
	if (FAILED(hr)) {
		OutputDebugStringA("[ModelRenderComponent] 定数バッファ作成失敗\n");
		return false;
	}
	return true;
}

DirectX::XMMATRIX ModelRenderComponent::BuildWorldMatrix() const {
	Transform t = _Parent->GetTransform();
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
		D3D11_SUBRESOURCE_DATA init{ &pixel,4,4 };
		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
		if (SUCCEEDED(dev->CreateTexture2D(&td, &init, tex.GetAddressOf())))
			dev->CreateShaderResourceView(tex.Get(), nullptr, s_whiteTexSRV.GetAddressOf());
	}
	if (!s_magentaTexSRV) {
		uint8_t pix[4] = { 255,255,255,255 };
		D3D11_TEXTURE2D_DESC td{};
		td.Width = td.Height = 1;
		td.MipLevels = 1; td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		D3D11_SUBRESOURCE_DATA init{ pix,4,4 };
		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
		if (SUCCEEDED(dev->CreateTexture2D(&td, &init, tex.GetAddressOf())))
			dev->CreateShaderResourceView(tex.Get(), nullptr, s_magentaTexSRV.GetAddressOf());
	}
	return (s_whiteTexSRV && s_magentaTexSRV);
}

bool ModelRenderComponent::EnsureRasterizerStates() {
	auto dev = DirectX11::GetInstance()->GetDevice();
	if (!dev) return false;

	// 裏面カリング（デフォルト）
	if (!s_rasterizerCullBack) {
		D3D11_RASTERIZER_DESC rd{};
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = D3D11_CULL_BACK;
		rd.FrontCounterClockwise = FALSE;
		rd.DepthClipEnable = TRUE;
		if (FAILED(dev->CreateRasterizerState(&rd, s_rasterizerCullBack.GetAddressOf()))) {
			OutputDebugStringA("[ModelRenderComponent] RasterizerState(CullBack) 作成失敗\n");
			return false;
		}
	}

	// 表面カリング
	if (!s_rasterizerCullFront) {
		D3D11_RASTERIZER_DESC rd{};
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = D3D11_CULL_FRONT;
		rd.FrontCounterClockwise = FALSE;
		rd.DepthClipEnable = TRUE;
		if (FAILED(dev->CreateRasterizerState(&rd, s_rasterizerCullFront.GetAddressOf()))) {
			OutputDebugStringA("[ModelRenderComponent] RasterizerState(CullFront) 作成失敗\n");
			return false;
		}
	}

	// カリングなし（両面描画）
	if (!s_rasterizerCullNone) {
		D3D11_RASTERIZER_DESC rd{};
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = D3D11_CULL_NONE;
		rd.FrontCounterClockwise = FALSE;
		rd.DepthClipEnable = TRUE;
		if (FAILED(dev->CreateRasterizerState(&rd, s_rasterizerCullNone.GetAddressOf()))) {
			OutputDebugStringA("[ModelRenderComponent] RasterizerState(CullNone) 作成失敗\n");
			return false;
		}
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
	std::string detail;

	if (sm.materialIndex >= m_materials.size()) {
		issue = TextureIssue::MaterialIndexOutOfRange;
		detail = "submesh. materialIndex=" + std::to_string(sm.materialIndex);
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
				else if (sm.uvAllZero) { issue = TextureIssue::UVAllZero; detail = "All UV zero"; }
			}
		}
	}
	if (issue == TextureIssue::None && !s_linearSmp) {
		issue = TextureIssue::SamplerMissing; detail = "Sampler missing";
	}
	if (issue == TextureIssue::None) {
		if (usedMagentaFallback) { issue = TextureIssue::StillFallbackMagenta; detail = "Magenta fallback"; }
		else if (usedWhiteFallback) { issue = TextureIssue::StillFallbackWhite; detail = "White fallback"; }
	}

	if (issue != TextureIssue::None) {
		m_texIssueReported[submeshIdx] = 1;
		static const char* issueNames[] = {
			"None","MaterialIndexOutOfRange","MaterialNoPath","TextureLoadFailed","TextureSRVNull",
			"NoUVChannel","UVAllZero","SamplerMissing","StillFallbackWhite","StillFallbackMagenta"
		};
		std::string msg = "SubMesh " + std::to_string(submeshIdx) +
			" Issue=" + issueNames[(int)issue] + " | " + detail;
		if (mat && !mat->texName.empty()) msg += " | path=" + mat->texName;
	}
}

DirectX::XMMATRIX ModelRenderComponent::BuildMeshWorldMatrix(const DirectX::XMFLOAT3& offset, const DirectX::XMFLOAT3& scale) const {
	Transform t = _Parent->GetTransform();

	XMMATRIX S = XMMatrixScaling(
		t.scale.x * scale.x,
		t.scale.y * scale.y,
		t.scale.z * scale.z
	);

	XMMATRIX R = XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z);

	XMMATRIX T = XMMatrixTranslation(
		t.position.x + offset.x,
		t.position.y + offset.y,
		t.position.z + offset.z
	);

	return S * R * T;
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

void ModelRenderComponent::SetMeshCullMode(size_t meshIndex, CullMode mode) {
	if (meshIndex >= m_materials.size()) return;
	m_materials[meshIndex].cullMode = mode;
}

ModelRenderComponent::CullMode ModelRenderComponent::GetMeshCullMode(size_t meshIndex) const {
	if (meshIndex >= m_materials.size()) return CullMode::Back;
	return m_materials[meshIndex].cullMode;
}

void ModelRenderComponent::EnsureDefaultBoneMatrices()
{
	if (!m_model) return;
	if (!m_model->hasSkin) return;
	size_t required = m_model->bones.size();
	if (required == 0) {
		required = 1;
	}
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
	Scene* scene = _Parent->GetParentScene();
	if (!scene) return;
	CameraComponent* cam = scene->GetMainCamera();
	if (!cam) return;

	if (!m_vs || !m_ps) {
		if (!EnsureShaders(false)) return;
	}
	if (m_model->hasSkin && m_boneMatrices.empty()) {
		EnsureDefaultBoneMatrices();
	}

	XMMATRIX view = cam->GetView();
	XMMATRIX proj = cam->GetProjection();

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

	if (m_model->hasSkin && m_useBoneMatrices && !m_boneMatrices.empty()) {
		SetupBoneMatricesForShader(ctx);
	}

	EnsureDebugFallbackTextures();

	for (size_t i = 0; i < m_model->submeshes.size(); ++i) {
		const SubMesh& sm = m_model->submeshes[i];
		size_t matIndex = sm.materialIndex;

		ID3D11ShaderResourceView* srv = s_whiteTexSRV.Get();
		DirectX::XMFLOAT4 materialColor = m_color;
		DirectX::XMFLOAT3 meshOffset(0.0f, 0.0f, 0.0f);
		DirectX::XMFLOAT3 meshScale(1.0f, 1.0f, 1.0f);
		CullMode cullMode = CullMode::Back;
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

		// カリングモードに応じたラスタライザーステートを設定
		ID3D11RasterizerState* rasterizerState = nullptr;
		switch (cullMode) {
		case CullMode::Back:
			rasterizerState = s_rasterizerCullBack.Get();
			break;
		case CullMode::Front:
			rasterizerState = s_rasterizerCullFront.Get();
			break;
		case CullMode::None:
			rasterizerState = s_rasterizerCullNone.Get();
			break;
		}
		if (rasterizerState) {
			ctx->RSSetState(rasterizerState);
		}

		XMMATRIX world = BuildMeshWorldMatrix(meshOffset, meshScale);

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
		ctx->DrawIndexed(sm.indexCount, sm.indexOffset, 0);

		if (usedWhite || usedMagenta) {
			DiagnoseAndReportTextureIssue(i, sm, matPtr, srv, usedMagenta, usedWhite);
		}
	}

	// デフォルトのラスタライザーステートに戻す
	ctx->RSSetState(s_rasterizerCullBack.Get());
}

void ModelRenderComponent::SetupBoneMatricesForShader(ID3D11DeviceContext* ctx)
{
	static Microsoft::WRL::ComPtr<ID3D11Buffer> s_boneCB;
	if (!s_boneCB) {
		D3D11_BUFFER_DESC bd{};
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.ByteWidth = sizeof(DirectX::XMFLOAT4X4) * 256;
		bd.Usage = D3D11_USAGE_DEFAULT;
		auto dev = DirectX11::GetInstance()->GetDevice();
		if (FAILED(dev->CreateBuffer(&bd, nullptr, s_boneCB.GetAddressOf()))) {
			return;
		}
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

	static int uploadCounter = 0;
	if (++uploadCounter % 240 == 0 && count > 0) {
		std::string log = "[BoneUpload] count=" + std::to_string(count);
		int show = std::min<int>((int)count, 3);
		for (int i = 0; i < show; ++i) {
			auto& m = m_boneMatrices[i];
			log += " b" + std::to_string(i) + "T(" + std::to_string(m._41) + "," + std::to_string(m._42) + "," + std::to_string(m._43) + ")";
		}
	}
}

bool ModelRenderComponent::SetMaterialTexture(int materialIndex, const std::string& texLogicalPath)
{
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

	out << m_materials.size() << "\n";
	for (const auto& mat : m_materials) {
		out << mat.texName << "\n";
		out << mat.meshOffset.x << " " << mat.meshOffset.y << " " << mat.meshOffset.z << "\n";
		out << mat.meshScale.x << " " << mat.meshScale.y << " " << mat.meshScale.z << "\n";
		out << static_cast<int>(mat.cullMode) << "\n";
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

	SetModel(m_modelPath);

	size_t matCount = 0;
	in >> matCount;
	in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

	for (size_t i = 0; i < matCount && i < m_materials.size(); ++i) {
		std::string texName;
		std::getline(in, texName);

		DirectX::XMFLOAT3 offset;
		in >> offset.x >> offset.y >> offset.z;
		in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

		DirectX::XMFLOAT3 scale;
		in >> scale.x >> scale.y >> scale.z;
		in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

		int cullModeInt;
		in >> cullModeInt;
		in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

		if (!texName.empty() && texName != m_materials[i].texName) {
			SetMaterialTexture(i, texName);
		}

		m_materials[i].meshOffset = offset;
		m_materials[i].meshScale = scale;
		m_materials[i].cullMode = static_cast<CullMode>(cullModeInt);
	}
}

void ModelRenderComponent::SetBoneMatrices(const std::vector<DirectX::XMFLOAT4X4>& matrices)
{
	m_boneMatrices = matrices;
	m_useBoneMatrices = !matrices.empty();

#ifdef _DEBUG
	static int debugCounter = 0;
	if (++debugCounter % 60 == 0) {
		OutputDebugStringA(("[ModelRender] Updated " +
			std::to_string(matrices.size()) + " bone matrices\n").c_str());
	}
#endif
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
	if (0 > _LayerNumber) _LayerNumber = 0;
	if (10 <= _LayerNumber) _LayerNumber = 9;
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("モデル名").c_str());
	ImGui::TableSetColumnIndex(1); ImGui::Text("%s", m_modelPath.c_str());

	ImGui::TableNextRow();

	static char pathBuf[256];
	std::snprintf(pathBuf, sizeof(pathBuf), "%s", m_modelPath.c_str());
	ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("モデルの設定").c_str());
	ImGui::TableSetColumnIndex(1);
	if (ImGui::Button(SJ("再読込/適用").c_str())) SetModel(pathBuf);
	ImGui::SameLine();
	if (ImGui::Button(SJ("モデル選択").c_str())) ImGui::OpenPopup("ModelSelectPopup");
	ShowModelSelectPopup();

	ImGui::TableNextRow();

	auto* sm = ShaderManager::GetInstance();
	static std::vector<std::string> vsList;
	static std::vector<std::string> psList;
	if (vsList.empty()) vsList = sm->GetShaderList("VS");
	if (psList.empty()) psList = sm->GetShaderList("PS");
	ImGui::TableSetColumnIndex(0); ImGui::Text("%s", SJ("シェーダーの再読込").c_str());
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
	ImGui::TableSetColumnIndex(0);
	ImGui::EndTable();

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
				float offset[3] = {
					m_materials[i].meshOffset.x,
					m_materials[i].meshOffset.y,
					m_materials[i].meshOffset.z
				};
				if (ImGui::DragFloat3("Offset", offset, 0.01f, -100.0f, 100.0f)) {
					m_materials[i].meshOffset.x = offset[0];
					m_materials[i].meshOffset.y = offset[1];
					m_materials[i].meshOffset.z = offset[2];
				}

				ImGui::Text("%s", SJ("メッシュスケール").c_str());
				float scale[3] = {
					m_materials[i].meshScale.x,
					m_materials[i].meshScale.y,
					m_materials[i].meshScale.z
				};
				if (ImGui::DragFloat3("Scale", scale, 0.01f, 0.01f, 10.0f)) {
					m_materials[i].meshScale.x = scale[0];
					m_materials[i].meshScale.y = scale[1];
					m_materials[i].meshScale.z = scale[2];
				}

				ImGui::Separator();
				ImGui::Text("%s", SJ("カリングモード").c_str());

				// 文字列を事前に変数として保持してダングリングポインタを回避
				std::string cullBack = SJ("裏面カリング(通常)");
				std::string cullFront = SJ("表面カリング");
				std::string cullNone = SJ("両面描画");
				const char* cullModeNames[] = {
					cullBack.c_str(),
					cullFront.c_str(),
					cullNone.c_str()
				};

				int currentCullMode = static_cast<int>(m_materials[i].cullMode);
				if (ImGui::Combo("CullMode", &currentCullMode, cullModeNames, 3)) {
					m_materials[i].cullMode = static_cast<CullMode>(currentCullMode);
				}

				if (ImGui::Button(SJ("オフセットリセット").c_str())) {
					m_materials[i].meshOffset = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
				}
				ImGui::SameLine();
				if (ImGui::Button(SJ("スケールリセット").c_str())) {
					m_materials[i].meshScale = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	if (m_openTexPopup) {
		ImGui::OpenPopup("TextureSelectPopup");
	}

	if (ImGui::BeginPopupModal("TextureSelectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		static char filter[128] = "";
		ImGui::InputText(SJ("フィルタ").c_str(), filter, sizeof(filter));

		auto list = AssetManager::Instance()->GetCachedTextureNames();
		ImGui::Text("%s:  %zu", SJ("総数").c_str(), list.size());
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

void ModelRenderComponent::ShowModelSelectPopup()
{
	if (ImGui::BeginPopupModal("ModelSelectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
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

		for (int i = 0; i < (int)list.size(); ++i)
		{
			const std::string& rawName = list[i];
			if (filter[0] && rawName.find(filter) == std::string::npos) continue;

			std::string dispName = GUI::GetInstance()->ShiftJISToUTF8(rawName.c_str());
			bool selected = (currentHighlight == i);
			if (ImGui::Selectable(dispName.c_str(), selected))
			{
				currentHighlight = i;
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					if (SetModel(rawName)) {
						m_modelPath = rawName;
					}
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
		if (ImGui::Button(SJ("適用").c_str()))
		{
			if (currentHighlight >= 0 && currentHighlight < (int)list.size()) {
				const std::string& sel = list[currentHighlight];
				if (SetModel(sel)) m_modelPath = sel;
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button(SJ("キャンセル").c_str())) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button(SJ("再ロード").c_str())) {
			if (!m_modelPath.empty()) SetModel(m_modelPath);
		}

		ImGui::EndPopup();
	}
}

void ModelRenderComponent::ShowTextureSelectPopup(int materialIndex)
{
	if (!m_openTexPopup || materialIndex < 0 || materialIndex >= (int)m_materials.size())
		return;
	if (ImGui::BeginPopupModal("TextureSelectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };

		static char filter[128] = "";
		ImGui::InputText(SJ("フィルタ").c_str(), filter, sizeof(filter));

		auto list = AssetManager::Instance()->GetCachedTextureNames();

		ImGui::Text("%s: %zu", SJ("総数").c_str(), list.size());
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