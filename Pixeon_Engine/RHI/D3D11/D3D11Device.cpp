/*
* ファイル名　D3D11Device
* 説　　　明　RHI の Direct3D 11 バックエンド
*             RHI の API は D3D11 のイミディエイトコンテキストとほぼ 1 対 1 で対応する
*/
#include "../RHI.h"
#include "../DXGI/DXGICommon.h"
#include "D3D11Native.h"
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>

#if defined(_MSC_VER)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#endif

using Microsoft::WRL::ComPtr;

namespace RHI {
	namespace {
		// ---------------------------------------------------------------
		// 列挙型の変換
		// ---------------------------------------------------------------
		D3D11_BLEND BlendGet(Blend b)
		{
			static const D3D11_BLEND table[] = {
				D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_SRC_COLOR, D3D11_BLEND_INV_SRC_COLOR,
				D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA,
				D3D11_BLEND_DEST_COLOR, D3D11_BLEND_INV_DEST_COLOR, D3D11_BLEND_SRC_ALPHA_SAT,
				D3D11_BLEND_BLEND_FACTOR, D3D11_BLEND_INV_BLEND_FACTOR,
			};
			return table[(int)b];
		}
		D3D11_BLEND_OP BlendOpGet(BlendOp o)
		{
			static const D3D11_BLEND_OP table[] = {
				D3D11_BLEND_OP_ADD, D3D11_BLEND_OP_SUBTRACT, D3D11_BLEND_OP_REV_SUBTRACT, D3D11_BLEND_OP_MIN, D3D11_BLEND_OP_MAX,
			};
			return table[(int)o];
		}
		D3D11_COMPARISON_FUNC CompareGet(CompareFunc c)
		{
			static const D3D11_COMPARISON_FUNC table[] = {
				D3D11_COMPARISON_NEVER, D3D11_COMPARISON_LESS, D3D11_COMPARISON_EQUAL, D3D11_COMPARISON_LESS_EQUAL,
				D3D11_COMPARISON_GREATER, D3D11_COMPARISON_NOT_EQUAL, D3D11_COMPARISON_GREATER_EQUAL, D3D11_COMPARISON_ALWAYS,
			};
			return table[(int)c];
		}
		D3D11_STENCIL_OP StencilOpGet(StencilOp o)
		{
			static const D3D11_STENCIL_OP table[] = {
				D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_ZERO, D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_INCR_SAT,
				D3D11_STENCIL_OP_DECR_SAT, D3D11_STENCIL_OP_INVERT, D3D11_STENCIL_OP_INCR, D3D11_STENCIL_OP_DECR,
			};
			return table[(int)o];
		}
		D3D11_FILTER FilterGet(Filter f)
		{
			switch (f) {
			case Filter::Point:                          return D3D11_FILTER_MIN_MAG_MIP_POINT;
			case Filter::MinMagLinearMipPoint:           return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			case Filter::Anisotropic:                    return D3D11_FILTER_ANISOTROPIC;
			case Filter::ComparisonPoint:                return D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
			case Filter::ComparisonLinear:               return D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
			case Filter::ComparisonMinMagLinearMipPoint: return D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
			default:                                     return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			}
		}
		D3D11_TEXTURE_ADDRESS_MODE AddressGet(AddressMode a)
		{
			static const D3D11_TEXTURE_ADDRESS_MODE table[] = {
				D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_MIRROR, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_BORDER,
			};
			return table[(int)a];
		}
		D3D11_PRIMITIVE_TOPOLOGY TopologyGet(Topology t)
		{
			switch (t) {
			case Topology::TriangleStrip: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			case Topology::LineList:      return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
			case Topology::LineStrip:     return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
			case Topology::PointList:     return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
			default:                      return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			}
		}

		// ---------------------------------------------------------------
		// リソース
		// ---------------------------------------------------------------
		class D3D11Buffer final : public Buffer {
		public:
			explicit D3D11Buffer(const BufferDesc& desc) : Buffer(desc) {}
			ComPtr<ID3D11Buffer> m_cls_Buffer;
			std::vector<uint8_t> m_Shadow;   // 部分更新のための CPU 側コピー（定数 / Dynamic バッファ）
		};

		class D3D11Texture final : public Texture {
		public:
			explicit D3D11Texture(const TextureDesc& desc) : Texture(desc) {}
			ComPtr<ID3D11Texture2D> m_cls_Texture;
			ComPtr<ID3D11ShaderResourceView> m_cls_Srv;
			ComPtr<ID3D11RenderTargetView> m_cls_Rtv;
			ComPtr<ID3D11DepthStencilView> m_cls_Dsv;
		};

		class D3D11Sampler final : public Sampler {
		public:
			explicit D3D11Sampler(const SamplerDesc& desc) : Sampler(desc) {}
			ComPtr<ID3D11SamplerState> m_cls_Sampler;
		};

		class D3D11Shader final : public Shader {
		public:
			D3D11Shader(ShaderStage stage, const void* code, size_t size, const char* name) : Shader(stage, code, size, name)
			{
				DxbcReflect(code, size, m_str_Reflection);
			}
			ComPtr<ID3D11VertexShader> m_cls_Vs;
			ComPtr<ID3D11PixelShader> m_cls_Ps;
		};

		class D3D11InputLayout final : public InputLayout {
		public:
			D3D11InputLayout(const InputElement* e, uint32_t n) : InputLayout(e, n) {}
			ComPtr<ID3D11InputLayout> m_cls_Layout;
		};

		class D3D11BlendState final : public BlendState {
		public:
			explicit D3D11BlendState(const BlendDesc& d) : BlendState(d) {}
			ComPtr<ID3D11BlendState> m_cls_State;
		};

		class D3D11DepthStencilState final : public DepthStencilState {
		public:
			explicit D3D11DepthStencilState(const DepthStencilDesc& d) : DepthStencilState(d) {}
			ComPtr<ID3D11DepthStencilState> m_cls_State;
		};

		class D3D11RasterizerState final : public RasterizerState {
		public:
			explicit D3D11RasterizerState(const RasterizerDesc& d) : RasterizerState(d) {}
			ComPtr<ID3D11RasterizerState> m_cls_State;
		};

		D3D11Texture* TexCast(Texture* t) { return static_cast<D3D11Texture*>(t); }
		D3D11Buffer* BufCast(Buffer* b) { return static_cast<D3D11Buffer*>(b); }

		// ---------------------------------------------------------------
		// デバイス
		// ---------------------------------------------------------------
		class D3D11Device final : public Device {
		public:
			~D3D11Device() override;
			bool Init(const DeviceCreateInfo& info, std::string* out_error);

			Backend BackendGet() const override { return Backend::D3D11; }
			const char* AdapterNameGet() const override { return m_AdapterName.c_str(); }
			NativeHandles NativeHandlesGet() const override;
			uint64_t FrameIndexGet() const override { return m_i_Frame; }

			BufferRef BufferCreate(const BufferDesc& desc, const void* initial_data) override;
			TextureRef TextureCreate(const TextureDesc& desc, const SubresourceData* initial_data) override;
			SamplerRef SamplerCreate(const SamplerDesc& desc) override;
			ShaderRef ShaderCreate(ShaderStage stage, const void* bytecode, size_t size, const char* name) override;
			InputLayoutRef InputLayoutCreate(const InputElement* elements, uint32_t count, const Shader* vs) override;
			BlendStateRef BlendStateCreate(const BlendDesc& desc) override;
			DepthStencilStateRef DepthStencilStateCreate(const DepthStencilDesc& desc) override;
			RasterizerStateRef RasterizerStateCreate(const RasterizerDesc& desc) override;

			void BufferUpdate(Buffer* buffer, const void* data, uint32_t size, uint32_t offset) override;
			void* BufferMap(Buffer* buffer) override;
			void BufferUnmap(Buffer* buffer) override;
			void TextureUpdate(Texture* texture, uint32_t mip, const void* data, uint32_t row_pitch) override;
			void TextureCopy(Texture* dst, Texture* src) override;
			bool TextureRead(Texture* texture, std::vector<uint8_t>& out_pixels, uint32_t& out_row_pitch) override;

			Texture* BackBufferGet() override { return m_cls_BackBuffer.get(); }
			void Present() override;
			void SwapChainResize(uint32_t width, uint32_t height) override;
			void VSyncSet(bool enable) override { m_b_VSync = enable; }
			void IdleWait() override { m_cls_Context->Flush(); }

			void RenderTargetsSet(uint32_t count, Texture* const* colors, Texture* depth) override;
			RenderTargetState RenderTargetsGet() const override { return m_str_RenderTargets; }
			void RenderTargetClear(Texture* target, const float color[4]) override;
			void DepthStencilClear(Texture* target, bool clear_depth, bool clear_stencil, float depth, uint8_t stencil) override;
			void ViewportSet(const Viewport& viewport) override;
			Viewport ViewportGet() const override { return m_str_Viewport; }
			void ScissorSet(const Rect& rect) override;

			void BlendStateSet(BlendState* state, const float blend_factor[4], uint32_t sample_mask) override;
			BlendState* BlendStateGet() const override { return m_cls_Blend; }
			void DepthStencilStateSet(DepthStencilState* state, uint32_t stencil_ref) override;
			DepthStencilState* DepthStencilStateGet() const override { return m_cls_Depth; }
			void RasterizerStateSet(RasterizerState* state) override;
			RasterizerState* RasterizerStateGet() const override { return m_cls_Raster; }

			void ShaderSet(ShaderStage stage, Shader* shader) override;
			void InputLayoutSet(InputLayout* layout) override;
			void TopologySet(Topology topology) override;
			void VertexBufferSet(uint32_t slot, Buffer* buffer, uint32_t stride, uint32_t offset) override;
			void IndexBufferSet(Buffer* buffer, IndexFormat format, uint32_t offset) override;
			void ConstantBufferSet(ShaderStage stage, uint32_t slot, Buffer* buffer) override;
			void TextureSet(ShaderStage stage, uint32_t slot, Texture* texture) override;
			void SamplerSet(ShaderStage stage, uint32_t slot, Sampler* sampler) override;
			void StateClear() override;

			void Draw(uint32_t vertex_count, uint32_t start_vertex) override;
			void IndexedDraw(uint32_t index_count, uint32_t start_index, int32_t base_vertex) override;
			void InstancedDraw(uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex, uint32_t start_instance) override;

			void* NativeCommandsBegin() override { return m_cls_Context.Get(); }
			void NativeCommandsEnd() override {}
			void MarkerBegin(const char* name) override;
			void MarkerEnd() override;

			ID3D11Device* NativeDeviceGet() const { return m_cls_Device.Get(); }

		private:
			bool SwapChainCreate(const DeviceCreateInfo& info);
			bool BackBufferCreate(uint32_t width, uint32_t height);
			bool TextureViewsCreate(D3D11Texture* tex);

			ComPtr<ID3D11Device> m_cls_Device;
			ComPtr<ID3D11DeviceContext> m_cls_Context;
			ComPtr<ID3DUserDefinedAnnotation> m_cls_Annotation;
			ComPtr<IDXGISwapChain> m_cls_SwapChain;
			std::shared_ptr<D3D11Texture> m_cls_BackBuffer;
			std::string m_AdapterName;
			bool m_b_VSync = true;
			bool m_b_Headless = false;
			uint64_t m_i_Frame = 0;

			RenderTargetState m_str_RenderTargets;
			Viewport m_str_Viewport;
			BlendState* m_cls_Blend = nullptr;
			DepthStencilState* m_cls_Depth = nullptr;
			RasterizerState* m_cls_Raster = nullptr;
		};

		D3D11Device::~D3D11Device()
		{
			if (m_cls_Context) m_cls_Context->ClearState();
			if (m_cls_SwapChain) m_cls_SwapChain->SetFullscreenState(FALSE, nullptr);
		}

		/*
		* 関数名　Init
		* 引　数　info：生成情報　out_error：エラー内容
		* 戻り値　成功なら true
		* 説　明　ハードウェア → WARP の順でデバイスを作り、スワップチェーンを用意する
		*/
		bool D3D11Device::Init(const DeviceCreateInfo& info, std::string* out_error)
		{
			UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
			if (info.b_Debug) flags |= D3D11_CREATE_DEVICE_DEBUG;
			const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
			HRESULT hr = E_FAIL;
			for (D3D_DRIVER_TYPE type : { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP }) {
				hr = D3D11CreateDevice(nullptr, type, nullptr, flags, levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
					m_cls_Device.GetAddressOf(), nullptr, m_cls_Context.GetAddressOf());
				if (FAILED(hr) && (flags & D3D11_CREATE_DEVICE_DEBUG)) {
					// デバッグレイヤが無い環境では無しで作り直す
					hr = D3D11CreateDevice(nullptr, type, nullptr, flags & ~D3D11_CREATE_DEVICE_DEBUG, levels, ARRAYSIZE(levels),
						D3D11_SDK_VERSION, m_cls_Device.GetAddressOf(), nullptr, m_cls_Context.GetAddressOf());
				}
				if (SUCCEEDED(hr)) break;
			}
			if (FAILED(hr)) {
				if (out_error) *out_error = "D3D11CreateDevice に失敗しました";
				return false;
			}
			m_cls_Context.As(&m_cls_Annotation);

			ComPtr<IDXGIDevice> dxgi_device;
			ComPtr<IDXGIAdapter> adapter;
			if (SUCCEEDED(m_cls_Device.As(&dxgi_device)) && SUCCEEDED(dxgi_device->GetAdapter(&adapter))) {
				DXGI_ADAPTER_DESC desc = {};
				adapter->GetDesc(&desc);
				char name[128] = {};
				WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), nullptr, nullptr);
				m_AdapterName = name;
			}

			m_b_VSync = info.b_VSync;
			m_b_Headless = (info.p_WindowHandle == nullptr);
			if (m_b_Headless) return BackBufferCreate(info.i_Width, info.i_Height);
			if (!SwapChainCreate(info)) {
				if (out_error) *out_error = "スワップチェーンの作成に失敗しました";
				return false;
			}
			return true;
		}

		/*
		* 関数名　SwapChainCreate
		* 引　数　info：生成情報
		* 戻り値　成功なら true
		* 説　明　フリップモデル（2 枚）のスワップチェーンを作成する
		*/
		bool D3D11Device::SwapChainCreate(const DeviceCreateInfo& info)
		{
			ComPtr<IDXGIDevice> dxgi_device;
			ComPtr<IDXGIAdapter> adapter;
			ComPtr<IDXGIFactory> factory;
			if (FAILED(m_cls_Device.As(&dxgi_device)) || FAILED(dxgi_device->GetAdapter(&adapter)) ||
				FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) return false;

			DXGI_SWAP_CHAIN_DESC sd = {};
			sd.BufferDesc.Width = info.i_Width;
			sd.BufferDesc.Height = info.i_Height;
			sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.SampleDesc.Count = 1;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.BufferCount = 2;
			sd.OutputWindow = (HWND)info.p_WindowHandle;
			sd.Windowed = info.b_Fullscreen ? FALSE : TRUE;
			sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			if (FAILED(factory->CreateSwapChain(m_cls_Device.Get(), &sd, m_cls_SwapChain.GetAddressOf()))) {
				// フリップモード非対応 (古い OS) は従来方式で再試行
				sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
				sd.BufferCount = 1;
				if (FAILED(factory->CreateSwapChain(m_cls_Device.Get(), &sd, m_cls_SwapChain.GetAddressOf()))) return false;
			}
			factory->MakeWindowAssociation(sd.OutputWindow, DXGI_MWA_NO_ALT_ENTER);
			return BackBufferCreate(info.i_Width, info.i_Height);
		}

		/*
		* 関数名　BackBufferCreate
		* 引　数　width / height：サイズ（ヘッドレス時のみ使用）
		* 戻り値　成功なら true
		* 説　明　スワップチェーンのバッファ（ヘッドレスなら通常のテクスチャ）を RHI テクスチャとして包む
		*/
		bool D3D11Device::BackBufferCreate(uint32_t width, uint32_t height)
		{
			TextureDesc desc;
			desc.e_Format = Format::R8G8B8A8_UNorm;
			desc.i_BindFlags = Bind_RenderTarget;
			if (m_b_Headless) {
				desc.i_Width = width;
				desc.i_Height = height;
				desc.i_BindFlags |= Bind_ShaderResource;
				auto tex = std::static_pointer_cast<D3D11Texture>(TextureCreate(desc, nullptr));
				m_cls_BackBuffer = tex;
				return tex != nullptr;
			}
			ComPtr<ID3D11Texture2D> buffer;
			if (FAILED(m_cls_SwapChain->GetBuffer(0, IID_PPV_ARGS(&buffer)))) return false;
			D3D11_TEXTURE2D_DESC td = {};
			buffer->GetDesc(&td);
			desc.i_Width = td.Width;
			desc.i_Height = td.Height;
			auto tex = std::make_shared<D3D11Texture>(desc);
			tex->m_cls_Texture = buffer;
			if (FAILED(m_cls_Device->CreateRenderTargetView(buffer.Get(), nullptr, tex->m_cls_Rtv.GetAddressOf()))) return false;
			m_cls_BackBuffer = tex;
			return true;
		}

		NativeHandles D3D11Device::NativeHandlesGet() const
		{
			NativeHandles h;
			h.p_Device = m_cls_Device.Get();
			h.p_Context = m_cls_Context.Get();
			return h;
		}

		// ---- リソース生成 ------------------------------------------------

		BufferRef D3D11Device::BufferCreate(const BufferDesc& desc, const void* initial_data)
		{
			auto buf = std::make_shared<D3D11Buffer>(desc);
			D3D11_BUFFER_DESC bd = {};
			bd.ByteWidth = (desc.i_BindFlags & Bind_ConstantBuffer) ? ((desc.i_Size + 15) & ~15u) : desc.i_Size;
			if (desc.i_BindFlags & Bind_VertexBuffer) bd.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
			if (desc.i_BindFlags & Bind_IndexBuffer) bd.BindFlags |= D3D11_BIND_INDEX_BUFFER;
			if (desc.i_BindFlags & Bind_ConstantBuffer) bd.BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
			if (desc.i_BindFlags & Bind_ShaderResource) bd.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
			bd.Usage = (desc.e_Usage == Usage::Dynamic) ? D3D11_USAGE_DYNAMIC
				: (desc.e_Usage == Usage::Immutable && initial_data) ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
			bd.CPUAccessFlags = (desc.e_Usage == Usage::Dynamic) ? D3D11_CPU_ACCESS_WRITE : 0;

			D3D11_SUBRESOURCE_DATA init = {};
			init.pSysMem = initial_data;
			if (FAILED(m_cls_Device->CreateBuffer(&bd, initial_data ? &init : nullptr, buf->m_cls_Buffer.GetAddressOf()))) {
				LogWrite("D3D11: CreateBuffer 失敗 (size=%u)", desc.i_Size);
				return nullptr;
			}
			if ((desc.i_BindFlags & Bind_ConstantBuffer) || desc.e_Usage == Usage::Dynamic) {
				buf->m_Shadow.assign(bd.ByteWidth, 0);
				if (initial_data) std::memcpy(buf->m_Shadow.data(), initial_data, desc.i_Size);
			}
			return buf;
		}

		/*
		* 関数名　TextureViewsCreate
		* 引　数　tex：テクスチャ
		* 戻り値　成功なら true
		* 説　明　用途に応じて SRV / RTV / DSV を作る。深度を SRV で読む場合は TYPELESS で作成済みの前提
		*/
		bool D3D11Device::TextureViewsCreate(D3D11Texture* tex)
		{
			const TextureDesc& d = tex->DescGet();
			const bool depth = FormatInfoGet(d.e_Format).b_Depth;
			if (d.i_BindFlags & Bind_ShaderResource) {
				D3D11_SHADER_RESOURCE_VIEW_DESC sv = {};
				sv.Format = depth ? DxgiDepthSrvFormatGet(d.e_Format) : DxgiFormatGet(d.e_Format);
				sv.ViewDimension = (d.i_ArraySize > 1) ? D3D11_SRV_DIMENSION_TEXTURE2DARRAY : D3D11_SRV_DIMENSION_TEXTURE2D;
				if (d.i_ArraySize > 1) { sv.Texture2DArray.MipLevels = d.i_MipLevels; sv.Texture2DArray.ArraySize = d.i_ArraySize; }
				else sv.Texture2D.MipLevels = d.i_MipLevels;
				if (FAILED(m_cls_Device->CreateShaderResourceView(tex->m_cls_Texture.Get(), &sv, tex->m_cls_Srv.GetAddressOf()))) return false;
			}
			if (d.i_BindFlags & Bind_RenderTarget) {
				D3D11_RENDER_TARGET_VIEW_DESC rv = {};
				rv.Format = DxgiFormatGet(d.e_Format);
				rv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
				if (FAILED(m_cls_Device->CreateRenderTargetView(tex->m_cls_Texture.Get(), &rv, tex->m_cls_Rtv.GetAddressOf()))) return false;
			}
			if (d.i_BindFlags & Bind_DepthStencil) {
				D3D11_DEPTH_STENCIL_VIEW_DESC dv = {};
				dv.Format = DxgiFormatGet(d.e_Format);
				dv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
				if (FAILED(m_cls_Device->CreateDepthStencilView(tex->m_cls_Texture.Get(), &dv, tex->m_cls_Dsv.GetAddressOf()))) return false;
			}
			return true;
		}

		TextureRef D3D11Device::TextureCreate(const TextureDesc& desc, const SubresourceData* initial_data)
		{
			auto tex = std::make_shared<D3D11Texture>(desc);
			const bool depth = FormatInfoGet(desc.e_Format).b_Depth;
			D3D11_TEXTURE2D_DESC td = {};
			td.Width = desc.i_Width;
			td.Height = desc.i_Height;
			td.MipLevels = desc.i_MipLevels;
			td.ArraySize = desc.i_ArraySize;
			td.Format = (depth && (desc.i_BindFlags & Bind_ShaderResource)) ? DxgiTypelessFormatGet(desc.e_Format) : DxgiFormatGet(desc.e_Format);
			td.SampleDesc.Count = 1;
			td.Usage = (desc.e_Usage == Usage::Immutable && initial_data) ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
			if (desc.i_BindFlags & Bind_ShaderResource) td.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
			if (desc.i_BindFlags & Bind_RenderTarget) td.BindFlags |= D3D11_BIND_RENDER_TARGET;
			if (desc.i_BindFlags & Bind_DepthStencil) td.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

			std::vector<D3D11_SUBRESOURCE_DATA> init;
			if (initial_data) {
				init.resize((size_t)desc.i_MipLevels * desc.i_ArraySize);
				for (size_t i = 0; i < init.size(); ++i) {
					init[i].pSysMem = initial_data[i].p_Data;
					init[i].SysMemPitch = initial_data[i].i_RowPitch;
					init[i].SysMemSlicePitch = initial_data[i].i_SlicePitch;
				}
			}
			if (FAILED(m_cls_Device->CreateTexture2D(&td, initial_data ? init.data() : nullptr, tex->m_cls_Texture.GetAddressOf())) ||
				!TextureViewsCreate(tex.get())) {
				LogWrite("D3D11: CreateTexture2D 失敗 (%ux%u %s)", desc.i_Width, desc.i_Height, FormatNameGet(desc.e_Format));
				return nullptr;
			}
			return tex;
		}

		SamplerRef D3D11Device::SamplerCreate(const SamplerDesc& desc)
		{
			auto s = std::make_shared<D3D11Sampler>(desc);
			D3D11_SAMPLER_DESC sd = {};
			sd.Filter = FilterGet(desc.e_Filter);
			sd.AddressU = AddressGet(desc.e_AddressU);
			sd.AddressV = AddressGet(desc.e_AddressV);
			sd.AddressW = AddressGet(desc.e_AddressW);
			sd.MipLODBias = desc.f_MipLodBias;
			sd.MaxAnisotropy = desc.i_MaxAnisotropy;
			sd.ComparisonFunc = CompareGet(desc.e_Compare);
			std::memcpy(sd.BorderColor, desc.f_BorderColor, sizeof(sd.BorderColor));
			sd.MinLOD = desc.f_MinLod;
			sd.MaxLOD = desc.f_MaxLod;
			if (FAILED(m_cls_Device->CreateSamplerState(&sd, s->m_cls_Sampler.GetAddressOf()))) return nullptr;
			return s;
		}

		ShaderRef D3D11Device::ShaderCreate(ShaderStage stage, const void* bytecode, size_t size, const char* name)
		{
			auto sh = std::make_shared<D3D11Shader>(stage, bytecode, size, name);
			HRESULT hr = (stage == ShaderStage::Vertex)
				? m_cls_Device->CreateVertexShader(bytecode, size, nullptr, sh->m_cls_Vs.GetAddressOf())
				: m_cls_Device->CreatePixelShader(bytecode, size, nullptr, sh->m_cls_Ps.GetAddressOf());
			if (FAILED(hr)) {
				LogWrite("D3D11: シェーダー作成失敗 %s", name ? name : "");
				return nullptr;
			}
			return sh;
		}

		InputLayoutRef D3D11Device::InputLayoutCreate(const InputElement* elements, uint32_t count, const Shader* vs)
		{
			if (!vs) return nullptr;
			auto layout = std::make_shared<D3D11InputLayout>(elements, count);
			std::vector<D3D11_INPUT_ELEMENT_DESC> descs;
			for (auto& e : layout->ElementsGet()) {
				D3D11_INPUT_ELEMENT_DESC d = {};
				d.SemanticName = e.c_Semantic;
				d.SemanticIndex = e.i_SemanticIndex;
				d.Format = DxgiFormatGet(e.e_Format);
				d.InputSlot = e.i_Slot;
				d.AlignedByteOffset = e.i_Offset;
				d.InputSlotClass = (e.e_Rate == InputRate::PerInstance) ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
				d.InstanceDataStepRate = e.i_InstanceStep;
				descs.push_back(d);
			}
			const auto& code = vs->BytecodeGet();
			if (FAILED(m_cls_Device->CreateInputLayout(descs.data(), (UINT)descs.size(), code.data(), code.size(), layout->m_cls_Layout.GetAddressOf()))) {
				LogWrite("D3D11: CreateInputLayout 失敗 (%s)", vs->NameGet().c_str());
				return nullptr;
			}
			return layout;
		}

		BlendStateRef D3D11Device::BlendStateCreate(const BlendDesc& desc)
		{
			auto s = std::make_shared<D3D11BlendState>(desc);
			D3D11_BLEND_DESC bd = {};
			bd.AlphaToCoverageEnable = desc.b_AlphaToCoverage;
			bd.IndependentBlendEnable = desc.b_IndependentBlend;
			for (uint32_t i = 0; i < MAX_RENDER_TARGETS; ++i) {
				const auto& r = desc.renderTarget[i];
				auto& o = bd.RenderTarget[i];
				o.BlendEnable = r.b_BlendEnable;
				o.SrcBlend = BlendGet(r.e_SrcBlend);
				o.DestBlend = BlendGet(r.e_DestBlend);
				o.BlendOp = BlendOpGet(r.e_BlendOp);
				o.SrcBlendAlpha = BlendGet(r.e_SrcBlendAlpha);
				o.DestBlendAlpha = BlendGet(r.e_DestBlendAlpha);
				o.BlendOpAlpha = BlendOpGet(r.e_BlendOpAlpha);
				o.RenderTargetWriteMask = r.i_WriteMask;
			}
			if (FAILED(m_cls_Device->CreateBlendState(&bd, s->m_cls_State.GetAddressOf()))) return nullptr;
			return s;
		}

		DepthStencilStateRef D3D11Device::DepthStencilStateCreate(const DepthStencilDesc& desc)
		{
			auto s = std::make_shared<D3D11DepthStencilState>(desc);
			D3D11_DEPTH_STENCIL_DESC dd = {};
			dd.DepthEnable = desc.b_DepthEnable;
			dd.DepthWriteMask = desc.b_DepthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
			dd.DepthFunc = CompareGet(desc.e_DepthFunc);
			dd.StencilEnable = desc.b_StencilEnable;
			dd.StencilReadMask = desc.i_StencilReadMask;
			dd.StencilWriteMask = desc.i_StencilWriteMask;
			auto face = [](const StencilFaceDesc& f) {
				D3D11_DEPTH_STENCILOP_DESC o = {};
				o.StencilFailOp = StencilOpGet(f.e_FailOp);
				o.StencilDepthFailOp = StencilOpGet(f.e_DepthFailOp);
				o.StencilPassOp = StencilOpGet(f.e_PassOp);
				o.StencilFunc = CompareGet(f.e_Func);
				return o;
			};
			dd.FrontFace = face(desc.frontFace);
			dd.BackFace = face(desc.backFace);
			if (FAILED(m_cls_Device->CreateDepthStencilState(&dd, s->m_cls_State.GetAddressOf()))) return nullptr;
			return s;
		}

		RasterizerStateRef D3D11Device::RasterizerStateCreate(const RasterizerDesc& desc)
		{
			auto s = std::make_shared<D3D11RasterizerState>(desc);
			D3D11_RASTERIZER_DESC rd = {};
			rd.FillMode = (desc.e_Fill == FillMode::Wireframe) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
			rd.CullMode = (desc.e_Cull == CullMode::None) ? D3D11_CULL_NONE : (desc.e_Cull == CullMode::Front) ? D3D11_CULL_FRONT : D3D11_CULL_BACK;
			rd.FrontCounterClockwise = desc.b_FrontCounterClockwise;
			rd.DepthBias = desc.i_DepthBias;
			rd.DepthBiasClamp = desc.f_DepthBiasClamp;
			rd.SlopeScaledDepthBias = desc.f_SlopeScaledDepthBias;
			rd.DepthClipEnable = desc.b_DepthClip;
			rd.ScissorEnable = desc.b_Scissor;
			if (FAILED(m_cls_Device->CreateRasterizerState(&rd, s->m_cls_State.GetAddressOf()))) return nullptr;
			return s;
		}

		// ---- 更新 ------------------------------------------------------

		/*
		* 関数名　BufferUpdate
		* 引　数　buffer：対象　data / size / offset：書き込む範囲
		* 戻り値　なし
		* 説　明　Dynamic は Map、定数バッファは影コピーを経由した全体更新、それ以外は UpdateSubresource
		*/
		void D3D11Device::BufferUpdate(Buffer* buffer, const void* data, uint32_t size, uint32_t offset)
		{
			D3D11Buffer* b = BufCast(buffer);
			if (!b || !data) return;
			if (!b->m_Shadow.empty()) {
				const uint32_t n = (offset < b->m_Shadow.size()) ? std::min<uint32_t>(size, (uint32_t)b->m_Shadow.size() - offset) : 0;
				std::memcpy(b->m_Shadow.data() + offset, data, n);
				if (b->DescGet().e_Usage == Usage::Dynamic) {
					D3D11_MAPPED_SUBRESOURCE mp = {};
					if (SUCCEEDED(m_cls_Context->Map(b->m_cls_Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp))) {
						std::memcpy(mp.pData, b->m_Shadow.data(), b->m_Shadow.size());
						m_cls_Context->Unmap(b->m_cls_Buffer.Get(), 0);
					}
				}
				else {
					m_cls_Context->UpdateSubresource(b->m_cls_Buffer.Get(), 0, nullptr, b->m_Shadow.data(), 0, 0);
				}
				return;
			}
			if (offset == 0 && size >= b->GetSize()) {
				m_cls_Context->UpdateSubresource(b->m_cls_Buffer.Get(), 0, nullptr, data, 0, 0);
				return;
			}
			D3D11_BOX box = { offset, 0, 0, offset + size, 1, 1 };
			m_cls_Context->UpdateSubresource(b->m_cls_Buffer.Get(), 0, &box, data, 0, 0);
		}

		void* D3D11Device::BufferMap(Buffer* buffer)
		{
			D3D11Buffer* b = BufCast(buffer);
			if (!b) return nullptr;
			D3D11_MAPPED_SUBRESOURCE mp = {};
			if (FAILED(m_cls_Context->Map(b->m_cls_Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp))) return nullptr;
			return mp.pData;
		}

		void D3D11Device::BufferUnmap(Buffer* buffer)
		{
			if (D3D11Buffer* b = BufCast(buffer)) m_cls_Context->Unmap(b->m_cls_Buffer.Get(), 0);
		}

		void D3D11Device::TextureUpdate(Texture* texture, uint32_t mip, const void* data, uint32_t row_pitch)
		{
			D3D11Texture* t = TexCast(texture);
			if (!t || !data) return;
			m_cls_Context->UpdateSubresource(t->m_cls_Texture.Get(), mip, nullptr, data, row_pitch, 0);
		}

		void D3D11Device::TextureCopy(Texture* dst, Texture* src)
		{
			if (!dst || !src) return;
			m_cls_Context->CopyResource(TexCast(dst)->m_cls_Texture.Get(), TexCast(src)->m_cls_Texture.Get());
		}

		/*
		* 関数名　TextureRead
		* 引　数　texture：読み戻すテクスチャ　out_pixels / out_row_pitch：出力
		* 戻り値　成功なら true
		* 説　明　ステージングテクスチャへコピーして CPU で読む（ミップ 0 のみ）
		*/
		bool D3D11Device::TextureRead(Texture* texture, std::vector<uint8_t>& out_pixels, uint32_t& out_row_pitch)
		{
			D3D11Texture* t = TexCast(texture);
			if (!t || FormatInfoGet(t->GetFormat()).b_Compressed) return false;
			D3D11_TEXTURE2D_DESC td = {};
			t->m_cls_Texture->GetDesc(&td);
			td.MipLevels = 1;
			td.ArraySize = 1;
			td.Usage = D3D11_USAGE_STAGING;
			td.BindFlags = 0;
			td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			td.MiscFlags = 0;
			ComPtr<ID3D11Texture2D> staging;
			if (FAILED(m_cls_Device->CreateTexture2D(&td, nullptr, staging.GetAddressOf()))) return false;
			m_cls_Context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, t->m_cls_Texture.Get(), 0, nullptr);
			D3D11_MAPPED_SUBRESOURCE mp = {};
			if (FAILED(m_cls_Context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mp))) return false;
			out_row_pitch = FormatRowPitchGet(t->GetFormat(), td.Width);
			out_pixels.resize((size_t)out_row_pitch * td.Height);
			for (uint32_t y = 0; y < td.Height; ++y)
				std::memcpy(out_pixels.data() + (size_t)y * out_row_pitch, (const uint8_t*)mp.pData + (size_t)y * mp.RowPitch, out_row_pitch);
			m_cls_Context->Unmap(staging.Get(), 0);
			return true;
		}

		// ---- フレーム --------------------------------------------------

		void D3D11Device::Present()
		{
			if (m_cls_SwapChain) m_cls_SwapChain->Present(m_b_VSync ? 1 : 0, 0);
			++m_i_Frame;
		}

		/*
		* 関数名　SwapChainResize
		* 引　数　width / height：新しいサイズ
		* 戻り値　なし
		* 説　明　バックバッファへの参照を外してからサイズ変更し、再度包み直す
		*/
		void D3D11Device::SwapChainResize(uint32_t width, uint32_t height)
		{
			if (width == 0 || height == 0) return;
			m_cls_Context->OMSetRenderTargets(0, nullptr, nullptr);
			m_str_RenderTargets = RenderTargetState{};
			m_cls_BackBuffer.reset();
			if (m_b_Headless) {
				BackBufferCreate(width, height);
				return;
			}
			if (FAILED(m_cls_SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH))) {
				LogWrite("D3D11: ResizeBuffers 失敗");
				return;
			}
			BackBufferCreate(width, height);
		}

		// ---- 出力 ------------------------------------------------------

		void D3D11Device::RenderTargetsSet(uint32_t count, Texture* const* colors, Texture* depth)
		{
			ID3D11RenderTargetView* rtvs[MAX_RENDER_TARGETS] = {};
			m_str_RenderTargets = RenderTargetState{};
			count = std::min(count, MAX_RENDER_TARGETS);
			for (uint32_t i = 0; i < count; ++i) {
				m_str_RenderTargets.p_Colors[i] = colors ? colors[i] : nullptr;
				rtvs[i] = m_str_RenderTargets.p_Colors[i] ? TexCast(m_str_RenderTargets.p_Colors[i])->m_cls_Rtv.Get() : nullptr;
			}
			m_str_RenderTargets.i_Count = count;
			m_str_RenderTargets.p_Depth = depth;
			m_cls_Context->OMSetRenderTargets(count, rtvs, depth ? TexCast(depth)->m_cls_Dsv.Get() : nullptr);
		}

		void D3D11Device::RenderTargetClear(Texture* target, const float color[4])
		{
			if (target && TexCast(target)->m_cls_Rtv) m_cls_Context->ClearRenderTargetView(TexCast(target)->m_cls_Rtv.Get(), color);
		}

		void D3D11Device::DepthStencilClear(Texture* target, bool clear_depth, bool clear_stencil, float depth, uint8_t stencil)
		{
			if (!target || !TexCast(target)->m_cls_Dsv) return;
			UINT flags = (clear_depth ? D3D11_CLEAR_DEPTH : 0) | (clear_stencil ? D3D11_CLEAR_STENCIL : 0);
			m_cls_Context->ClearDepthStencilView(TexCast(target)->m_cls_Dsv.Get(), flags, depth, stencil);
		}

		void D3D11Device::ViewportSet(const Viewport& v)
		{
			m_str_Viewport = v;
			D3D11_VIEWPORT vp = { v.f_X, v.f_Y, v.f_Width, v.f_Height, v.f_MinDepth, v.f_MaxDepth };
			m_cls_Context->RSSetViewports(1, &vp);
		}

		void D3D11Device::ScissorSet(const Rect& r)
		{
			D3D11_RECT rc = { r.i_Left, r.i_Top, r.i_Right, r.i_Bottom };
			m_cls_Context->RSSetScissorRects(1, &rc);
		}

		// ---- ステート --------------------------------------------------

		void D3D11Device::BlendStateSet(BlendState* state, const float blend_factor[4], uint32_t sample_mask)
		{
			m_cls_Blend = state;
			const float zero[4] = { 0, 0, 0, 0 };
			m_cls_Context->OMSetBlendState(state ? static_cast<D3D11BlendState*>(state)->m_cls_State.Get() : nullptr,
				blend_factor ? blend_factor : zero, sample_mask);
		}

		void D3D11Device::DepthStencilStateSet(DepthStencilState* state, uint32_t stencil_ref)
		{
			m_cls_Depth = state;
			m_cls_Context->OMSetDepthStencilState(state ? static_cast<D3D11DepthStencilState*>(state)->m_cls_State.Get() : nullptr, stencil_ref);
		}

		void D3D11Device::RasterizerStateSet(RasterizerState* state)
		{
			m_cls_Raster = state;
			m_cls_Context->RSSetState(state ? static_cast<D3D11RasterizerState*>(state)->m_cls_State.Get() : nullptr);
		}

		void D3D11Device::ShaderSet(ShaderStage stage, Shader* shader)
		{
			D3D11Shader* s = static_cast<D3D11Shader*>(shader);
			if (stage == ShaderStage::Vertex) m_cls_Context->VSSetShader(s ? s->m_cls_Vs.Get() : nullptr, nullptr, 0);
			else m_cls_Context->PSSetShader(s ? s->m_cls_Ps.Get() : nullptr, nullptr, 0);
		}

		void D3D11Device::InputLayoutSet(InputLayout* layout)
		{
			m_cls_Context->IASetInputLayout(layout ? static_cast<D3D11InputLayout*>(layout)->m_cls_Layout.Get() : nullptr);
		}

		void D3D11Device::TopologySet(Topology topology)
		{
			m_cls_Context->IASetPrimitiveTopology(TopologyGet(topology));
		}

		void D3D11Device::VertexBufferSet(uint32_t slot, Buffer* buffer, uint32_t stride, uint32_t offset)
		{
			ID3D11Buffer* b = buffer ? BufCast(buffer)->m_cls_Buffer.Get() : nullptr;
			m_cls_Context->IASetVertexBuffers(slot, 1, &b, &stride, &offset);
		}

		void D3D11Device::IndexBufferSet(Buffer* buffer, IndexFormat format, uint32_t offset)
		{
			m_cls_Context->IASetIndexBuffer(buffer ? BufCast(buffer)->m_cls_Buffer.Get() : nullptr,
				format == IndexFormat::UInt16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, offset);
		}

		void D3D11Device::ConstantBufferSet(ShaderStage stage, uint32_t slot, Buffer* buffer)
		{
			ID3D11Buffer* b = buffer ? BufCast(buffer)->m_cls_Buffer.Get() : nullptr;
			if (stage == ShaderStage::Vertex) m_cls_Context->VSSetConstantBuffers(slot, 1, &b);
			else m_cls_Context->PSSetConstantBuffers(slot, 1, &b);
		}

		void D3D11Device::TextureSet(ShaderStage stage, uint32_t slot, Texture* texture)
		{
			ID3D11ShaderResourceView* srv = texture ? TexCast(texture)->m_cls_Srv.Get() : nullptr;
			if (stage == ShaderStage::Vertex) m_cls_Context->VSSetShaderResources(slot, 1, &srv);
			else m_cls_Context->PSSetShaderResources(slot, 1, &srv);
		}

		void D3D11Device::SamplerSet(ShaderStage stage, uint32_t slot, Sampler* sampler)
		{
			ID3D11SamplerState* s = sampler ? static_cast<D3D11Sampler*>(sampler)->m_cls_Sampler.Get() : nullptr;
			if (stage == ShaderStage::Vertex) m_cls_Context->VSSetSamplers(slot, 1, &s);
			else m_cls_Context->PSSetSamplers(slot, 1, &s);
		}

		void D3D11Device::StateClear()
		{
			m_cls_Context->ClearState();
			m_str_RenderTargets = RenderTargetState{};
			m_cls_Blend = nullptr;
			m_cls_Depth = nullptr;
			m_cls_Raster = nullptr;
		}

		// ---- 描画 ------------------------------------------------------

		void D3D11Device::Draw(uint32_t vertex_count, uint32_t start_vertex)
		{
			m_cls_Context->Draw(vertex_count, start_vertex);
		}

		void D3D11Device::IndexedDraw(uint32_t index_count, uint32_t start_index, int32_t base_vertex)
		{
			m_cls_Context->DrawIndexed(index_count, start_index, base_vertex);
		}

		void D3D11Device::InstancedDraw(uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex, uint32_t start_instance)
		{
			m_cls_Context->DrawInstanced(vertex_count, instance_count, start_vertex, start_instance);
		}

		void D3D11Device::MarkerBegin(const char* name)
		{
			if (!m_cls_Annotation || !name) return;
			wchar_t wname[128] = {};
			MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, 127);
			m_cls_Annotation->BeginEvent(wname);
		}

		void D3D11Device::MarkerEnd()
		{
			if (m_cls_Annotation) m_cls_Annotation->EndEvent();
		}
	}

	// ================================================================
	// 公開関数
	// ================================================================

	std::unique_ptr<Device> D3D11DeviceCreate(const DeviceCreateInfo& info, std::string* out_error)
	{
		auto device = std::make_unique<D3D11Device>();
		if (!device->Init(info, out_error)) return nullptr;
		LogWrite("D3D11: %s", device->AdapterNameGet());
		return device;
	}

	ID3D11ShaderResourceView* D3D11SrvGet(Texture* texture)
	{
		return texture ? TexCast(texture)->m_cls_Srv.Get() : nullptr;
	}

	ID3D11Resource* D3D11ResourceGet(Texture* texture)
	{
		return texture ? TexCast(texture)->m_cls_Texture.Get() : nullptr;
	}

	/*
	* 関数名　D3D11TextureWrap
	* 引　数　device：デバイス　srv：既存の SRV
	* 戻り値　RHI テクスチャ
	* 説　明　外部ライブラリが作った SRV を RHI のテクスチャとして扱えるようにする
	*/
	TextureRef D3D11TextureWrap(Device* device, ID3D11ShaderResourceView* srv)
	{
		(void)device;
		if (!srv) return nullptr;
		ComPtr<ID3D11Resource> res;
		srv->GetResource(res.GetAddressOf());
		ComPtr<ID3D11Texture2D> tex2d;
		if (FAILED(res.As(&tex2d))) return nullptr;
		D3D11_TEXTURE2D_DESC td = {};
		tex2d->GetDesc(&td);
		TextureDesc desc;
		desc.i_Width = td.Width;
		desc.i_Height = td.Height;
		desc.i_MipLevels = td.MipLevels;
		desc.i_ArraySize = td.ArraySize;
		desc.e_Format = FormatFromDxgi(td.Format);
		desc.i_BindFlags = Bind_ShaderResource;
		auto t = std::make_shared<D3D11Texture>(desc);
		t->m_cls_Texture = tex2d;
		t->m_cls_Srv = srv;
		return t;
	}
}
