/*
* ファイル名　System
* 説　　　明　描画システムの基本処理
*             以前は D3D11 を直接扱う DirectX11 クラスだったが、RHI を介して
*             D3D11 / D3D12 / Vulkan を切り替えられる GraphicsSystem に置き換えた
*/
#pragma once
#include <math.h>
#include <Windows.h>
#include <DirectXMath.h>
#include <timeapi.h>
#include <mmsystem.h>
#include <Shlwapi.h>
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "RHI/RHI.h"
#include "../Common/GameManifest.h"
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shlwapi.lib")

#define SAFE_DELETE(p)			do{if(p){delete p; p = nullptr;}}while(0)
#define SAFE_DELETE_ARRAY(p)	do{if(p){delete[] p; p = nullptr;}}while(0)

enum CullMode
{
	CULL_NONE,
	CULL_FRONT,
	CULL_BACK,
	CULL_MAX
};

enum BlendMode
{
	BLEND_NONE,
	BLEND_ALPHA,
	BLEND_ADD,
	BLEND_ADDALPHA,
	BLEND_SUB,
	BLEND_SCREEN,
	BLEND_PREMULTIPLIED,   // 乗算済みアルファ (src=ONE, dst=INV_SRC_ALPHA)
	BLEND_ADD_ALL,         // カラー・アルファとも加算 (ONE, ONE)
	BLEND_MAX
};

enum SamplerState
{
	SAMPLER_LINEAR,
	SAMPLER_POINT,
	SAMPLER_LINEAR_CLAMP,
	SAMPLER_POINT_CLAMP,
	SAMPLER_SHADOW,        // 比較サンプラー（シャドウマップ用、境界色 1.0）
	SAMPLER_MAX
};

enum DepthMode
{
	DEPTH_DEFAULT,         // 深度テスト LESS + 書き込みあり（D3D11 の既定）
	DEPTH_OFF,             // テスト・書き込みなし
	DEPTH_READ_ONLY,       // テスト LESS_EQUAL + 書き込みなし
	DEPTH_LESS_EQUAL,      // テスト LESS_EQUAL + 書き込みあり
	DEPTH_MAX
};

// 描画システム（シングルトン）
class GraphicsSystem
{
public:
	static GraphicsSystem* GetInstance();
	static void DestroyInstance();
public:
	bool Init(HWND hWnd, UINT width, UINT height, bool fullScreen, bool vsync, GraphicsBackend backend, std::string* out_error = nullptr);
	void Uninit();
	void BeginDraw();
	void EndDraw();
	void OnResize(UINT width, UINT height);

	RHI::Device* GetDevice() const { return m_cls_Device.get(); }
	RHI::ImGuiRenderer* GetImGuiRenderer() const { return m_cls_ImGui.get(); }
	RHI::Backend GetBackend() const { return m_cls_Device ? m_cls_Device->BackendGet() : RHI::Backend::D3D11; }
	RHI::Texture* GetBackBuffer() const { return m_cls_Device ? m_cls_Device->BackBufferGet() : nullptr; }
	RHI::Texture* GetDefaultDepth() const { return m_cls_DefaultDepth.get(); }
	UINT GetWidth() const { return m_i_Width; }
	UINT GetHeight() const { return m_i_Height; }

	// バックバッファを描画先に戻す（ビューポートも全画面に設定）
	void BackBufferTargetSet(bool with_depth = true);
	// 任意のターゲットへ切り替え、そのサイズのビューポートを設定する
	void RenderTargetsSet(UINT num, RHI::Texture* const* colors, RHI::Texture* depth);

	void SetCullingMode(CullMode cull);
	void SetBlendMode(BlendMode blend);
	void SetSamplerState(SamplerState state, UINT slot = 0, RHI::ShaderStage stage = RHI::ShaderStage::Pixel);
	void SetDepthMode(DepthMode mode);

	RHI::RasterizerState* GetRasterizerState(CullMode cull) const { return m_RasterizerStates[cull].get(); }
	RHI::BlendState* GetBlendState(BlendMode blend) const { return m_BlendStates[blend].get(); }
	RHI::Sampler* GetSampler(SamplerState state) const { return m_Samplers[state].get(); }
	RHI::DepthStencilState* GetDepthState(DepthMode mode) const { return m_DepthStates[mode].get(); }
	RHI::Texture* GetWhiteTexture() const { return m_cls_WhiteTexture.get(); }

private:
	bool StatesCreate();
	bool DefaultTargetsCreate(UINT width, UINT height);

	static GraphicsSystem* m_cls_Instance;

	std::unique_ptr<RHI::Device> m_cls_Device;
	std::unique_ptr<RHI::ImGuiRenderer> m_cls_ImGui;
	RHI::TextureRef m_cls_DefaultDepth;
	RHI::TextureRef m_cls_WhiteTexture;
	RHI::RasterizerStateRef m_RasterizerStates[CULL_MAX];
	RHI::BlendStateRef m_BlendStates[BLEND_MAX];
	RHI::SamplerRef m_Samplers[SAMPLER_MAX];
	RHI::DepthStencilStateRef m_DepthStates[DEPTH_MAX];
	UINT m_i_Width = 0;
	UINT m_i_Height = 0;

	GraphicsSystem() = default;
	~GraphicsSystem() = default;
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
