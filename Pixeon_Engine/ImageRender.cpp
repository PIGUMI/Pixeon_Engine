#define NOMINMAX
#include "ImageRender.h"
#include "GameRenderTarget.h"
#include "SettingManager.h"
// static
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ImageRender::s_whiteTexSRV;
Microsoft::WRL::ComPtr<ID3D11SamplerState>       ImageRender::s_linearSmp;
Microsoft::WRL::ComPtr<ID3D11BlendState> ImageRender::s_alphaBlendState;
Microsoft::WRL::ComPtr<ID3D11DepthStencilState> ImageRender::s_depthStencilState;

ImageRender::ImageRender() {
	_ComponentName = "ImageRender";
	_Type = ComponentManager::COMPONENT_TYPE::IMAGE;
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

void ImageRender::SaveToFile(std::ostream& out) {
	out << m_textureName << std::endl;
	out << (int)m_mode << std::endl;
	out << m_size2D.x << " " << m_size2D.y << std::endl;
	out << m_sizeWorld.x << " " << m_sizeWorld.y << std::endl;
	out << m_offset2D.x << " " << m_offset2D.y << std::endl;
	out << m_offset3D.x << " " << m_offset3D.y << " " << m_offset3D.z << std::endl;
	// m_offsetRot を XMFLOAT3 として保存 (deg)
	out << m_offsetRot.x << " " << m_offsetRot.y << " " << m_offsetRot.z << std::endl;
	out << m_uvRect.x << " " << m_uvRect.y << " " << m_uvRect.z << " " << m_uvRect.w << std::endl;
	out << m_color.x << " " << m_color.y << " " << m_color.z << " " << m_color.w << std::endl;
}

void ImageRender::LoadFromFile(std::istream& in) {
	int mode = 0;
	in >> m_textureName;
	in >> mode; m_mode = (PlacementMode)mode;
	in >> m_size2D.x >> m_size2D.y;
	in >> m_sizeWorld.x >> m_sizeWorld.y;
	in >> m_offset2D.x >> m_offset2D.y;
	in >> m_offset3D.x >> m_offset3D.y >> m_offset3D.z;
	// m_offsetRot を XMFLOAT3 として読み込み (deg)
	in >> m_offsetRot.x >> m_offsetRot.y >> m_offsetRot.z;
	in >> m_uvRect.x >> m_uvRect.y >> m_uvRect.z >> m_uvRect.w;
	in >> m_color.x >> m_color.y >> m_color.z >> m_color.w;
	if (!m_textureName.empty()) {
		m_texture = ResourceService::Instance().GetTexture(m_textureName);
	}
}

void ImageRender::SetCamera(CameraComponent* ptr)
{
	cam = ptr;
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

void ImageRender::InGameUpdate()
{
	Update();
}

void ImageRender::EditUpdate()
{
	Update();
}

void ImageRender::Update()
{
	Scene* scene = _Parent ? _Parent->GetParentScene() : nullptr;
	if (scene)
	{
		cam = scene ? scene->GetMainCamera() : nullptr;
	}
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

bool ImageRender::EnsureBlendState() {
	if (s_alphaBlendState) return true;
	D3D11_BLEND_DESC desc = {};
	desc.RenderTarget[0].BlendEnable = TRUE;

	// 以下の設定に変更
	desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;  // SRC_ALPHA → ONE に変更
	desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

	// アルファチャンネルのブレンド設定も変更
	desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;  // ZERO → INV_SRC_ALPHA に変更
	desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	auto dev = DirectX11::GetInstance()->GetDevice();
	HRESULT hr = dev->CreateBlendState(&desc, s_alphaBlendState.GetAddressOf());
	return SUCCEEDED(hr);
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

bool ImageRender::EnsureDepthStencilState() {
	if (s_depthStencilState) return true;
	D3D11_DEPTH_STENCIL_DESC desc = {};
	desc.DepthEnable = FALSE; // 2DではこれでOK
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.StencilEnable = FALSE;
	auto dev = DirectX11::GetInstance()->GetDevice();
	HRESULT hr = dev->CreateDepthStencilState(&desc, s_depthStencilState.GetAddressOf());
	return SUCCEEDED(hr);
}

// VB更新

void ImageRender::UpdateVB(const Vertex v[4]) {
	auto ctx = DirectX11::GetInstance()->GetContext();
	D3D11_MAPPED_SUBRESOURCE mp{};
	if (SUCCEEDED(ctx->Map(m_vb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp))) {
		memcpy(mp.pData, v, sizeof(Vertex) * 4);
		ctx->Unmap(m_vb.Get(), 0);
	}
}

void ImageRender::UpdateVertices2D(Vertex outV[4], float& outZClip) {
	// 2D (スクリーン) は基本的に Z (roll) 回転を使う（deg -> rad）
	float radZ = DirectX::XMConvertToRadians(-m_offsetRot.z); // 以前の向きと互換を保つため符号反転
	float c = cosf(radZ), s = sinf(radZ);

	if (!cam) {
		float W = (float)DirectX11::GetInstance()->GetDefaultRTV()->GetWidth();
		float H = (float)DirectX11::GetInstance()->GetDefaultRTV()->GetHeight();

		float cx = W * 0.5f + m_offset2D.x;
		float cy = H * 0.5f + m_offset2D.y;

		float hw = m_size2D.x * 0.5f;
		float hh = m_size2D.y * 0.5f;

		// 四隅の相対オフセット (px)
		DirectX::XMFLOAT2 offs[4] = {
			{-hw, -hh},
			{ hw, -hh},
			{ hw,  hh},
			{-hw,  hh},
		};

		auto toNDC = [&](float x, float y)->DirectX::XMFLOAT2 {
			float ndcX = (x / W) * 2.0f - 1.0f;
			float ndcY = 1.0f - (y / H) * 2.0f;
			return { ndcX, ndcY };
			};

		for (int i = 0; i < 4; ++i) {
			// Z 回転のみ適用
			float dx = offs[i].x;
			float dy = offs[i].y;
			float rx = dx * c - dy * s;
			float ry = dx * s + dy * c;
			float sx = cx + rx;
			float sy = cy + ry;
			DirectX::XMFLOAT2 ndc = toNDC(sx, sy);
			outV[i].pos = { ndc.x, ndc.y, 0.0f };
		}

		outV[0].uv = { m_uvRect.x, m_uvRect.y };
		outV[1].uv = { m_uvRect.z, m_uvRect.y };
		outV[2].uv = { m_uvRect.z, m_uvRect.w };
		outV[3].uv = { m_uvRect.x, m_uvRect.w };

		outZClip = 0.0f;
		return;
	}

	DirectX::XMMATRIX V = cam->GetView();
	DirectX::XMMATRIX P = cam->GetProjection();

	Transform t = _Parent->GetTransform();
	DirectX::XMVECTOR posW = DirectX::XMVectorSet(t.position.x, t.position.y, t.position.z, 1.0f);

	DirectX::XMVECTOR clip = DirectX::XMVector4Transform(DirectX::XMVector4Transform(posW, V), P);
	DirectX::XMFLOAT4 clipF; DirectX::XMStoreFloat4(&clipF, clip);
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

	DirectX::XMFLOAT2 offs2[4] = {
		{-hw, -hh},
		{ hw, -hh},
		{ hw,  hh},
		{-hw,  hh},
	};

	auto toNDC2 = [&](float x, float y)->DirectX::XMFLOAT2 {
		float ndcX2 = (x / W) * 2.0f - 1.0f;
		float ndcY2 = 1.0f - (y / H) * 2.0f;
		return { ndcX2, ndcY2 };
		};

	for (int i = 0; i < 4; ++i) {
		float dx = offs2[i].x;
		float dy = offs2[i].y;
		float rx = dx * c - dy * s;
		float ry = dx * s + dy * c;
		float sx = px + rx;
		float sy = py + ry;
		DirectX::XMFLOAT2 ndc = toNDC2(sx, sy);
		outV[i].pos = { ndc.x, ndc.y, outZClip };
	}

	outV[0].uv = { m_uvRect.x, m_uvRect.y };
	outV[1].uv = { m_uvRect.z, m_uvRect.y };
	outV[2].uv = { m_uvRect.z, m_uvRect.w };
	outV[3].uv = { m_uvRect.x, m_uvRect.w };
}

void ImageRender::UpdateVerticesBillboard(Vertex outV[4]) {
	if (!cam) {
		float dummyZ = 0.0f;
		UpdateVertices2D(outV, dummyZ);
		return;
	}
	DirectX::XMMATRIX V = cam->GetView();
	DirectX::XMMATRIX invV = DirectX::XMMatrixInverse(nullptr, V);

	DirectX::XMFLOAT3 right, up;
	right = DirectX::XMFLOAT3(invV.r[0].m128_f32[0], invV.r[0].m128_f32[1], invV.r[0].m128_f32[2]);
	up = DirectX::XMFLOAT3(invV.r[1].m128_f32[0], invV.r[1].m128_f32[1], invV.r[1].m128_f32[2]);

	DirectX::XMVECTOR vRight = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&right));
	DirectX::XMVECTOR vUp = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&up));

	// m_offsetRot (deg) -> ラジアン、符号は以前と互換性を保つため反転
	float pitch = DirectX::XMConvertToRadians(-m_offsetRot.x);
	float yaw = DirectX::XMConvertToRadians(-m_offsetRot.y);
	float roll = DirectX::XMConvertToRadians(-m_offsetRot.z);
	DirectX::XMMATRIX rotM = DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

	DirectX::XMVECTOR rRot = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(vRight, rotM));
	DirectX::XMVECTOR uRot = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(vUp, rotM));

	Transform t = _Parent->GetTransform();
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z);
	DirectX::XMVECTOR off = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_offset3D), R);
	DirectX::XMVECTOR center = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&t.position), off);

	float hw = m_sizeWorld.x * 0.5f;
	float hh = m_sizeWorld.y * 0.5f;

	DirectX::XMVECTOR tl = DirectX::XMVectorAdd(
		DirectX::XMVectorSubtract(center, DirectX::XMVectorMultiply(rRot, DirectX::XMVectorReplicate(hw))),
		DirectX::XMVectorMultiply(uRot, DirectX::XMVectorReplicate(hh))
	);
	DirectX::XMVECTOR tr = DirectX::XMVectorAdd(
		DirectX::XMVectorAdd(center, DirectX::XMVectorMultiply(rRot, DirectX::XMVectorReplicate(hw))),
		DirectX::XMVectorMultiply(uRot, DirectX::XMVectorReplicate(hh))
	);
	DirectX::XMVECTOR br = DirectX::XMVectorSubtract(
		DirectX::XMVectorAdd(center, DirectX::XMVectorMultiply(rRot, DirectX::XMVectorReplicate(hw))),
		DirectX::XMVectorMultiply(uRot, DirectX::XMVectorReplicate(hh))
	);
	DirectX::XMVECTOR bl = DirectX::XMVectorSubtract(
		DirectX::XMVectorSubtract(center, DirectX::XMVectorMultiply(rRot, DirectX::XMVectorReplicate(hw))),
		DirectX::XMVectorMultiply(uRot, DirectX::XMVectorReplicate(hh))
	);

	DirectX::XMFLOAT3 f;
	DirectX::XMStoreFloat3(&f, tl); outV[0].pos = f; outV[0].uv = { m_uvRect.x, m_uvRect.y };
	DirectX::XMStoreFloat3(&f, tr); outV[1].pos = f; outV[1].uv = { m_uvRect.z, m_uvRect.y };
	DirectX::XMStoreFloat3(&f, br); outV[2].pos = f; outV[2].uv = { m_uvRect.z, m_uvRect.w };
	DirectX::XMStoreFloat3(&f, bl); outV[3].pos = f; outV[3].uv = { m_uvRect.x, m_uvRect.w };
}

void ImageRender::UpdateVerticesWorld3D(Vertex outV[4]) {
	Transform t = _Parent->GetTransform();
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z);

	// m_offsetRot をローカル回転として適用 (deg -> rad), 符号反転して互換性を保つ
	float pitch = DirectX::XMConvertToRadians(-m_offsetRot.x);
	float yaw = DirectX::XMConvertToRadians(-m_offsetRot.y);
	float roll = DirectX::XMConvertToRadians(-m_offsetRot.z);
	DirectX::XMMATRIX rotM = DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

	// オブジェクト回転にオフセット回転を乗算（オブジェクト回転 -> オフセット回転）
	DirectX::XMMATRIX ROffset = DirectX::XMMatrixMultiply(R, rotM);

	DirectX::XMVECTOR vRight = DirectX::XMVector3Normalize(ROffset.r[0]);
	DirectX::XMVECTOR vUp = DirectX::XMVector3Normalize(ROffset.r[1]);

	DirectX::XMVECTOR off = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&m_offset3D), R);
	DirectX::XMVECTOR center = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&t.position), off);

	float hw = m_sizeWorld.x * 0.5f;
	float hh = m_sizeWorld.y * 0.5f;

	DirectX::XMVECTOR tl = DirectX::XMVectorAdd(
		DirectX::XMVectorSubtract(center, DirectX::XMVectorMultiply(vRight, DirectX::XMVectorReplicate(hw))),
		DirectX::XMVectorMultiply(vUp, DirectX::XMVectorReplicate(hh))
	);
	DirectX::XMVECTOR tr = DirectX::XMVectorAdd(
		DirectX::XMVectorAdd(center, DirectX::XMVectorMultiply(vRight, DirectX::XMVectorReplicate(hw))),
		DirectX::XMVectorMultiply(vUp, DirectX::XMVectorReplicate(hh))
	);
	DirectX::XMVECTOR br = DirectX::XMVectorSubtract(
		DirectX::XMVectorAdd(center, DirectX::XMVectorMultiply(vRight, DirectX::XMVectorReplicate(hw))),
		DirectX::XMVectorMultiply(vUp, DirectX::XMVectorReplicate(hh))
	);
	DirectX::XMVECTOR bl = DirectX::XMVectorSubtract(
		DirectX::XMVectorSubtract(center, DirectX::XMVectorMultiply(vRight, DirectX::XMVectorReplicate(hw))),
		DirectX::XMVectorMultiply(vUp, DirectX::XMVectorReplicate(hh))
	);

	DirectX::XMFLOAT3 f;
	DirectX::XMStoreFloat3(&f, tl); outV[0].pos = f; outV[0].uv = { m_uvRect.x, m_uvRect.y };
	DirectX::XMStoreFloat3(&f, tr); outV[1].pos = f; outV[1].uv = { m_uvRect.z, m_uvRect.y };
	DirectX::XMStoreFloat3(&f, br); outV[2].pos = f; outV[2].uv = { m_uvRect.z, m_uvRect.w };
	DirectX::XMStoreFloat3(&f, bl); outV[3].pos = f; outV[3].uv = { m_uvRect.x, m_uvRect.w };
}

void ImageRender::UpdateVerticesUI(Vertex outV[4])
{
	if (!cam) {
		// カメラがない場合は 2D と同様 (既存挙動)
		float dummyZ = 0.0f;
		UpdateVertices2D(outV, dummyZ);
		return;
	}

	// View / inverse(View) / Projection を取得
	DirectX::XMMATRIX V = cam->GetView();
	DirectX::XMMATRIX invV = DirectX::XMMatrixInverse(nullptr, V);
	DirectX::XMMATRIX P = cam->GetProjection();

	// UI をカメラ前方の固定距離に配置
	const float uiDistance = 3.0f;
	// ビュー空間での中心点 (ビュー空間座標)
	DirectX::XMVECTOR centerView = DirectX::XMVectorSet(0.0f, 0.0f, uiDistance, 1.0f);

	// スクリーンサイズ（ピクセル）
	float W = (float)DirectX11::GetInstance()->GetDefaultRTV()->GetWidth();
	float H = (float)DirectX11::GetInstance()->GetDefaultRTV()->GetHeight();
	if (W <= 0.0f || H <= 0.0f) {
		// 退避: おかしな値なら 2D と同様に処理
		float dummyZ = 0.0f;
		UpdateVertices2D(outV, dummyZ);
		return;
	}

	// カメラの Projection から垂直 FOV を復元して、
	// 距離 uiDistance におけるビュー空間の垂直サイズを計算する
	// DirectX の一般的な Perspective 行列では m._22 = cot(fovY/2)
	float p22 = P.r[1].m128_f32[1];
	if (p22 == 0.0f) p22 = 1.0f;
	float fovY = 2.0f * atanf(1.0f / p22);
	float viewHeightAtDist = 2.0f * uiDistance * tanf(fovY * 0.5f);
	float aspect = W / H;
	float viewWidthAtDist = viewHeightAtDist * aspect;

	// UI モードでは Inspector で指定する m_size2D (px) を使って、
	// 距離 uiDistance におけるワールド単位の半幅・半高を算出する
	float worldPerPixelY = viewHeightAtDist / H;
	float worldPerPixelX = viewWidthAtDist / W;

	float halfW_viewUnits = (m_size2D.x * 0.5f) * worldPerPixelX;
	float halfH_viewUnits = (m_size2D.y * 0.5f) * worldPerPixelY;

	// 回転行列（ビュー空間での回転）
	// m_offsetRot は度で保持している前提。符号は従来の互換に合わせて反転している点に注意。
	float pitch = DirectX::XMConvertToRadians(-m_offsetRot.x);
	float yaw = DirectX::XMConvertToRadians(-m_offsetRot.y);
	float roll = DirectX::XMConvertToRadians(-m_offsetRot.z);
	DirectX::XMMATRIX rotView = DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

	// ビュー空間での四隅（Y up）
	DirectX::XMVECTOR cornersView[4];
	// top-left, top-right, bottom-right, bottom-left (ビュー空間の Y は上方向)
	cornersView[0] = DirectX::XMVectorSet(-halfW_viewUnits, halfH_viewUnits, 0.0f, 0.0f); // TL
	cornersView[1] = DirectX::XMVectorSet(halfW_viewUnits, halfH_viewUnits, 0.0f, 0.0f); // TR
	cornersView[2] = DirectX::XMVectorSet(halfW_viewUnits, -halfH_viewUnits, 0.0f, 0.0f); // BR
	cornersView[3] = DirectX::XMVectorSet(-halfW_viewUnits, -halfH_viewUnits, 0.0f, 0.0f); // BL

	for (int i = 0; i < 4; ++i) {
		// ビュー空間で回転を適用 (回転はビュー空間ベースなのでカメラ回転に影響されない)
		DirectX::XMVECTOR rotated = DirectX::XMVector3TransformNormal(cornersView[i], rotView);
		// 中心に足してビュー空間上の座標を得る
		DirectX::XMVECTOR posView = DirectX::XMVectorAdd(centerView, rotated);
		// ビュー空間 -> ワールド空間に変換
		DirectX::XMVECTOR posWorld = DirectX::XMVector3TransformCoord(posView, invV);
		// 書き出し
		DirectX::XMFLOAT3 f;
		DirectX::XMStoreFloat3(&f, posWorld);
		outV[i].pos = f;
	}

	// UV は通常どおり
	outV[0].uv = { m_uvRect.x, m_uvRect.y };
	outV[1].uv = { m_uvRect.z, m_uvRect.y };
	outV[2].uv = { m_uvRect.z, m_uvRect.w };
	outV[3].uv = { m_uvRect.x, m_uvRect.w };
}

void ImageRender::Draw() {
	if (!m_ready) return;
	auto* dx = DirectX11::GetInstance();
	auto ctx = dx->GetContext();
	if (!ctx) return;

	if (!EnsureShaders(false)) return;
	if (!EnsureConstantBuffer()) return;
	if (!EnsureBuffers()) return;
	if (!EnsureBlendState()) return;
	if (!EnsureDepthStencilState()) return;

	// ブレンドステートとDepthStencilStateを先に設定
	float blendFactor[4] = { 0,0,0,0 };
	UINT sampleMask = 0xFFFFFFFF;
	ctx->OMSetBlendState(s_alphaBlendState.Get(), blendFactor, sampleMask);
	ctx->OMSetDepthStencilState(s_depthStencilState.Get(), 0);

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
	else if (m_mode == PlacementMode::UI) {
		UpdateVerticesUI(v);
		mode2DFlag = 0;
	}
	else {
		UpdateVerticesWorld3D(v);
		mode2DFlag = 0;
	}

	UpdateVB(v);

	Scene* scene = _Parent ? _Parent->GetParentScene() : nullptr;
	CameraComponent* cam = scene ? scene->GetMainCamera() : nullptr;

	CBVS cb{};
	if (cam) {
		cb.View = DirectX::XMMatrixTranspose(cam->GetView());
		cb.Proj = DirectX::XMMatrixTranspose(cam->GetProjection());
	}
	else {
		cb.View = DirectX::XMMatrixIdentity();
		cb.Proj = DirectX::XMMatrixIdentity();
	}
	cb.Color = m_color;
	cb.mode2D = mode2DFlag;

	ctx->UpdateSubresource(m_cbVS.Get(), 0, nullptr, &cb, 0, 0);

	ID3D11ShaderResourceView* srv = (m_texture && m_texture->srv) ? m_texture->srv.Get() : s_whiteTexSRV.Get();

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

	ctx->DrawIndexed(6, 0, 0);
}

void ImageRender::DrawInspector() {
	auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };
	std::string title = _ComponentName + "##" + std::to_string((uintptr_t)this);
	if (!ImGui::CollapsingHeader(SJ(title.c_str()).c_str()))
		return;

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

	std::string modeLabels[4] = { SJ("2D"), SJ("Billboard"), SJ("3D"), SJ("UI") };
	int modeIdx = (int)m_mode;
	if (ImGui::BeginCombo(SJ("配置モード").c_str(), modeLabels[modeIdx].c_str())) {
		for (int i = 0; i < 4; ++i) {
			bool sel = (i == modeIdx);
			if (ImGui::Selectable(modeLabels[i].c_str(), sel)) {
				m_mode = (PlacementMode)i;
			}
			if (sel) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if (m_mode == PlacementMode::Screen2D || m_mode == PlacementMode::UI) {
		ImGui::InputFloat2(SJ("2Dオフセット(px)").c_str(), (float*)&m_offset2D);
		ImGui::InputFloat2(SJ("サイズ(px)").c_str(), (float*)&m_size2D);
	}
	else {
		ImGui::InputFloat3(SJ("3Dオフセット").c_str(), (float*)&m_offset3D);
		ImGui::InputFloat2(SJ("サイズ(ワールド)").c_str(), (float*)&m_sizeWorld);
	}

	// m_offsetRot を XYZ で編集 (度)
	ImGui::InputFloat3(SJ("回転(度) XYZ").c_str(), (float*)&m_offsetRot);

	ImGui::InputFloat4("UV(u0,v0,u1,v1)", (float*)&m_uvRect);
	m_uvRect.x = Clamp(m_uvRect.x, 0.0f, 1.0f);
	m_uvRect.y = Clamp(m_uvRect.y, 0.0f, 1.0f);
	m_uvRect.z = Clamp(m_uvRect.z, 0.0f, 1.0f);
	m_uvRect.w = Clamp(m_uvRect.w, 0.0f, 1.0f);

	ImGui::ColorEdit4(SJ("色").c_str(), (float*)&m_color);

	if (ImGui::TreeNode(SJ("シェーダ設定").c_str())) {
		auto* sm = ShaderManager::GetInstance();
		static std::vector<std::string> vsList;
		static std::vector<std::string> psList;
		if (ImGui::Button(SJ("更新リスト").c_str())) {
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