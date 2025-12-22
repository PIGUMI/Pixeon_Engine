//Dx11 の基本処理
#pragma once
// Include
#include <math.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <dxgidebug.h>
#include <timeapi.h>
#include <d3dcompiler.h>
#include <Windows.h>
#include <Digitalv.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <MSAcm.h>
#include <Shlwapi.h>
#include <vector>
#include <map>
#include <string>
#include <xaudio2.h>
#include <wrl/client.h>
#pragma comment(lib,"Winmm.lib")
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "msacm32.lib")
#pragma comment(lib, "shlwapi.lib")

#define SAFE_DELETE(p)			do{if(p){delete p; p = nullptr;}}while(0)
#define SAFE_DELETE_ARRAY(p)	do{if(p){delete[] p; p = nullptr;}}while(0)
#define SAFE_RELEASE(p)			do{if(p){p->Release(); p = nullptr;}}while(0)

enum BlendMode
{
	BLEND_NONE,
	BLEND_ALPHA,
	BLEND_ADD,
	BLEND_ADDALPHA,
	BLEND_SUB,
	BLEND_SCREEN,
	BLEND_MAX
};

enum SamplerState
{
	SAMPLER_LINEAR,
	SAMPLER_POINT,
	SAMPLER_MAX
};

struct Vertex {
	float pos[3];
	float uv[2];
};

// テクスチャ
class Render
{
public:
	Render();
	virtual ~Render();
	HRESULT Create(const char* fileName);
	HRESULT Create(DXGI_FORMAT format, UINT width, UINT height, const void* pData = nullptr);

	UINT GetWidth() const;
	UINT GetHeight() const;
	ID3D11ShaderResourceView* GetResource() const;

protected:
	D3D11_TEXTURE2D_DESC MakeTexDesc(DXGI_FORMAT format, UINT width, UINT height);
	virtual HRESULT CreateResource(D3D11_TEXTURE2D_DESC& desc, const void* pData);

protected:
	UINT m_width;
	UINT m_height;
	ID3D11ShaderResourceView* m_pSRV;
	ID3D11Texture2D* m_pTex;
};
// レンダーターゲット
class RenderTarget : public Render
{
public:
	RenderTarget();
	virtual ~RenderTarget();
	void Clear();
	void Clear(const float* color);
	HRESULT Create(DXGI_FORMAT format, UINT width, UINT height);
	HRESULT CreateFromScreen();
	ID3D11RenderTargetView* GetView() const;
protected:
	virtual HRESULT CreateResource(D3D11_TEXTURE2D_DESC& desc, const void* pData = nullptr);
private:
	ID3D11RenderTargetView* m_pRTV;
};
// 深度バッファ
class DepthStencil : public Render
{
public:
	DepthStencil();
	~DepthStencil();
	void Clear();
	HRESULT Create(UINT width, UINT height, bool useStencil);
	ID3D11DepthStencilView* GetView() const;

protected:
	virtual HRESULT CreateResource(D3D11_TEXTURE2D_DESC& desc, const void* pData = nullptr);
private:
	ID3D11DepthStencilView* m_pDSV;
};

// Singleton
class DirectX11
{
public:
	static DirectX11* GetInstance();
	static void DestroyInstance();
public:
	HRESULT Init(HWND hWnd, UINT width, UINT height, bool fullScreen);
	void Uninit();
	void BeginDraw();
	void EndDraw();

	void OnResize(UINT width, UINT height);
	ID3D11Device* GetDevice() { return g_pDevice; }
	ID3D11DeviceContext* GetContext() { return g_pContext; }
	IDXGISwapChain* GetSwapChain() { return g_pSwapChain; }
	RenderTarget* GetDefaultRTV() { return g_pRTV; }
	DepthStencil* GetDefaultDSV() { return g_pDSV; }
	void SetRenderTargets(UINT num, RenderTarget** ppViews, DepthStencil* pView);
	void SetCullingMode(D3D11_CULL_MODE cull);
	void SetBlendMode(BlendMode blend);
	void SetSamplerState(SamplerState state);
	ID3D11Buffer* CreateVertexBuffer(void* vtxData, UINT vtxNum);

public:
	void InitializeHDRPipeline(UINT width, UINT height);
	void RenderHDRScene();
	void ApplyToneMappingPass();

	ID3D11ShaderResourceView* GetHDRSRV() const { return _hdrSRV.Get(); }
	ID3D11RenderTargetView* GetHDRRTV() const { return _hdrRTV.Get(); }
private:
	static DirectX11* instance;

	ID3D11Device* g_pDevice;
	ID3D11DeviceContext* g_pContext;
	IDXGISwapChain* g_pSwapChain;
	RenderTarget* g_pRTV;
	DepthStencil* g_pDSV;
	ID3D11RasterizerState* g_pRasterizerState[3];
	ID3D11BlendState* g_pBlendState[BLEND_MAX];
	ID3D11SamplerState* g_pSamplerState[SAMPLER_MAX];
	ID3D11Debug* g_Debug;
private:
	Microsoft::WRL::ComPtr<ID3D11Texture2D> _hdrTexture;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> _hdrRTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _hdrSRV;

	Microsoft::WRL::ComPtr<ID3D11Buffer> _tonemapCB;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> _tonemapVS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> _tonemapPS;


	struct TonemapParams {
		float exposure;
		float gamma;
		float pad1, pad2;
	};
private:
	DirectX11() {};
	~DirectX11() {};
};

template<typename A, size_t N, typename T>
void Fill(A(&array)[N], const T& val) {
	std::fill((T*)array, (T*)(array + N), val);
}

template<typename T>
T Clamp(const T& value, const T& minVal, const T& maxVal)
{
	return (value < minVal) ? minVal : (value > maxVal) ? maxVal : value;
}
