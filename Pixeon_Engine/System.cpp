#include "System.h"
#include "DirectXTex/TextureLoad.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "EngineManager.h"
#include "SettingManager.h"

DirectX11* DirectX11::instance;

DirectX11* DirectX11::GetInstance()
{
	if (instance == nullptr) {
		instance = new DirectX11();
	}
	return instance;
}

void DirectX11::DestroyInstance()
{
	if (instance != nullptr) {
		delete instance;
		instance = nullptr;
	}
}

HRESULT DirectX11::Init(HWND hWnd, UINT width, UINT height, bool fullScreen)
{
	HRESULT result = E_FAIL;

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.SampleDesc.Count = 1;
	sd.BufferDesc.RefreshRate.Numerator = 1000;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 1;
	sd.OutputWindow = hWnd;
	sd.Windowed = fullScreen ? FALSE : TRUE;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// ドライバの種類
	D3D_DRIVER_TYPE driverTypes[] = {
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	UINT createDeviceFlags = 0;
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	D3D_DRIVER_TYPE driverType;
	D3D_FEATURE_LEVEL featureLevel;

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; ++driverTypeIndex)
	{
		driverType = driverTypes[driverTypeIndex];
		result = D3D11CreateDeviceAndSwapChain(
			NULL,
			driverType,
			NULL,
			createDeviceFlags,
			featureLevels,
			numFeatureLevels,
			D3D11_SDK_VERSION,
			&sd,
			&g_pSwapChain,
			&g_pDevice,
			&featureLevel,
			&g_pContext);
		if (SUCCEEDED(result)) {
			break;
		}
	}
	if (FAILED(result)) {
		return result;
	}

	g_pRTV = new RenderTarget();
	if(FAILED(result = g_pRTV->CreateFromScreen()))return result;
	g_pDSV = new DepthStencil();
	if (FAILED(result = g_pDSV->Create(g_pRTV->GetWidth(), g_pRTV->GetHeight(), false)))return result;
	SetRenderTargets(1, &g_pRTV, nullptr);

	D3D11_RASTERIZER_DESC rasterizer = {};
	D3D11_CULL_MODE cull[] = {
		D3D11_CULL_NONE,
		D3D11_CULL_FRONT,
		D3D11_CULL_BACK
	};
	rasterizer.FillMode = D3D11_FILL_SOLID;
	rasterizer.FrontCounterClockwise = false;
	for (int i = 0; i < 3; ++i)
	{
		rasterizer.CullMode = cull[i];
		result = g_pDevice->CreateRasterizerState(&rasterizer, &g_pRasterizerState[i]);
		if (FAILED(result)) { return result; }
	}
	SetCullingMode(D3D11_CULL_BACK);

	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	D3D11_BLEND blend[BLEND_MAX][2] = {
		{D3D11_BLEND_ONE, D3D11_BLEND_ZERO},
		{D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA},
		{D3D11_BLEND_ONE, D3D11_BLEND_ONE},
		{D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_ONE},
		{D3D11_BLEND_ZERO, D3D11_BLEND_INV_SRC_COLOR},
		{D3D11_BLEND_INV_DEST_COLOR, D3D11_BLEND_ONE},
	};
	for (int i = 0; i < BLEND_MAX; ++i)
	{
		blendDesc.RenderTarget[0].SrcBlend = blend[i][0];
		blendDesc.RenderTarget[0].DestBlend = blend[i][1];
		result = g_pDevice->CreateBlendState(&blendDesc, &g_pBlendState[i]);
		if (FAILED(result)) { return result; }
	}
	SetBlendMode(BLEND_ALPHA);

	D3D11_SAMPLER_DESC samplerDesc = {};
	D3D11_FILTER filter[] = {
		D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		D3D11_FILTER_MIN_MAG_MIP_POINT,
	};
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	for (int i = 0; i < SAMPLER_MAX; ++i)
	{
		samplerDesc.Filter = filter[i];
		result = g_pDevice->CreateSamplerState(&samplerDesc, &g_pSamplerState[i]);
		if (FAILED(result)) { return result; }
	}
	SetSamplerState(SAMPLER_LINEAR);

	g_pDevice->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void**>(&g_Debug));

	return S_OK;
}

void DirectX11::Uninit()
{
	SAFE_DELETE(g_pDSV);
	SAFE_DELETE(g_pRTV);

	for (int i = 0; i < SAMPLER_MAX; ++i)
		SAFE_RELEASE(g_pSamplerState[i]);
	for (int i = 0; i < BLEND_MAX; ++i)
		SAFE_RELEASE(g_pBlendState[i]);
	for (int i = 0; i < 3; ++i)
		SAFE_RELEASE(g_pRasterizerState[i]);
	if (g_pContext)
		g_pContext->ClearState();
	SAFE_RELEASE(g_pContext);
	if (g_pSwapChain)
		g_pSwapChain->SetFullscreenState(false, NULL);
	SAFE_RELEASE(g_pSwapChain);
	SAFE_RELEASE(g_pDevice);
}

void DirectX11::BeginDraw()
{

	float color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
	//if (IsInGame) {
	//	DirectX::XMFLOAT4 Temp;
	//	Temp = SettingManager::GetInstance()->GetBackgroundColor();
	//	color[0] = Temp.x;
	//	color[1] = Temp.y;
	//	color[2] = Temp.z;
	//	color[3] = Temp.w;
	//}
	g_pRTV->Clear(color);
	g_pDSV->Clear();
}

void DirectX11::EndDraw()
{
	g_pSwapChain->Present(0, 0);
}

void DirectX11::OnResize(UINT width, UINT height){
	if (width == 0 || height == 0) return;

	// ImGui: デバイスオブジェクト無効化
	ImGui_ImplDX11_InvalidateDeviceObjects();

	// 既存リソース削除
	SAFE_DELETE(g_pRTV);
	SAFE_DELETE(g_pDSV);

	// スワップチェーンのリサイズ
	HRESULT hr = g_pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr)) {
		MessageBox(nullptr, "ResizeBuffers Failed", "Error", MB_OK);
		return;
	}

	// 新しいRTV/DSVの生成
	g_pRTV = new RenderTarget();
	g_pRTV->CreateFromScreen();
	g_pDSV = new DepthStencil();
	g_pDSV->Create(g_pRTV->GetWidth(), g_pRTV->GetHeight(), false);

	// ビューポート再設定
	SetRenderTargets(1, &g_pRTV, g_pDSV);

	// ImGui: デバイスオブジェクト再生成
	ImGui_ImplDX11_CreateDeviceObjects();
}

void DirectX11::SetRenderTargets(UINT num, RenderTarget** ppViews, DepthStencil* pView)
{
	static ID3D11RenderTargetView* rtvs[4];

	if (num > 4) num = 4;
	for (UINT i = 0; i < num; ++i)
		rtvs[i] = ppViews[i]->GetView();
	g_pContext->OMSetRenderTargets(num, rtvs, pView ? pView->GetView() : nullptr);

	// ビューポートの設定
	D3D11_VIEWPORT vp;
	vp.TopLeftX = 0.0f;
	vp.TopLeftY = 0.0f;
	vp.Width = (float)ppViews[0]->GetWidth();
	vp.Height = (float)ppViews[0]->GetHeight();
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	g_pContext->RSSetViewports(1, &vp);
}

void DirectX11::SetCullingMode(D3D11_CULL_MODE cull)
{
	switch (cull)
	{
	case D3D11_CULL_NONE: g_pContext->RSSetState(g_pRasterizerState[0]); break;
	case D3D11_CULL_FRONT: g_pContext->RSSetState(g_pRasterizerState[1]); break;
	case D3D11_CULL_BACK: g_pContext->RSSetState(g_pRasterizerState[2]); break;
	}
}

void DirectX11::SetBlendMode(BlendMode blend)
{
	if (blend < 0 || blend >= BLEND_MAX) return;
	FLOAT blendFactor[4] = { D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO };
	g_pContext->OMSetBlendState(g_pBlendState[blend], blendFactor, 0xffffffff);
}

void DirectX11::SetSamplerState(SamplerState state)
{
	if (state < 0 || state >= SAMPLER_MAX) return;
	g_pContext->PSSetSamplers(0, 1, &g_pSamplerState[state]);
}

ID3D11Buffer* DirectX11::CreateVertexBuffer(void* vtxData, UINT vtxNum)
{
	// バッファ情報 設定
	D3D11_BUFFER_DESC vtxBufDesc;
	ZeroMemory(&vtxBufDesc, sizeof(vtxBufDesc));
	vtxBufDesc.ByteWidth = sizeof(Vertex) * vtxNum; // バッファの大きさ
	vtxBufDesc.Usage = D3D11_USAGE_DEFAULT; // メモリ上での管理方法
	vtxBufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER; // GPU上での利用方法
	// バッファ初期データ 設定
	D3D11_SUBRESOURCE_DATA vtxSubResource;
	ZeroMemory(&vtxSubResource, sizeof(vtxSubResource));
	vtxSubResource.pSysMem = vtxData; // バッファに流し込むデータ
	// 作成
	HRESULT hr;
	ID3D11Buffer* pVtxBuf;
	hr = GetDevice()->CreateBuffer(&vtxBufDesc, &vtxSubResource, &pVtxBuf);
	if (FAILED(hr)) { return nullptr; }
	return pVtxBuf;
}

// Render class

Render::Render()
	:m_width(0) , m_height(0)
	, m_pTex(nullptr)
	, m_pSRV(nullptr)
{
}

Render::~Render()
{
	SAFE_RELEASE(m_pSRV);
	SAFE_RELEASE(m_pTex);
}

HRESULT Render::Create(const char* fileName)
{
	HRESULT result = S_OK;

	wchar_t wPath[260];
	size_t wLen = 0;
	MultiByteToWideChar(0, 0, fileName, -1, wPath, MAX_PATH);
	
	DirectX::TexMetadata	mdata;
	DirectX::ScratchImage	image;

	if (strstr(fileName, ".tag"))result = DirectX::LoadFromTGAFile(wPath, &mdata, image);
	else result = DirectX::LoadFromWICFile(wPath, DirectX::WIC_FLAGS_NONE, &mdata, image);
	if (FAILED(result)) return result;

	result = DirectX::CreateShaderResourceView(DirectX11::GetInstance()->GetDevice(), image.GetImages(), image.GetImageCount(), mdata, &m_pSRV);
	if(SUCCEEDED(result))
	{
		m_width = (UINT)mdata.width;
		m_height = (UINT)mdata.height;
	}
	return result;
}

HRESULT Render::Create(DXGI_FORMAT format, UINT width, UINT height, const void* pData)
{
	D3D11_TEXTURE2D_DESC desc = MakeTexDesc(format, width, height);
	return CreateResource(desc, pData);
}

UINT Render::GetWidth() const
{
	return m_width;
}

UINT Render::GetHeight() const
{
	return m_height;
}

ID3D11ShaderResourceView* Render::GetResource() const
{
	return m_pSRV;
}

D3D11_TEXTURE2D_DESC Render::MakeTexDesc(DXGI_FORMAT format, UINT width, UINT height)
{
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.Format = format;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	return desc;
}

HRESULT Render::CreateResource(D3D11_TEXTURE2D_DESC& desc, const void* pData)
{
	HRESULT hr = E_FAIL;

	// テクスチャ作成
	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = pData;
	data.SysMemPitch = desc.Width * 4;
	hr = DirectX11::GetInstance()->GetDevice()->CreateTexture2D(&desc, pData ? &data : nullptr, &m_pTex);
	if (FAILED(hr)) { return hr; }

	// 設定
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	switch (desc.Format)
	{
	default:						srvDesc.Format = desc.Format;			break;
	case DXGI_FORMAT_R32_TYPELESS: 	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;	break;
	}
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	// 生成
	hr = DirectX11::GetInstance()->GetDevice()->CreateShaderResourceView(m_pTex, &srvDesc, &m_pSRV);
	if (SUCCEEDED(hr))
	{
		m_width = desc.Width;
		m_height = desc.Height;
	}
	return hr;
}

// RenderTarget class

RenderTarget::RenderTarget()
	:m_pRTV(nullptr)
{
}

RenderTarget::~RenderTarget()
{
	SAFE_RELEASE(m_pRTV);
}

void RenderTarget::Clear()
{
	static float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	Clear(color);
}

void RenderTarget::Clear(const float* color)
{
	DirectX11::GetInstance()->GetContext()->ClearRenderTargetView(m_pRTV, color);
}

HRESULT RenderTarget::Create(DXGI_FORMAT format, UINT width, UINT height)
{
	D3D11_TEXTURE2D_DESC desc = MakeTexDesc(format, width, height);
	desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
	return CreateResource(desc);
}

HRESULT RenderTarget::CreateFromScreen()
{
	HRESULT hr;

	// バックバッファのポインタを取得
	ID3D11Texture2D* pBackBuffer = NULL;
	hr = DirectX11::GetInstance()->GetSwapChain()->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&m_pTex);
	if (FAILED(hr)) { return hr; }

	// バックバッファへのポインタを指定してレンダーターゲットビューを作成
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvDesc.Texture2D.MipSlice = 0;
	hr = DirectX11::GetInstance()->GetDevice()->CreateRenderTargetView(m_pTex, &rtvDesc, &m_pRTV);
	if (SUCCEEDED(hr))
	{
		D3D11_TEXTURE2D_DESC desc;
		m_pTex->GetDesc(&desc);
		m_width = desc.Width;
		m_height = desc.Height;
	}
	return hr;
}

ID3D11RenderTargetView* RenderTarget::GetView() const
{
	return m_pRTV;
}

HRESULT RenderTarget::CreateResource(D3D11_TEXTURE2D_DESC& desc, const void* pData)
{
	// テクスチャリソース作成
	HRESULT hr = Render::CreateResource(desc, nullptr);
	if (FAILED(hr)) { return hr; }

	// 設定
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = desc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	// 生成
	return DirectX11::GetInstance()->GetDevice()->CreateRenderTargetView(m_pTex, &rtvDesc, &m_pRTV);
}

// DepthStencil class

DepthStencil::DepthStencil()
	:m_pDSV(nullptr)
{
}

DepthStencil::~DepthStencil()
{
	SAFE_RELEASE(m_pDSV);
}

void DepthStencil::Clear()
{
	DirectX11::GetInstance()->GetContext()->ClearDepthStencilView(m_pDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

HRESULT DepthStencil::Create(UINT width, UINT height, bool useStencil)
{
	D3D11_TEXTURE2D_DESC desc = MakeTexDesc(useStencil ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_R32_TYPELESS, width, height);
	desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
	return CreateResource(desc);
}

ID3D11DepthStencilView* DepthStencil::GetView() const
{
	return m_pDSV;
}

HRESULT DepthStencil::CreateResource(D3D11_TEXTURE2D_DESC& desc, const void* pData)
{
	// ステンシル使用判定
	bool useStencil = (desc.Format == DXGI_FORMAT_R24G8_TYPELESS);

	// リソース生成
	desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
	HRESULT hr = Render::CreateResource(desc, nullptr);
	if (FAILED(hr)) { return hr; }

	// 設定
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = useStencil ? DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

	// 生成
	return DirectX11::GetInstance()->GetDevice()->CreateDepthStencilView(m_pTex, &dsvDesc, &m_pDSV);
}
