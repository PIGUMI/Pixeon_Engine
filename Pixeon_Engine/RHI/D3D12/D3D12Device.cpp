/*
* ファイル名　D3D12Device
* 説　　　明　RHI の Direct3D 12 バックエンド
*
* D3D11 方式のイミディエイト API を次の仕組みでエミュレートする
*   コマンド      : フレームごとのコマンドアロケータ + 1 本のコマンドリスト
*   定数バッファ  : CPU 側に内容を持ち、Draw 時に変更があればアップロードリングへ書き出して
*                   ルート CBV として渡す（D3D11 の UpdateSubresource と同じ結果になる）
*   Dynamic バッファ : 同様にアップロードリングへ書き出して頂点 / インデックスバッファとして使う
*   ディスクリプタ: テクスチャ・サンプラーは CPU ヒープに作っておき、Draw 時にシェーダー可視ヒープの
*                   リングへコピーしてテーブルとして渡す（サンプラーは組み合わせごとにキャッシュ）
*   PSO           : Draw 時点のステートからキーを作ってキャッシュ
*   状態遷移      : テクスチャは状態を追跡して自動でバリアを張る。バッファはコマンドリスト実行後に
*                   COMMON へ戻る (decay) ため、提出ごとに COMMON とみなす
*   破棄          : GPU が使い終わるまで（フェンスで確認）実体の解放を遅延する
*
* ルートシグネチャ（ステージごとに D3D11 と同じスロット番号を使う）
*   [0..7]   VS b0-b7 ルート CBV     [8..15]  PS b0-b7 ルート CBV
*   [16]     VS t0-t15 テーブル      [17]     PS t0-t15 テーブル
*   [18]     VS s0-s3 テーブル       [19]     PS s0-s3 テーブル
*/
#include "../RHI.h"
#include "../DXGI/DXGICommon.h"
#include "../../../Common/ShaderBindings.h"
#include "D3D12Native.h"
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_5.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

#if defined(_MSC_VER)
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#endif

using Microsoft::WRL::ComPtr;

namespace RHI {
	namespace {
		constexpr uint32_t STAGES = ShaderBindings::STAGE_COUNT;
		constexpr uint32_t CB_SLOTS = ShaderBindings::CB_SLOT_COUNT;
		constexpr uint32_t SRV_SLOTS = ShaderBindings::SRV_SLOT_COUNT;
		constexpr uint32_t SMP_SLOTS = ShaderBindings::SAMPLER_SLOT_COUNT;

		constexpr uint32_t ROOT_CBV_BASE = 0;                          // 0..15
		constexpr uint32_t ROOT_SRV_TABLE = ROOT_CBV_BASE + STAGES * CB_SLOTS;   // 16, 17
		constexpr uint32_t ROOT_SMP_TABLE = ROOT_SRV_TABLE + STAGES;   // 18, 19

		constexpr uint32_t GPU_SRV_HEAP_SIZE = 65536;      // シェーダー可視 CBV/SRV/UAV ヒープ
		constexpr uint32_t GPU_SAMPLER_HEAP_SIZE = 2048;   // シェーダー可視サンプラーヒープ (上限)
		constexpr uint64_t UPLOAD_PAGE_SIZE = 4ull * 1024 * 1024;
		constexpr uint32_t DUMMY_CB_SIZE = 65536;          // 未設定スロット用（CB の最大サイズ）

		uint64_t ObjectIdNext()
		{
			static uint64_t s_id = 0;
			return ++s_id;
		}

		// ---------------------------------------------------------------
		// 列挙型の変換
		// ---------------------------------------------------------------
		D3D12_BLEND BlendGet(Blend b)
		{
			static const D3D12_BLEND table[] = {
				D3D12_BLEND_ZERO, D3D12_BLEND_ONE, D3D12_BLEND_SRC_COLOR, D3D12_BLEND_INV_SRC_COLOR,
				D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_DEST_ALPHA, D3D12_BLEND_INV_DEST_ALPHA,
				D3D12_BLEND_DEST_COLOR, D3D12_BLEND_INV_DEST_COLOR, D3D12_BLEND_SRC_ALPHA_SAT,
				D3D12_BLEND_BLEND_FACTOR, D3D12_BLEND_INV_BLEND_FACTOR,
			};
			return table[(int)b];
		}
		D3D12_BLEND_OP BlendOpGet(BlendOp o)
		{
			static const D3D12_BLEND_OP table[] = {
				D3D12_BLEND_OP_ADD, D3D12_BLEND_OP_SUBTRACT, D3D12_BLEND_OP_REV_SUBTRACT, D3D12_BLEND_OP_MIN, D3D12_BLEND_OP_MAX,
			};
			return table[(int)o];
		}
		D3D12_COMPARISON_FUNC CompareGet(CompareFunc c)
		{
			static const D3D12_COMPARISON_FUNC table[] = {
				D3D12_COMPARISON_FUNC_NEVER, D3D12_COMPARISON_FUNC_LESS, D3D12_COMPARISON_FUNC_EQUAL, D3D12_COMPARISON_FUNC_LESS_EQUAL,
				D3D12_COMPARISON_FUNC_GREATER, D3D12_COMPARISON_FUNC_NOT_EQUAL, D3D12_COMPARISON_FUNC_GREATER_EQUAL, D3D12_COMPARISON_FUNC_ALWAYS,
			};
			return table[(int)c];
		}
		D3D12_STENCIL_OP StencilOpGet(StencilOp o)
		{
			static const D3D12_STENCIL_OP table[] = {
				D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_STENCIL_OP_REPLACE, D3D12_STENCIL_OP_INCR_SAT,
				D3D12_STENCIL_OP_DECR_SAT, D3D12_STENCIL_OP_INVERT, D3D12_STENCIL_OP_INCR, D3D12_STENCIL_OP_DECR,
			};
			return table[(int)o];
		}
		D3D12_FILTER FilterGet(Filter f)
		{
			switch (f) {
			case Filter::Point:                          return D3D12_FILTER_MIN_MAG_MIP_POINT;
			case Filter::MinMagLinearMipPoint:           return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			case Filter::Anisotropic:                    return D3D12_FILTER_ANISOTROPIC;
			case Filter::ComparisonPoint:                return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
			case Filter::ComparisonLinear:               return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
			case Filter::ComparisonMinMagLinearMipPoint: return D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
			default:                                     return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			}
		}
		D3D12_TEXTURE_ADDRESS_MODE AddressGet(AddressMode a)
		{
			static const D3D12_TEXTURE_ADDRESS_MODE table[] = {
				D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_BORDER,
			};
			return table[(int)a];
		}
		D3D12_PRIMITIVE_TOPOLOGY TopologyGet(Topology t)
		{
			switch (t) {
			case Topology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			case Topology::LineList:      return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
			case Topology::LineStrip:     return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
			case Topology::PointList:     return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
			default:                      return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			}
		}
		D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyTypeGet(Topology t)
		{
			switch (t) {
			case Topology::LineList:
			case Topology::LineStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
			case Topology::PointList: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
			default:                  return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			}
		}

		D3D12_HEAP_PROPERTIES HeapPropsGet(D3D12_HEAP_TYPE type)
		{
			D3D12_HEAP_PROPERTIES p = {};
			p.Type = type;
			p.CreationNodeMask = 1;
			p.VisibleNodeMask = 1;
			return p;
		}

		D3D12_RESOURCE_DESC BufferDescGet(uint64_t size)
		{
			D3D12_RESOURCE_DESC d = {};
			d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			d.Width = size;
			d.Height = 1;
			d.DepthOrArraySize = 1;
			d.MipLevels = 1;
			d.SampleDesc.Count = 1;
			d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			return d;
		}

		// ---------------------------------------------------------------
		// CPU 専用ディスクリプタヒープ（固定長ページ + 空きリスト）
		// ---------------------------------------------------------------
		class CpuDescriptorAllocator {
		public:
			void Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type)
			{
				m_p_Device = device;
				m_e_Type = type;
				m_i_Increment = device->GetDescriptorHandleIncrementSize(type);
			}
			D3D12_CPU_DESCRIPTOR_HANDLE Allocate()
			{
				std::lock_guard<std::mutex> lock(m_Mutex);
				if (m_Free.empty()) PageAdd();
				if (m_Free.empty()) return D3D12_CPU_DESCRIPTOR_HANDLE{ 0 };
				D3D12_CPU_DESCRIPTOR_HANDLE h = m_Free.back();
				m_Free.pop_back();
				return h;
			}
			void Free(D3D12_CPU_DESCRIPTOR_HANDLE h)
			{
				if (h.ptr == 0) return;
				std::lock_guard<std::mutex> lock(m_Mutex);
				m_Free.push_back(h);
			}
		private:
			void PageAdd()
			{
				constexpr uint32_t PAGE = 1024;
				D3D12_DESCRIPTOR_HEAP_DESC hd = {};
				hd.Type = m_e_Type;
				hd.NumDescriptors = PAGE;
				ComPtr<ID3D12DescriptorHeap> heap;
				if (FAILED(m_p_Device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)))) return;
				D3D12_CPU_DESCRIPTOR_HANDLE base = heap->GetCPUDescriptorHandleForHeapStart();
				for (uint32_t i = PAGE; i-- > 0;) m_Free.push_back(D3D12_CPU_DESCRIPTOR_HANDLE{ base.ptr + (SIZE_T)i * m_i_Increment });
				m_Heaps.push_back(heap);
			}
			ID3D12Device* m_p_Device = nullptr;
			D3D12_DESCRIPTOR_HEAP_TYPE m_e_Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			uint32_t m_i_Increment = 0;
			std::vector<ComPtr<ID3D12DescriptorHeap>> m_Heaps;
			std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_Free;
			std::mutex m_Mutex;
		};

		// ---------------------------------------------------------------
		// デバイス本体の共有部分
		// リソースはこれを shared_ptr で持ち、破棄時に遅延解放リストへ積む
		// （デバイスより後にリソースが破棄されても安全）
		// ---------------------------------------------------------------
		struct D3D12Core {
			ComPtr<ID3D12Device> cls_Device;
			ComPtr<ID3D12Fence> cls_Fence;
			uint64_t i_NextFenceValue = 1;    // 次に Signal する値（＝現在記録中のコマンドの完了値）
			CpuDescriptorAllocator cls_RtvAlloc;
			CpuDescriptorAllocator cls_DsvAlloc;
			CpuDescriptorAllocator cls_SrvAlloc;
			CpuDescriptorAllocator cls_SamplerAlloc;

			struct Pending { uint64_t i_Fence; ComPtr<IUnknown> cls_Object; };
			std::deque<Pending> deferred;
			std::mutex mutex;

			// デバイス操作の排他（別スレッドでのリソース生成に備える）と、
			// 破棄されたリソースをデバイスのバインド状態から外すためのフック
			std::recursive_mutex deviceMutex;
			void* p_Owner = nullptr;
			void (*p_UnbindFn)(void* owner, const void* resource) = nullptr;
			void Unbind(const void* resource)
			{
				std::lock_guard<std::recursive_mutex> lock(deviceMutex);
				if (p_Owner && p_UnbindFn) p_UnbindFn(p_Owner, resource);
			}

			void DeferredRelease(ComPtr<IUnknown> object)
			{
				if (!object) return;
				std::lock_guard<std::mutex> lock(mutex);
				deferred.push_back({ i_NextFenceValue, std::move(object) });
			}
			void DeferredCollect()
			{
				const uint64_t done = cls_Fence ? cls_Fence->GetCompletedValue() : UINT64_MAX;
				std::lock_guard<std::mutex> lock(mutex);
				while (!deferred.empty() && deferred.front().i_Fence <= done) deferred.pop_front();
			}
		};
		using CoreRef = std::shared_ptr<D3D12Core>;

		// ---------------------------------------------------------------
		// リソース
		// ---------------------------------------------------------------
		class D3D12Buffer final : public Buffer {
		public:
			D3D12Buffer(const BufferDesc& desc, CoreRef core) : Buffer(desc), m_cls_Core(std::move(core)) {}
			~D3D12Buffer() override
			{
				m_cls_Core->Unbind(static_cast<Buffer*>(this));
				m_cls_Core->DeferredRelease(m_cls_Resource);
			}

			// GPU 上に実体を持たず、使うたびにアップロードリングへ書き出すバッファか
			bool IsStreamed() const { return !m_Shadow.empty(); }

			CoreRef m_cls_Core;
			ComPtr<ID3D12Resource> m_cls_Resource;     // Default ヒープ（Streamed では空）
			D3D12_RESOURCE_STATES m_e_State = D3D12_RESOURCE_STATE_COMMON;
			uint64_t m_i_StateSubmission = 0;          // m_e_State が有効な提出番号
			std::vector<uint8_t> m_Shadow;             // Streamed の内容
			bool m_b_Dirty = true;
			uint64_t m_i_UploadFrame = UINT64_MAX;
			D3D12_GPU_VIRTUAL_ADDRESS m_i_GpuAddress = 0;
		};

		class D3D12Texture final : public Texture {
		public:
			D3D12Texture(const TextureDesc& desc, CoreRef core) : Texture(desc), m_cls_Core(std::move(core)) {}
			~D3D12Texture() override
			{
				m_cls_Core->Unbind(static_cast<Texture*>(this));
				if (!m_b_External) m_cls_Core->DeferredRelease(m_cls_Resource);
				if (m_b_OwnsViews) ViewsFree();
			}
			void ViewsFree()
			{
				m_cls_Core->cls_SrvAlloc.Free(m_str_Srv);
				m_cls_Core->cls_RtvAlloc.Free(m_str_Rtv);
				m_cls_Core->cls_DsvAlloc.Free(m_str_Dsv);
				m_str_Srv = m_str_Rtv = m_str_Dsv = D3D12_CPU_DESCRIPTOR_HANDLE{ 0 };
			}
			void SizeSet(uint32_t w, uint32_t h) { m_str_Desc.i_Width = w; m_str_Desc.i_Height = h; }

			CoreRef m_cls_Core;
			ComPtr<ID3D12Resource> m_cls_Resource;
			D3D12_RESOURCE_STATES m_e_State = D3D12_RESOURCE_STATE_COMMON;
			D3D12_CPU_DESCRIPTOR_HANDLE m_str_Srv = { 0 };
			D3D12_CPU_DESCRIPTOR_HANDLE m_str_Rtv = { 0 };
			D3D12_CPU_DESCRIPTOR_HANDLE m_str_Dsv = { 0 };
			bool m_b_OwnsViews = true;
			bool m_b_External = false;   // スワップチェーンのバッファ（解放はデバイスが行う）
		};

		class D3D12Sampler final : public Sampler {
		public:
			D3D12Sampler(const SamplerDesc& desc, CoreRef core) : Sampler(desc), m_cls_Core(std::move(core)), m_i_Id(ObjectIdNext()) {}
			~D3D12Sampler() override
			{
				m_cls_Core->Unbind(static_cast<Sampler*>(this));
				m_cls_Core->cls_SamplerAlloc.Free(m_str_Handle);
			}
			CoreRef m_cls_Core;
			D3D12_CPU_DESCRIPTOR_HANDLE m_str_Handle = { 0 };
			uint64_t m_i_Id;
		};

		class D3D12Shader final : public Shader {
		public:
			D3D12Shader(ShaderStage stage, const void* code, size_t size, const char* name)
				: Shader(stage, code, size, name), m_i_Id(ObjectIdNext())
			{
				DxbcReflect(code, size, m_str_Reflection);
			}
			uint64_t m_i_Id;
		};

		class D3D12InputLayout final : public InputLayout {
		public:
			D3D12InputLayout(const InputElement* e, uint32_t n) : InputLayout(e, n), m_i_Id(ObjectIdNext())
			{
				for (auto& el : m_Elements) {
					D3D12_INPUT_ELEMENT_DESC d = {};
					d.SemanticName = el.c_Semantic;
					d.SemanticIndex = el.i_SemanticIndex;
					d.Format = DxgiFormatGet(el.e_Format);
					d.InputSlot = el.i_Slot;
					d.AlignedByteOffset = el.i_Offset;
					d.InputSlotClass = (el.e_Rate == InputRate::PerInstance) ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
					d.InstanceDataStepRate = el.i_InstanceStep;
					m_Descs.push_back(d);
				}
			}
			std::vector<D3D12_INPUT_ELEMENT_DESC> m_Descs;
			uint64_t m_i_Id;
		};

		class D3D12BlendState final : public BlendState {
		public:
			explicit D3D12BlendState(const BlendDesc& d) : BlendState(d)
			{
				m_str_Native.AlphaToCoverageEnable = d.b_AlphaToCoverage;
				m_str_Native.IndependentBlendEnable = d.b_IndependentBlend;
				for (uint32_t i = 0; i < MAX_RENDER_TARGETS; ++i) {
					const auto& r = d.renderTarget[i];
					auto& o = m_str_Native.RenderTarget[i];
					o.BlendEnable = r.b_BlendEnable;
					o.SrcBlend = BlendGet(r.e_SrcBlend);
					o.DestBlend = BlendGet(r.e_DestBlend);
					o.BlendOp = BlendOpGet(r.e_BlendOp);
					o.SrcBlendAlpha = BlendGet(r.e_SrcBlendAlpha);
					o.DestBlendAlpha = BlendGet(r.e_DestBlendAlpha);
					o.BlendOpAlpha = BlendOpGet(r.e_BlendOpAlpha);
					o.LogicOp = D3D12_LOGIC_OP_NOOP;
					o.RenderTargetWriteMask = r.i_WriteMask;
				}
			}
			D3D12_BLEND_DESC m_str_Native = {};
		};

		class D3D12DepthStencilState final : public DepthStencilState {
		public:
			explicit D3D12DepthStencilState(const DepthStencilDesc& d) : DepthStencilState(d)
			{
				m_str_Native.DepthEnable = d.b_DepthEnable;
				m_str_Native.DepthWriteMask = d.b_DepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
				m_str_Native.DepthFunc = CompareGet(d.e_DepthFunc);
				m_str_Native.StencilEnable = d.b_StencilEnable;
				m_str_Native.StencilReadMask = d.i_StencilReadMask;
				m_str_Native.StencilWriteMask = d.i_StencilWriteMask;
				auto face = [](const StencilFaceDesc& f) {
					D3D12_DEPTH_STENCILOP_DESC o = {};
					o.StencilFailOp = StencilOpGet(f.e_FailOp);
					o.StencilDepthFailOp = StencilOpGet(f.e_DepthFailOp);
					o.StencilPassOp = StencilOpGet(f.e_PassOp);
					o.StencilFunc = CompareGet(f.e_Func);
					return o;
				};
				m_str_Native.FrontFace = face(d.frontFace);
				m_str_Native.BackFace = face(d.backFace);
			}
			D3D12_DEPTH_STENCIL_DESC m_str_Native = {};
		};

		class D3D12RasterizerState final : public RasterizerState {
		public:
			explicit D3D12RasterizerState(const RasterizerDesc& d) : RasterizerState(d)
			{
				m_str_Native.FillMode = (d.e_Fill == FillMode::Wireframe) ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
				m_str_Native.CullMode = (d.e_Cull == CullMode::None) ? D3D12_CULL_MODE_NONE : (d.e_Cull == CullMode::Front) ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_BACK;
				m_str_Native.FrontCounterClockwise = d.b_FrontCounterClockwise;
				m_str_Native.DepthBias = d.i_DepthBias;
				m_str_Native.DepthBiasClamp = d.f_DepthBiasClamp;
				m_str_Native.SlopeScaledDepthBias = d.f_SlopeScaledDepthBias;
				m_str_Native.DepthClipEnable = d.b_DepthClip;
			}
			D3D12_RASTERIZER_DESC m_str_Native = {};
		};

#define D3D12_LOCK std::lock_guard<std::recursive_mutex> lock_(m_cls_Core->deviceMutex)

		D3D12Texture* TexCast(Texture* t) { return static_cast<D3D12Texture*>(t); }
		D3D12Buffer* BufCast(Buffer* b) { return static_cast<D3D12Buffer*>(b); }

		// 既定ステート（nullptr を Set したとき = D3D11 の既定値）
		const D3D12BlendState& DefaultBlendGet() { static D3D12BlendState s{ BlendDesc{} }; return s; }
		const D3D12DepthStencilState& DefaultDepthGet() { static D3D12DepthStencilState s{ DepthStencilDesc{} }; return s; }
		const D3D12RasterizerState& DefaultRasterGet() { static D3D12RasterizerState s{ RasterizerDesc{} }; return s; }

		// PSO キャッシュのキー（そのままバイト列として比較する）
		struct PipelineKey {
			uint64_t i_Vs = 0, i_Ps = 0, i_Layout = 0;
			D3D12_BLEND_DESC blend;
			D3D12_DEPTH_STENCIL_DESC depth;
			D3D12_RASTERIZER_DESC raster;
			uint32_t i_SampleMask = 0;
			uint32_t i_TopologyType = 0;
			uint32_t i_RtCount = 0;
			DXGI_FORMAT e_Rtv[MAX_RENDER_TARGETS];
			DXGI_FORMAT e_Dsv;
		};

		// ---------------------------------------------------------------
		// デバイス
		// ---------------------------------------------------------------
		class D3D12Device final : public Device {
		public:
			~D3D12Device() override;
			bool Init(const DeviceCreateInfo& info, std::string* out_error);

			Backend BackendGet() const override { return Backend::D3D12; }
			const char* AdapterNameGet() const override { return m_AdapterName.c_str(); }
			NativeHandles NativeHandlesGet() const override;
			uint64_t FrameIndexGet() const override { return m_i_Frame; }

			BufferRef BufferCreate(const BufferDesc& desc, const void* initial_data) override;
			TextureRef TextureCreate(const TextureDesc& desc, const SubresourceData* initial_data) override;
			SamplerRef SamplerCreate(const SamplerDesc& desc) override;
			ShaderRef ShaderCreate(ShaderStage stage, const void* bytecode, size_t size, const char* name) override;
			InputLayoutRef InputLayoutCreate(const InputElement* elements, uint32_t count, const Shader* vs) override;
			BlendStateRef BlendStateCreate(const BlendDesc& desc) override { return std::make_shared<D3D12BlendState>(desc); }
			DepthStencilStateRef DepthStencilStateCreate(const DepthStencilDesc& desc) override { return std::make_shared<D3D12DepthStencilState>(desc); }
			RasterizerStateRef RasterizerStateCreate(const RasterizerDesc& desc) override { return std::make_shared<D3D12RasterizerState>(desc); }

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
			void IdleWait() override;

			void RenderTargetsSet(uint32_t count, Texture* const* colors, Texture* depth) override;
			RenderTargetState RenderTargetsGet() const override { return m_str_RenderTargets; }
			void RenderTargetClear(Texture* target, const float color[4]) override;
			void DepthStencilClear(Texture* target, bool clear_depth, bool clear_stencil, float depth, uint8_t stencil) override;
			void ViewportSet(const Viewport& viewport) override { m_str_Viewport = viewport; m_b_ViewportDirty = true; }
			Viewport ViewportGet() const override { return m_str_Viewport; }
			void ScissorSet(const Rect& rect) override { m_str_Scissor = rect; m_b_ScissorDirty = true; }

			void BlendStateSet(BlendState* state, const float blend_factor[4], uint32_t sample_mask) override;
			BlendState* BlendStateGet() const override { return m_cls_Blend; }
			void DepthStencilStateSet(DepthStencilState* state, uint32_t stencil_ref) override;
			DepthStencilState* DepthStencilStateGet() const override { return m_cls_Depth; }
			void RasterizerStateSet(RasterizerState* state) override { m_cls_Raster = state; m_b_ScissorDirty = true; }
			RasterizerState* RasterizerStateGet() const override { return m_cls_Raster; }

			void ShaderSet(ShaderStage stage, Shader* shader) override;
			void InputLayoutSet(InputLayout* layout) override { m_cls_Layout = static_cast<D3D12InputLayout*>(layout); }
			void TopologySet(Topology topology) override { m_e_Topology = topology; m_b_TopologyDirty = true; }
			void VertexBufferSet(uint32_t slot, Buffer* buffer, uint32_t stride, uint32_t offset) override;
			void IndexBufferSet(Buffer* buffer, IndexFormat format, uint32_t offset) override;
			void ConstantBufferSet(ShaderStage stage, uint32_t slot, Buffer* buffer) override;
			void TextureSet(ShaderStage stage, uint32_t slot, Texture* texture) override;
			void SamplerSet(ShaderStage stage, uint32_t slot, Sampler* sampler) override;
			void StateClear() override;

			void Draw(uint32_t vertex_count, uint32_t start_vertex) override;
			void IndexedDraw(uint32_t index_count, uint32_t start_index, int32_t base_vertex) override;
			void InstancedDraw(uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex, uint32_t start_instance) override;

			void* NativeCommandsBegin() override;
			void NativeCommandsEnd() override;
			void MarkerBegin(const char* name) override;
			void MarkerEnd() override;

			uint32_t FramesInFlightGet() const { return m_i_FrameCount; }

		private:
			struct UploadPage {
				ComPtr<ID3D12Resource> cls_Resource;
				uint8_t* p_Cpu = nullptr;
				D3D12_GPU_VIRTUAL_ADDRESS i_Gpu = 0;
				uint64_t i_Size = 0;
				uint64_t i_Offset = 0;
			};
			struct UploadAlloc {
				ID3D12Resource* p_Resource = nullptr;
				uint64_t i_Offset = 0;
				uint8_t* p_Cpu = nullptr;
				D3D12_GPU_VIRTUAL_ADDRESS i_Gpu = 0;
			};
			struct FrameContext {
				ComPtr<ID3D12CommandAllocator> cls_Allocator;
				uint64_t i_FenceValue = 0;
				std::vector<UploadPage> uploadPages;   // このフレームで使ったページ
				uint32_t i_SrvRingStart = 0;           // GPU SRV ヒープのリングの区間
			};

			bool SwapChainCreate(const DeviceCreateInfo& info);
			bool BackBuffersAcquire();
			void BackBuffersRelease();
			bool RootSignatureCreate();
			bool TextureViewsCreate(D3D12Texture* tex);

			void CommandsBegin();
			uint64_t CommandsSubmit();
			void FenceWait(uint64_t value);
			void FrameBegin();

			UploadAlloc UploadAllocate(uint64_t size, uint64_t alignment);
			D3D12_GPU_VIRTUAL_ADDRESS StreamedBufferResolve(D3D12Buffer* buffer);
			void BufferTransition(D3D12Buffer* buffer, D3D12_RESOURCE_STATES state);
			void TextureTransition(D3D12Texture* tex, D3D12_RESOURCE_STATES state);
			void BarriersFlush();
			bool DrawPrepare();
			ID3D12PipelineState* PipelineGet();
			D3D12_GPU_DESCRIPTOR_HANDLE SamplerTableGet(uint32_t stage);
			void AllDirtyMark();
			void BoundTextureUnbind(Texture* texture);
			void ResourceUnbind(const void* resource);

			CoreRef m_cls_Core;
			ComPtr<IDXGIFactory4> m_cls_Factory;
			ComPtr<ID3D12CommandQueue> m_cls_Queue;
			ComPtr<ID3D12GraphicsCommandList> m_cls_List;
			ComPtr<IDXGISwapChain3> m_cls_SwapChain;
			ComPtr<ID3D12RootSignature> m_cls_RootSignature;
			ComPtr<ID3D12DescriptorHeap> m_cls_GpuSrvHeap;
			ComPtr<ID3D12DescriptorHeap> m_cls_GpuSamplerHeap;
			ComPtr<ID3D12Resource> m_cls_DummyCb;
			HANDLE m_p_FenceEvent = nullptr;
			std::string m_AdapterName;

			std::vector<FrameContext> m_Frames;
			uint32_t m_i_FrameCount = 2;
			uint32_t m_i_FrameSlot = 0;
			uint64_t m_i_Frame = 0;
			uint64_t m_i_Submission = 1;   // 提出ごとに増える（バッファの状態の有効期限）
			bool m_b_ListOpen = false;
			std::vector<UploadPage> m_FreePages;

			// スワップチェーン
			std::vector<ComPtr<ID3D12Resource>> m_BackBufferResources;
			std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_BackBufferRtvs;
			std::vector<D3D12_RESOURCE_STATES> m_BackBufferStates;
			std::shared_ptr<D3D12Texture> m_cls_BackBuffer;   // 現在のバッファを指す代理テクスチャ
			uint32_t m_i_BackBufferIndex = 0;
			bool m_b_Headless = false;
			bool m_b_VSync = true;
			bool m_b_TearingSupported = false;

			// ディスクリプタ
			uint32_t m_i_SrvIncrement = 0;
			uint32_t m_i_SamplerIncrement = 0;
			uint32_t m_i_SrvRingHead = 0;
			D3D12_CPU_DESCRIPTOR_HANDLE m_str_NullSrv = { 0 };
			D3D12_CPU_DESCRIPTOR_HANDLE m_str_NullRtv = { 0 };
			D3D12_CPU_DESCRIPTOR_HANDLE m_str_NullSampler = { 0 };
			std::unordered_map<std::string, uint32_t> m_SamplerTables;   // 組み合わせ → GPU ヒープ内の位置
			uint32_t m_i_SamplerHeapUsed = 0;

			// パイプライン
			std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_Pipelines;
			ID3D12PipelineState* m_p_CurrentPipeline = nullptr;

			// D3D11 方式のステート
			std::vector<D3D12_RESOURCE_BARRIER> m_Barriers;
			RenderTargetState m_str_RenderTargets;
			Viewport m_str_Viewport;
			Rect m_str_Scissor;
			BlendState* m_cls_Blend = nullptr;
			DepthStencilState* m_cls_Depth = nullptr;
			RasterizerState* m_cls_Raster = nullptr;
			float m_f_BlendFactor[4] = {};
			uint32_t m_i_SampleMask = 0xFFFFFFFFu;
			uint32_t m_i_StencilRef = 0;
			D3D12Shader* m_cls_Shaders[STAGES] = {};
			D3D12InputLayout* m_cls_Layout = nullptr;
			Topology m_e_Topology = Topology::TriangleList;
			struct VertexBinding { D3D12Buffer* p_Buffer = nullptr; uint32_t i_Stride = 0; uint32_t i_Offset = 0; };
			VertexBinding m_VertexBuffers[16];
			D3D12Buffer* m_p_IndexBuffer = nullptr;
			IndexFormat m_e_IndexFormat = IndexFormat::UInt16;
			uint32_t m_i_IndexOffset = 0;
			D3D12Buffer* m_ConstantBuffers[STAGES][CB_SLOTS] = {};
			Texture* m_Textures[STAGES][SRV_SLOTS] = {};
			D3D12Sampler* m_Samplers[STAGES][SMP_SLOTS] = {};

			bool m_b_RenderTargetsDirty = true;
			bool m_b_ViewportDirty = true;
			bool m_b_ScissorDirty = true;
			bool m_b_TopologyDirty = true;
			bool m_b_RootDirty = true;
			bool m_b_VertexBuffersDirty = true;
			bool m_b_IndexBufferDirty = true;
			bool m_b_SrvDirty[STAGES] = { true, true };
			bool m_b_SamplerDirty[STAGES] = { true, true };
			bool m_b_BlendFactorDirty = true;
			bool m_b_StencilRefDirty = true;
		};

		D3D12Device::~D3D12Device()
		{
			if (m_cls_Core) m_cls_Core->p_Owner = nullptr;
			if (m_cls_Queue && m_cls_Core && m_cls_Core->cls_Fence) {
				if (m_b_ListOpen) CommandsSubmit();
				FenceWait(m_cls_Core->i_NextFenceValue - 1);
			}
			if (m_cls_SwapChain) m_cls_SwapChain->SetFullscreenState(FALSE, nullptr);
			m_cls_BackBuffer.reset();
			BackBuffersRelease();
			if (m_cls_Core) {
				m_cls_Core->cls_SrvAlloc.Free(m_str_NullSrv);
				m_cls_Core->cls_RtvAlloc.Free(m_str_NullRtv);
				m_cls_Core->cls_SamplerAlloc.Free(m_str_NullSampler);
				m_cls_Core->DeferredCollect();
			}
			if (m_p_FenceEvent) CloseHandle(m_p_FenceEvent);
		}

		/*
		* 関数名　Init
		* 引　数　info：生成情報　out_error：エラー内容
		* 戻り値　成功なら true
		* 説　明　アダプタ選択 → デバイス・キュー・フェンス → ヒープ・ルートシグネチャ → スワップチェーンの順に作る
		*/
		bool D3D12Device::Init(const DeviceCreateInfo& info, std::string* out_error)
		{
			auto fail = [&](const char* message) { if (out_error) *out_error = message; return false; };
			m_cls_Core = std::make_shared<D3D12Core>();
			m_cls_Core->p_Owner = this;
			m_cls_Core->p_UnbindFn = [](void* owner, const void* resource) { static_cast<D3D12Device*>(owner)->ResourceUnbind(resource); };

			UINT factory_flags = 0;
			if (info.b_Debug) {
				ComPtr<ID3D12Debug> debug;
				if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
					debug->EnableDebugLayer();
					factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
				}
			}
			if (FAILED(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&m_cls_Factory))) &&
				FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&m_cls_Factory)))) return fail("DXGI ファクトリの作成に失敗しました");

			// ハードウェアアダプタを順に試し、だめなら WARP
			ComPtr<IDXGIAdapter1> adapter;
			for (UINT i = 0; m_cls_Factory->EnumAdapters1(i, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i) {
				DXGI_ADAPTER_DESC1 ad = {};
				adapter->GetDesc1(&ad);
				if (ad.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
				if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_cls_Core->cls_Device)))) break;
			}
			if (!m_cls_Core->cls_Device) {
				if (FAILED(m_cls_Factory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()))) ||
					FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_cls_Core->cls_Device))))
					return fail("D3D12 に対応したアダプタが見つかりません");
			}
			{
				DXGI_ADAPTER_DESC1 ad = {};
				adapter->GetDesc1(&ad);
				char name[128] = {};
				WideCharToMultiByte(CP_UTF8, 0, ad.Description, -1, name, sizeof(name), nullptr, nullptr);
				m_AdapterName = name;
			}
			ID3D12Device* dev = m_cls_Core->cls_Device.Get();

			D3D12_COMMAND_QUEUE_DESC qd = {};
			qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			if (FAILED(dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&m_cls_Queue)))) return fail("コマンドキューの作成に失敗しました");
			if (FAILED(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_cls_Core->cls_Fence)))) return fail("フェンスの作成に失敗しました");
			m_p_FenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

			m_i_FrameCount = std::max<uint32_t>(2, std::min<uint32_t>(info.i_FramesInFlight, 3));
			m_Frames.resize(m_i_FrameCount);
			for (auto& f : m_Frames)
				if (FAILED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&f.cls_Allocator)))) return fail("コマンドアロケータの作成に失敗しました");
			if (FAILED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_Frames[0].cls_Allocator.Get(), nullptr, IID_PPV_ARGS(&m_cls_List))))
				return fail("コマンドリストの作成に失敗しました");
			m_b_ListOpen = true;

			m_cls_Core->cls_RtvAlloc.Init(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			m_cls_Core->cls_DsvAlloc.Init(dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
			m_cls_Core->cls_SrvAlloc.Init(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			m_cls_Core->cls_SamplerAlloc.Init(dev, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			m_i_SrvIncrement = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			m_i_SamplerIncrement = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

			D3D12_DESCRIPTOR_HEAP_DESC hd = {};
			hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			hd.NumDescriptors = GPU_SRV_HEAP_SIZE;
			hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			if (FAILED(dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&m_cls_GpuSrvHeap)))) return fail("ディスクリプタヒープの作成に失敗しました");
			hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			hd.NumDescriptors = GPU_SAMPLER_HEAP_SIZE;
			if (FAILED(dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&m_cls_GpuSamplerHeap)))) return fail("サンプラーヒープの作成に失敗しました");

			// 未設定スロット用のヌルディスクリプタ
			m_str_NullSrv = m_cls_Core->cls_SrvAlloc.Allocate();
			D3D12_SHADER_RESOURCE_VIEW_DESC nsv = {};
			nsv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			nsv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			nsv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			nsv.Texture2D.MipLevels = 1;
			dev->CreateShaderResourceView(nullptr, &nsv, m_str_NullSrv);
			m_str_NullRtv = m_cls_Core->cls_RtvAlloc.Allocate();
			D3D12_RENDER_TARGET_VIEW_DESC nrv = {};
			nrv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			nrv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			dev->CreateRenderTargetView(nullptr, &nrv, m_str_NullRtv);
			m_str_NullSampler = m_cls_Core->cls_SamplerAlloc.Allocate();
			D3D12_SAMPLER_DESC nsd = {};
			nsd.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			nsd.AddressU = nsd.AddressV = nsd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			nsd.MaxLOD = D3D12_FLOAT32_MAX;
			nsd.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			dev->CreateSampler(&nsd, m_str_NullSampler);

			// 未設定 CB 用のゼロ埋めバッファ
			const D3D12_HEAP_PROPERTIES upload_heap = HeapPropsGet(D3D12_HEAP_TYPE_UPLOAD);
			const D3D12_RESOURCE_DESC cb_desc = BufferDescGet(DUMMY_CB_SIZE);
			if (FAILED(dev->CreateCommittedResource(&upload_heap, D3D12_HEAP_FLAG_NONE, &cb_desc, D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr, IID_PPV_ARGS(&m_cls_DummyCb)))) return fail("バッファの作成に失敗しました");
			void* mapped = nullptr;
			if (SUCCEEDED(m_cls_DummyCb->Map(0, nullptr, &mapped))) {
				std::memset(mapped, 0, DUMMY_CB_SIZE);
				m_cls_DummyCb->Unmap(0, nullptr);
			}

			if (!RootSignatureCreate()) return fail("ルートシグネチャの作成に失敗しました");

			m_b_VSync = info.b_VSync;
			m_b_Headless = (info.p_WindowHandle == nullptr);
			if (m_b_Headless) {
				TextureDesc bd;
				bd.i_Width = info.i_Width;
				bd.i_Height = info.i_Height;
				bd.e_Format = Format::R8G8B8A8_UNorm;
				bd.i_BindFlags = Bind_RenderTarget | Bind_ShaderResource;
				bd.c_DebugName = "BackBuffer";
				m_cls_BackBuffer = std::static_pointer_cast<D3D12Texture>(TextureCreate(bd, nullptr));
				if (!m_cls_BackBuffer) return fail("オフスクリーンバッファの作成に失敗しました");
			}
			else if (!SwapChainCreate(info)) {
				return fail("スワップチェーンの作成に失敗しました");
			}
			FrameBegin();
			return true;
		}

		/*
		* 関数名　RootSignatureCreate
		* 引　数　なし
		* 戻り値　成功なら true
		* 説　明　全シェーダー共通のルートシグネチャを作る（先頭コメントの配置）
		*/
		bool D3D12Device::RootSignatureCreate()
		{
			D3D12_ROOT_PARAMETER params[ROOT_SMP_TABLE + STAGES] = {};
			D3D12_DESCRIPTOR_RANGE srv_ranges[STAGES] = {};
			D3D12_DESCRIPTOR_RANGE smp_ranges[STAGES] = {};
			for (uint32_t s = 0; s < STAGES; ++s) {
				const D3D12_SHADER_VISIBILITY vis = (s == 0) ? D3D12_SHADER_VISIBILITY_VERTEX : D3D12_SHADER_VISIBILITY_PIXEL;
				for (uint32_t i = 0; i < CB_SLOTS; ++i) {
					auto& p = params[ROOT_CBV_BASE + s * CB_SLOTS + i];
					p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
					p.Descriptor.ShaderRegister = i;
					p.ShaderVisibility = vis;
				}
				srv_ranges[s].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				srv_ranges[s].NumDescriptors = SRV_SLOTS;
				auto& ps = params[ROOT_SRV_TABLE + s];
				ps.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				ps.DescriptorTable.NumDescriptorRanges = 1;
				ps.DescriptorTable.pDescriptorRanges = &srv_ranges[s];
				ps.ShaderVisibility = vis;

				smp_ranges[s].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
				smp_ranges[s].NumDescriptors = SMP_SLOTS;
				auto& pm = params[ROOT_SMP_TABLE + s];
				pm.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				pm.DescriptorTable.NumDescriptorRanges = 1;
				pm.DescriptorTable.pDescriptorRanges = &smp_ranges[s];
				pm.ShaderVisibility = vis;
			}
			D3D12_ROOT_SIGNATURE_DESC rd = {};
			rd.NumParameters = (UINT)(sizeof(params) / sizeof(params[0]));
			rd.pParameters = params;
			rd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			ComPtr<ID3DBlob> blob, error;
			if (FAILED(D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error))) {
				if (error) LogWrite("D3D12: %s", (const char*)error->GetBufferPointer());
				return false;
			}
			return SUCCEEDED(m_cls_Core->cls_Device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
				IID_PPV_ARGS(&m_cls_RootSignature)));
		}

		/*
		* 関数名　SwapChainCreate
		* 引　数　info：生成情報
		* 戻り値　成功なら true
		* 説　明　フリップモデルのスワップチェーンを作り、代理のバックバッファテクスチャを用意する
		*/
		bool D3D12Device::SwapChainCreate(const DeviceCreateInfo& info)
		{
			ComPtr<IDXGIFactory5> factory5;
			if (SUCCEEDED(m_cls_Factory.As(&factory5))) {
				BOOL allow = FALSE;
				if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof(allow))))
					m_b_TearingSupported = (allow == TRUE);
			}

			DXGI_SWAP_CHAIN_DESC1 sd = {};
			sd.Width = info.i_Width;
			sd.Height = info.i_Height;
			sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.SampleDesc.Count = 1;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.BufferCount = m_i_FrameCount;
			sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			sd.Flags = m_b_TearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

			ComPtr<IDXGISwapChain1> sc1;
			HWND hwnd = (HWND)info.p_WindowHandle;
			if (FAILED(m_cls_Factory->CreateSwapChainForHwnd(m_cls_Queue.Get(), hwnd, &sd, nullptr, nullptr, &sc1))) return false;
			if (FAILED(sc1.As(&m_cls_SwapChain))) return false;
			m_cls_Factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
			if (info.b_Fullscreen) m_cls_SwapChain->SetFullscreenState(TRUE, nullptr);

			TextureDesc bd;
			bd.i_Width = info.i_Width;
			bd.i_Height = info.i_Height;
			bd.e_Format = Format::R8G8B8A8_UNorm;
			bd.i_BindFlags = Bind_RenderTarget;
			m_cls_BackBuffer = std::make_shared<D3D12Texture>(bd, m_cls_Core);
			m_cls_BackBuffer->m_b_OwnsViews = false;
			m_cls_BackBuffer->m_b_External = true;
			return BackBuffersAcquire();
		}

		bool D3D12Device::BackBuffersAcquire()
		{
			DXGI_SWAP_CHAIN_DESC1 sd = {};
			m_cls_SwapChain->GetDesc1(&sd);
			m_BackBufferResources.resize(sd.BufferCount);
			m_BackBufferRtvs.resize(sd.BufferCount);
			m_BackBufferStates.assign(sd.BufferCount, D3D12_RESOURCE_STATE_PRESENT);
			for (UINT i = 0; i < sd.BufferCount; ++i) {
				if (FAILED(m_cls_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_BackBufferResources[i])))) return false;
				m_BackBufferRtvs[i] = m_cls_Core->cls_RtvAlloc.Allocate();
				m_cls_Core->cls_Device->CreateRenderTargetView(m_BackBufferResources[i].Get(), nullptr, m_BackBufferRtvs[i]);
			}
			m_cls_BackBuffer->SizeSet(sd.Width, sd.Height);
			m_i_BackBufferIndex = m_cls_SwapChain->GetCurrentBackBufferIndex();
			m_cls_BackBuffer->m_cls_Resource = m_BackBufferResources[m_i_BackBufferIndex];
			m_cls_BackBuffer->m_str_Rtv = m_BackBufferRtvs[m_i_BackBufferIndex];
			m_cls_BackBuffer->m_e_State = D3D12_RESOURCE_STATE_PRESENT;
			return true;
		}

		void D3D12Device::BackBuffersRelease()
		{
			for (auto& h : m_BackBufferRtvs) m_cls_Core->cls_RtvAlloc.Free(h);
			m_BackBufferRtvs.clear();
			m_BackBufferResources.clear();
			if (m_cls_BackBuffer && m_cls_BackBuffer->m_b_External) {
				m_cls_BackBuffer->m_cls_Resource.Reset();
				m_cls_BackBuffer->m_str_Rtv = D3D12_CPU_DESCRIPTOR_HANDLE{ 0 };
			}
		}

		NativeHandles D3D12Device::NativeHandlesGet() const
		{
			NativeHandles h;
			h.p_Device = m_cls_Core->cls_Device.Get();
			h.p_Context = m_cls_List.Get();
			h.p_Queue = m_cls_Queue.Get();
			return h;
		}

		// ---- コマンド・フレーム ------------------------------------------

		void D3D12Device::CommandsBegin()
		{
			if (m_b_ListOpen) return;
			m_cls_List->Reset(m_Frames[m_i_FrameSlot].cls_Allocator.Get(), nullptr);
			m_b_ListOpen = true;
			AllDirtyMark();
		}

		/*
		* 関数名　CommandsSubmit
		* 引　数　なし
		* 戻り値　この提出の完了を表すフェンス値
		* 説　明　記録中のコマンドを実行キューへ送り、フェンスを Signal する
		*/
		uint64_t D3D12Device::CommandsSubmit()
		{
			if (m_b_ListOpen) {
				BarriersFlush();
				m_cls_List->Close();
				ID3D12CommandList* lists[] = { m_cls_List.Get() };
				m_cls_Queue->ExecuteCommandLists(1, lists);
				m_b_ListOpen = false;
			}
			const uint64_t value = m_cls_Core->i_NextFenceValue++;
			m_cls_Queue->Signal(m_cls_Core->cls_Fence.Get(), value);
			++m_i_Submission;
			return value;
		}

		void D3D12Device::FenceWait(uint64_t value)
		{
			if (m_cls_Core->cls_Fence->GetCompletedValue() >= value) return;
			m_cls_Core->cls_Fence->SetEventOnCompletion(value, m_p_FenceEvent);
			WaitForSingleObject(m_p_FenceEvent, INFINITE);
		}

		/*
		* 関数名　FrameBegin
		* 引　数　なし
		* 戻り値　なし
		* 説　明　このスロットを前回使ったフレームの完了を待ち、アロケータとアップロードページを再利用する
		*/
		void D3D12Device::FrameBegin()
		{
			FrameContext& f = m_Frames[m_i_FrameSlot];
			FenceWait(f.i_FenceValue);
			for (auto& page : f.uploadPages) {
				if (page.i_Size == UPLOAD_PAGE_SIZE) {
					page.i_Offset = 0;
					m_FreePages.push_back(std::move(page));
				}
			}
			f.uploadPages.clear();
			m_cls_Core->DeferredCollect();

			if (!m_b_ListOpen) {
				f.cls_Allocator->Reset();
				m_cls_List->Reset(f.cls_Allocator.Get(), nullptr);
				m_b_ListOpen = true;
			}
			// SRV リングはフレーム数で等分して使う
			const uint32_t segment = GPU_SRV_HEAP_SIZE / m_i_FrameCount;
			f.i_SrvRingStart = segment * m_i_FrameSlot;
			m_i_SrvRingHead = f.i_SrvRingStart;
			AllDirtyMark();

			if (m_cls_SwapChain) {
				m_i_BackBufferIndex = m_cls_SwapChain->GetCurrentBackBufferIndex();
				m_cls_BackBuffer->m_cls_Resource = m_BackBufferResources[m_i_BackBufferIndex];
				m_cls_BackBuffer->m_str_Rtv = m_BackBufferRtvs[m_i_BackBufferIndex];
				m_cls_BackBuffer->m_e_State = m_BackBufferStates[m_i_BackBufferIndex];
			}
		}

		void D3D12Device::Present()
		{
			D3D12_LOCK;
			if (m_cls_SwapChain) {
				TextureTransition(m_cls_BackBuffer.get(), D3D12_RESOURCE_STATE_PRESENT);
				m_BackBufferStates[m_i_BackBufferIndex] = D3D12_RESOURCE_STATE_PRESENT;
			}
			m_Frames[m_i_FrameSlot].i_FenceValue = CommandsSubmit();
			if (m_cls_SwapChain) {
				const UINT flags = (!m_b_VSync && m_b_TearingSupported) ? DXGI_PRESENT_ALLOW_TEARING : 0;
				m_cls_SwapChain->Present(m_b_VSync ? 1 : 0, flags);
			}
			++m_i_Frame;
			m_i_FrameSlot = (m_i_FrameSlot + 1) % m_i_FrameCount;
			FrameBegin();
		}

		/*
		* 関数名　IdleWait
		* 引　数　なし
		* 戻り値　なし
		* 説　明　記録済みのコマンドを提出して GPU の完了を待つ（続けて記録できる状態に戻す）
		*/
		void D3D12Device::IdleWait()
		{
			D3D12_LOCK;
			if (m_cls_SwapChain) m_BackBufferStates[m_i_BackBufferIndex] = m_cls_BackBuffer->m_e_State;
			const uint64_t value = CommandsSubmit();
			FenceWait(value);
			m_cls_Core->DeferredCollect();
			// GPU は空なので現在のアロケータを巻き戻して再開する
			m_Frames[m_i_FrameSlot].cls_Allocator->Reset();
			m_cls_List->Reset(m_Frames[m_i_FrameSlot].cls_Allocator.Get(), nullptr);
			m_b_ListOpen = true;
			m_i_SrvRingHead = m_Frames[m_i_FrameSlot].i_SrvRingStart;
			AllDirtyMark();
		}

		void D3D12Device::SwapChainResize(uint32_t width, uint32_t height)
		{
			D3D12_LOCK;
			if (width == 0 || height == 0) return;
			IdleWait();
			for (auto& f : m_Frames) f.i_FenceValue = 0;
			m_str_RenderTargets = RenderTargetState{};
			m_b_RenderTargetsDirty = true;
			if (m_b_Headless) {
				TextureDesc bd = m_cls_BackBuffer->DescGet();
				bd.i_Width = width;
				bd.i_Height = height;
				m_cls_BackBuffer = std::static_pointer_cast<D3D12Texture>(TextureCreate(bd, nullptr));
				return;
			}
			// コマンドリストがバックバッファを参照していないよう一度提出して待つ
			const uint64_t value = CommandsSubmit();
			FenceWait(value);
			BackBuffersRelease();
			DXGI_SWAP_CHAIN_DESC1 sd = {};
			m_cls_SwapChain->GetDesc1(&sd);
			if (FAILED(m_cls_SwapChain->ResizeBuffers(sd.BufferCount, width, height, DXGI_FORMAT_UNKNOWN, sd.Flags)))
				LogWrite("D3D12: ResizeBuffers 失敗");
			BackBuffersAcquire();
			m_Frames[m_i_FrameSlot].cls_Allocator->Reset();
			m_cls_List->Reset(m_Frames[m_i_FrameSlot].cls_Allocator.Get(), nullptr);
			m_b_ListOpen = true;
			AllDirtyMark();
		}

		void D3D12Device::AllDirtyMark()
		{
			m_b_RenderTargetsDirty = m_b_ViewportDirty = m_b_ScissorDirty = m_b_TopologyDirty = true;
			m_b_RootDirty = m_b_VertexBuffersDirty = m_b_IndexBufferDirty = true;
			m_b_BlendFactorDirty = m_b_StencilRefDirty = true;
			for (uint32_t s = 0; s < STAGES; ++s) m_b_SrvDirty[s] = m_b_SamplerDirty[s] = true;
			m_p_CurrentPipeline = nullptr;
		}

		// ---- アップロード ------------------------------------------------

		/*
		* 関数名　UploadAllocate
		* 引　数　size：サイズ　alignment：アライメント
		* 戻り値　アップロードヒープ上の領域
		* 説　明　フレーム用のアップロードページから切り出す。ページが足りなければ追加する
		*/
		D3D12Device::UploadAlloc D3D12Device::UploadAllocate(uint64_t size, uint64_t alignment)
		{
			FrameContext& f = m_Frames[m_i_FrameSlot];
			auto try_page = [&](UploadPage& page, UploadAlloc& out) {
				const uint64_t offset = (page.i_Offset + alignment - 1) & ~(alignment - 1);
				if (offset + size > page.i_Size) return false;
				page.i_Offset = offset + size;
				out.p_Resource = page.cls_Resource.Get();
				out.i_Offset = offset;
				out.p_Cpu = page.p_Cpu + offset;
				out.i_Gpu = page.i_Gpu + offset;
				return true;
			};
			UploadAlloc out;
			if (!f.uploadPages.empty() && try_page(f.uploadPages.back(), out)) return out;

			UploadPage page;
			if (size + alignment <= UPLOAD_PAGE_SIZE && !m_FreePages.empty()) {
				page = std::move(m_FreePages.back());
				m_FreePages.pop_back();
			}
			else {
				page.i_Size = std::max<uint64_t>(UPLOAD_PAGE_SIZE, (size + alignment + 0xFFFF) & ~0xFFFFull);
				const D3D12_HEAP_PROPERTIES hp = HeapPropsGet(D3D12_HEAP_TYPE_UPLOAD);
				const D3D12_RESOURCE_DESC rd = BufferDescGet(page.i_Size);
				if (FAILED(m_cls_Core->cls_Device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr, IID_PPV_ARGS(&page.cls_Resource)))) {
					LogWrite("D3D12: アップロードバッファの確保に失敗 (%llu bytes)", (unsigned long long)page.i_Size);
					return out;
				}
				void* cpu = nullptr;
				page.cls_Resource->Map(0, nullptr, &cpu);
				page.p_Cpu = (uint8_t*)cpu;
				page.i_Gpu = page.cls_Resource->GetGPUVirtualAddress();
			}
			f.uploadPages.push_back(std::move(page));
			try_page(f.uploadPages.back(), out);
			// 大きな専用ページは使い終わったら解放（FrameBegin でサイズを見て捨てる）
			return out;
		}

		/*
		* 関数名　StreamedBufferResolve
		* 引　数　buffer：定数バッファ / Dynamic バッファ
		* 戻り値　このフレームで有効な GPU アドレス
		* 説　明　内容が変わったか、前のフレームに書き出したものなら書き出し直す
		*/
		D3D12_GPU_VIRTUAL_ADDRESS D3D12Device::StreamedBufferResolve(D3D12Buffer* b)
		{
			if (!b->m_b_Dirty && b->m_i_UploadFrame == m_i_Frame) return b->m_i_GpuAddress;
			const uint64_t align = (b->DescGet().i_BindFlags & Bind_ConstantBuffer) ? D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT : 16;
			UploadAlloc a = UploadAllocate(b->m_Shadow.size(), align);
			if (!a.p_Cpu) return m_cls_DummyCb->GetGPUVirtualAddress();
			std::memcpy(a.p_Cpu, b->m_Shadow.data(), b->m_Shadow.size());
			b->m_i_GpuAddress = a.i_Gpu;
			b->m_i_UploadFrame = m_i_Frame;
			b->m_b_Dirty = false;
			return a.i_Gpu;
		}

		// ---- 状態遷移 ----------------------------------------------------

		void D3D12Device::BufferTransition(D3D12Buffer* b, D3D12_RESOURCE_STATES state)
		{
			if (!b->m_cls_Resource) return;
			if (b->m_i_StateSubmission != m_i_Submission) {
				b->m_e_State = D3D12_RESOURCE_STATE_COMMON;   // 前回の提出で COMMON に戻っている
				b->m_i_StateSubmission = m_i_Submission;
			}
			if (b->m_e_State == state) return;
			D3D12_RESOURCE_BARRIER br = {};
			br.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			br.Transition.pResource = b->m_cls_Resource.Get();
			br.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			br.Transition.StateBefore = b->m_e_State;
			br.Transition.StateAfter = state;
			m_Barriers.push_back(br);
			b->m_e_State = state;
		}

		void D3D12Device::TextureTransition(D3D12Texture* t, D3D12_RESOURCE_STATES state)
		{
			if (!t || !t->m_cls_Resource || t->m_e_State == state) return;
			// 読み取り状態同士は合成して余計なバリアを減らす
			const D3D12_RESOURCE_STATES read_states = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			if ((state & ~read_states) == 0 && (t->m_e_State & state) == state && (t->m_e_State & ~read_states) == 0) return;
			D3D12_RESOURCE_BARRIER br = {};
			br.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			br.Transition.pResource = t->m_cls_Resource.Get();
			br.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			br.Transition.StateBefore = t->m_e_State;
			br.Transition.StateAfter = state;
			m_Barriers.push_back(br);
			t->m_e_State = state;
		}

		void D3D12Device::BarriersFlush()
		{
			if (m_Barriers.empty()) return;
			m_cls_List->ResourceBarrier((UINT)m_Barriers.size(), m_Barriers.data());
			m_Barriers.clear();
		}

		// ---- リソース生成 ------------------------------------------------

		/*
		* 関数名　BufferCreate
		* 引　数　desc：記述　initial_data：初期データ
		* 戻り値　バッファ
		* 説　明　定数バッファと Dynamic バッファは CPU 側のみ（描画時にアップロード）、
		*         それ以外は Default ヒープに作って初期データをコピーする
		*/
		BufferRef D3D12Device::BufferCreate(const BufferDesc& desc, const void* initial_data)
		{
			D3D12_LOCK;
			auto buf = std::make_shared<D3D12Buffer>(desc, m_cls_Core);
			const bool streamed = (desc.i_BindFlags & Bind_ConstantBuffer) || desc.e_Usage == Usage::Dynamic;
			if (streamed) {
				const uint32_t size = (desc.i_BindFlags & Bind_ConstantBuffer) ? ((desc.i_Size + 255) & ~255u) : desc.i_Size;
				buf->m_Shadow.assign(std::max<uint32_t>(size, 16), 0);
				if (initial_data) std::memcpy(buf->m_Shadow.data(), initial_data, desc.i_Size);
				return buf;
			}
			const D3D12_HEAP_PROPERTIES hp = HeapPropsGet(D3D12_HEAP_TYPE_DEFAULT);
			const D3D12_RESOURCE_DESC rd = BufferDescGet(std::max<uint32_t>(desc.i_Size, 16));
			if (FAILED(m_cls_Core->cls_Device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON,
				nullptr, IID_PPV_ARGS(&buf->m_cls_Resource)))) {
				LogWrite("D3D12: バッファ作成失敗 (size=%u)", desc.i_Size);
				return nullptr;
			}
			if (initial_data) BufferUpdate(buf.get(), initial_data, desc.i_Size, 0);
			return buf;
		}

		bool D3D12Device::TextureViewsCreate(D3D12Texture* tex)
		{
			ID3D12Device* dev = m_cls_Core->cls_Device.Get();
			const TextureDesc& d = tex->DescGet();
			const bool depth = FormatInfoGet(d.e_Format).b_Depth;
			if (d.i_BindFlags & Bind_ShaderResource) {
				D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
				sv.Format = depth ? DxgiDepthSrvFormatGet(d.e_Format) : DxgiFormatGet(d.e_Format);
				sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				if (d.i_ArraySize > 1) {
					sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					sv.Texture2DArray.MipLevels = d.i_MipLevels;
					sv.Texture2DArray.ArraySize = d.i_ArraySize;
				}
				else {
					sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					sv.Texture2D.MipLevels = d.i_MipLevels;
				}
				tex->m_str_Srv = m_cls_Core->cls_SrvAlloc.Allocate();
				dev->CreateShaderResourceView(tex->m_cls_Resource.Get(), &sv, tex->m_str_Srv);
			}
			if (d.i_BindFlags & Bind_RenderTarget) {
				D3D12_RENDER_TARGET_VIEW_DESC rv = {};
				rv.Format = DxgiFormatGet(d.e_Format);
				rv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				tex->m_str_Rtv = m_cls_Core->cls_RtvAlloc.Allocate();
				dev->CreateRenderTargetView(tex->m_cls_Resource.Get(), &rv, tex->m_str_Rtv);
			}
			if (d.i_BindFlags & Bind_DepthStencil) {
				D3D12_DEPTH_STENCIL_VIEW_DESC dv = {};
				dv.Format = DxgiFormatGet(d.e_Format);
				dv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				tex->m_str_Dsv = m_cls_Core->cls_DsvAlloc.Allocate();
				dev->CreateDepthStencilView(tex->m_cls_Resource.Get(), &dv, tex->m_str_Dsv);
			}
			return true;
		}

		/*
		* 関数名　TextureCreate
		* 引　数　desc：記述　initial_data：初期データ（D3D11 のサブリソース順）
		* 戻り値　テクスチャ
		* 説　明　Default ヒープに作り、初期データはアップロードページ経由でコピーする
		*/
		TextureRef D3D12Device::TextureCreate(const TextureDesc& desc, const SubresourceData* initial_data)
		{
			D3D12_LOCK;
			auto tex = std::make_shared<D3D12Texture>(desc, m_cls_Core);
			const bool depth = FormatInfoGet(desc.e_Format).b_Depth;
			D3D12_RESOURCE_DESC rd = {};
			rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			rd.Width = desc.i_Width;
			rd.Height = desc.i_Height;
			rd.DepthOrArraySize = (UINT16)desc.i_ArraySize;
			rd.MipLevels = (UINT16)desc.i_MipLevels;
			rd.Format = (depth && (desc.i_BindFlags & Bind_ShaderResource)) ? DxgiTypelessFormatGet(desc.e_Format) : DxgiFormatGet(desc.e_Format);
			rd.SampleDesc.Count = 1;
			rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			if (desc.i_BindFlags & Bind_RenderTarget) rd.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			if (desc.i_BindFlags & Bind_DepthStencil) {
				rd.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
				if (!(desc.i_BindFlags & Bind_ShaderResource)) rd.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
			}

			D3D12_CLEAR_VALUE clear = {};
			const D3D12_CLEAR_VALUE* p_clear = nullptr;
			D3D12_RESOURCE_STATES initial = D3D12_RESOURCE_STATE_COMMON;
			if (desc.i_BindFlags & Bind_DepthStencil) {
				clear.Format = DxgiFormatGet(desc.e_Format);
				clear.DepthStencil.Depth = desc.f_ClearDepth;
				p_clear = &clear;
				initial = D3D12_RESOURCE_STATE_DEPTH_WRITE;
			}
			else if (desc.i_BindFlags & Bind_RenderTarget) {
				clear.Format = DxgiFormatGet(desc.e_Format);
				std::memcpy(clear.Color, desc.f_ClearColor, sizeof(clear.Color));
				p_clear = &clear;
				initial = D3D12_RESOURCE_STATE_RENDER_TARGET;
			}
			const D3D12_HEAP_PROPERTIES hp = HeapPropsGet(D3D12_HEAP_TYPE_DEFAULT);
			if (FAILED(m_cls_Core->cls_Device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, initial, p_clear,
				IID_PPV_ARGS(&tex->m_cls_Resource)))) {
				LogWrite("D3D12: テクスチャ作成失敗 (%ux%u %s)", desc.i_Width, desc.i_Height, FormatNameGet(desc.e_Format));
				return nullptr;
			}
			tex->m_e_State = initial;
			if (desc.c_DebugName) {
				wchar_t wname[128] = {};
				MultiByteToWideChar(CP_UTF8, 0, desc.c_DebugName, -1, wname, 127);
				tex->m_cls_Resource->SetName(wname);
			}
			TextureViewsCreate(tex.get());

			if (initial_data) {
				const uint32_t count = desc.i_MipLevels * desc.i_ArraySize;
				for (uint32_t i = 0; i < count; ++i) {
					if (!initial_data[i].p_Data) continue;
					const uint32_t mip = i % desc.i_MipLevels;
					const uint32_t slice = i / desc.i_MipLevels;
					// D3D11 のサブリソース番号と D3D12 の番号は同じ並び (mip + slice * mips)
					D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp = {};
					UINT rows = 0;
					UINT64 row_size = 0, total = 0;
					m_cls_Core->cls_Device->GetCopyableFootprints(&rd, i, 1, 0, &fp, &rows, &row_size, &total);
					UploadAlloc a = UploadAllocate(total, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
					if (!a.p_Cpu) break;
					const uint32_t src_pitch = initial_data[i].i_RowPitch ? initial_data[i].i_RowPitch : FormatRowPitchGet(desc.e_Format, std::max(1u, desc.i_Width >> mip));
					for (UINT y = 0; y < rows; ++y)
						std::memcpy(a.p_Cpu + (size_t)y * fp.Footprint.RowPitch, (const uint8_t*)initial_data[i].p_Data + (size_t)y * src_pitch,
							(size_t)std::min<UINT64>(row_size, src_pitch));
					fp.Offset = a.i_Offset;
					TextureTransition(tex.get(), D3D12_RESOURCE_STATE_COPY_DEST);
					BarriersFlush();
					D3D12_TEXTURE_COPY_LOCATION dst = {};
					dst.pResource = tex->m_cls_Resource.Get();
					dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
					dst.SubresourceIndex = mip + slice * desc.i_MipLevels;
					D3D12_TEXTURE_COPY_LOCATION src = {};
					src.pResource = a.p_Resource;
					src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
					src.PlacedFootprint = fp;
					m_cls_List->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
				}
				TextureTransition(tex.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			}
			return tex;
		}

		SamplerRef D3D12Device::SamplerCreate(const SamplerDesc& desc)
		{
			D3D12_LOCK;
			auto s = std::make_shared<D3D12Sampler>(desc, m_cls_Core);
			D3D12_SAMPLER_DESC sd = {};
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
			s->m_str_Handle = m_cls_Core->cls_SamplerAlloc.Allocate();
			m_cls_Core->cls_Device->CreateSampler(&sd, s->m_str_Handle);
			return s;
		}

		ShaderRef D3D12Device::ShaderCreate(ShaderStage stage, const void* bytecode, size_t size, const char* name)
		{
			// DXBC の先頭は "DXBC"。SPIR-V などを誤って渡した場合はここで弾く
			if (!bytecode || size < 4 || std::memcmp(bytecode, "DXBC", 4) != 0) {
				LogWrite("D3D12: DXBC ではないシェーダーです %s", name ? name : "");
				return nullptr;
			}
			return std::make_shared<D3D12Shader>(stage, bytecode, size, name);
		}

		InputLayoutRef D3D12Device::InputLayoutCreate(const InputElement* elements, uint32_t count, const Shader* vs)
		{
			if (!vs) return nullptr;
			return std::make_shared<D3D12InputLayout>(elements, count);
		}

		// ---- 更新 ------------------------------------------------------

		void D3D12Device::BufferUpdate(Buffer* buffer, const void* data, uint32_t size, uint32_t offset)
		{
			D3D12_LOCK;
			D3D12Buffer* b = BufCast(buffer);
			if (!b || !data) return;
			if (b->IsStreamed()) {
				const uint32_t n = (offset < b->m_Shadow.size()) ? std::min<uint32_t>(size, (uint32_t)b->m_Shadow.size() - offset) : 0;
				std::memcpy(b->m_Shadow.data() + offset, data, n);
				b->m_b_Dirty = true;
				return;
			}
			if (offset >= b->GetSize()) return;
			size = std::min(size, b->GetSize() - offset);
			UploadAlloc a = UploadAllocate(size, 16);
			if (!a.p_Cpu) return;
			std::memcpy(a.p_Cpu, data, size);
			BufferTransition(b, D3D12_RESOURCE_STATE_COPY_DEST);
			BarriersFlush();
			m_cls_List->CopyBufferRegion(b->m_cls_Resource.Get(), offset, a.p_Resource, a.i_Offset, size);
			m_b_VertexBuffersDirty = m_b_IndexBufferDirty = true;
		}

		void* D3D12Device::BufferMap(Buffer* buffer)
		{
			D3D12Buffer* b = BufCast(buffer);
			if (!b || !b->IsStreamed()) return nullptr;
			return b->m_Shadow.data();
		}

		void D3D12Device::BufferUnmap(Buffer* buffer)
		{
			D3D12Buffer* b = BufCast(buffer);
			if (!b) return;
			b->m_b_Dirty = true;
			m_b_VertexBuffersDirty = m_b_IndexBufferDirty = true;
		}

		void D3D12Device::TextureUpdate(Texture* texture, uint32_t mip, const void* data, uint32_t row_pitch)
		{
			D3D12_LOCK;
			D3D12Texture* t = TexCast(texture);
			if (!t || !data || !t->m_cls_Resource) return;
			const D3D12_RESOURCE_DESC rd = t->m_cls_Resource->GetDesc();
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp = {};
			UINT rows = 0;
			UINT64 row_size = 0, total = 0;
			m_cls_Core->cls_Device->GetCopyableFootprints(&rd, mip, 1, 0, &fp, &rows, &row_size, &total);
			UploadAlloc a = UploadAllocate(total, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
			if (!a.p_Cpu) return;
			if (row_pitch == 0) row_pitch = (uint32_t)row_size;
			for (UINT y = 0; y < rows; ++y)
				std::memcpy(a.p_Cpu + (size_t)y * fp.Footprint.RowPitch, (const uint8_t*)data + (size_t)y * row_pitch, (size_t)std::min<UINT64>(row_size, row_pitch));
			fp.Offset = a.i_Offset;
			const D3D12_RESOURCE_STATES prev = t->m_e_State;
			TextureTransition(t, D3D12_RESOURCE_STATE_COPY_DEST);
			BarriersFlush();
			D3D12_TEXTURE_COPY_LOCATION dst = {};
			dst.pResource = t->m_cls_Resource.Get();
			dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dst.SubresourceIndex = mip;
			D3D12_TEXTURE_COPY_LOCATION src = {};
			src.pResource = a.p_Resource;
			src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src.PlacedFootprint = fp;
			m_cls_List->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
			TextureTransition(t, prev == D3D12_RESOURCE_STATE_COPY_DEST ? (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) : prev);
		}

		void D3D12Device::TextureCopy(Texture* dst, Texture* src)
		{
			D3D12_LOCK;
			D3D12Texture* d = TexCast(dst);
			D3D12Texture* s = TexCast(src);
			if (!d || !s || !d->m_cls_Resource || !s->m_cls_Resource) return;
			TextureTransition(d, D3D12_RESOURCE_STATE_COPY_DEST);
			TextureTransition(s, D3D12_RESOURCE_STATE_COPY_SOURCE);
			BarriersFlush();
			m_cls_List->CopyResource(d->m_cls_Resource.Get(), s->m_cls_Resource.Get());
			m_b_RenderTargetsDirty = true;
		}

		/*
		* 関数名　TextureRead
		* 引　数　texture：読み戻すテクスチャ　out_pixels / out_row_pitch：出力
		* 戻り値　成功なら true
		* 説　明　リードバックバッファへコピーし、GPU の完了を待って読む（ミップ 0 のみ）
		*/
		bool D3D12Device::TextureRead(Texture* texture, std::vector<uint8_t>& out_pixels, uint32_t& out_row_pitch)
		{
			D3D12_LOCK;
			D3D12Texture* t = TexCast(texture);
			if (!t || !t->m_cls_Resource || FormatInfoGet(t->GetFormat()).b_Compressed || FormatInfoGet(t->GetFormat()).b_Depth) return false;
			const D3D12_RESOURCE_DESC rd = t->m_cls_Resource->GetDesc();
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp = {};
			UINT rows = 0;
			UINT64 row_size = 0, total = 0;
			m_cls_Core->cls_Device->GetCopyableFootprints(&rd, 0, 1, 0, &fp, &rows, &row_size, &total);

			ComPtr<ID3D12Resource> readback;
			const D3D12_HEAP_PROPERTIES hp = HeapPropsGet(D3D12_HEAP_TYPE_READBACK);
			const D3D12_RESOURCE_DESC bd = BufferDescGet(total);
			if (FAILED(m_cls_Core->cls_Device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr, IID_PPV_ARGS(&readback)))) return false;

			const D3D12_RESOURCE_STATES prev = t->m_e_State;
			TextureTransition(t, D3D12_RESOURCE_STATE_COPY_SOURCE);
			BarriersFlush();
			D3D12_TEXTURE_COPY_LOCATION dst = {};
			dst.pResource = readback.Get();
			dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			dst.PlacedFootprint = fp;
			D3D12_TEXTURE_COPY_LOCATION src = {};
			src.pResource = t->m_cls_Resource.Get();
			src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			src.SubresourceIndex = 0;
			m_cls_List->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
			TextureTransition(t, prev);
			IdleWait();

			out_row_pitch = FormatRowPitchGet(t->GetFormat(), t->GetWidth());
			out_pixels.resize((size_t)out_row_pitch * rows);
			void* mapped = nullptr;
			const D3D12_RANGE range = { 0, (SIZE_T)total };
			if (FAILED(readback->Map(0, &range, &mapped))) return false;
			for (UINT y = 0; y < rows; ++y)
				std::memcpy(out_pixels.data() + (size_t)y * out_row_pitch, (const uint8_t*)mapped + fp.Offset + (size_t)y * fp.Footprint.RowPitch, out_row_pitch);
			const D3D12_RANGE none = { 0, 0 };
			readback->Unmap(0, &none);
			return true;
		}

		// ---- 出力 ------------------------------------------------------

		/*
		* 関数名　BoundTextureUnbind
		* 引　数　texture：外すテクスチャ（nullptr なら何もしない）
		* 戻り値　なし
		* 説　明　D3D11 と同様、描画先に設定したテクスチャはシェーダー入力から外す
		*/
		void D3D12Device::BoundTextureUnbind(Texture* texture)
		{
			if (!texture) return;
			for (uint32_t s = 0; s < STAGES; ++s)
				for (uint32_t i = 0; i < SRV_SLOTS; ++i)
					if (m_Textures[s][i] == texture) { m_Textures[s][i] = nullptr; m_b_SrvDirty[s] = true; }
		}

		/*
		* 関数名　ResourceUnbind
		* 引　数　resource：破棄されるリソース
		* 戻り値　なし
		* 説　明　D3D11 はバインド中のリソースを参照で保持するが、ここでは生ポインタなので外しておく
		*/
		void D3D12Device::ResourceUnbind(const void* r)
		{
			for (uint32_t s = 0; s < STAGES; ++s) {
				for (uint32_t i = 0; i < SRV_SLOTS; ++i)
					if ((const void*)m_Textures[s][i] == r) { m_Textures[s][i] = nullptr; m_b_SrvDirty[s] = true; }
				for (uint32_t i = 0; i < CB_SLOTS; ++i)
					if ((const void*)static_cast<Buffer*>(m_ConstantBuffers[s][i]) == r) m_ConstantBuffers[s][i] = nullptr;
				for (uint32_t i = 0; i < SMP_SLOTS; ++i)
					if ((const void*)static_cast<Sampler*>(m_Samplers[s][i]) == r) { m_Samplers[s][i] = nullptr; m_b_SamplerDirty[s] = true; }
			}
			for (uint32_t i = 0; i < m_str_RenderTargets.i_Count; ++i)
				if ((const void*)m_str_RenderTargets.p_Colors[i] == r) { m_str_RenderTargets.p_Colors[i] = nullptr; m_b_RenderTargetsDirty = true; }
			if ((const void*)m_str_RenderTargets.p_Depth == r) { m_str_RenderTargets.p_Depth = nullptr; m_b_RenderTargetsDirty = true; }
			for (auto& vb : m_VertexBuffers)
				if (vb.p_Buffer && (const void*)static_cast<Buffer*>(vb.p_Buffer) == r) { vb = VertexBinding{}; m_b_VertexBuffersDirty = true; }
			if (m_p_IndexBuffer && (const void*)static_cast<Buffer*>(m_p_IndexBuffer) == r) m_p_IndexBuffer = nullptr;
		}

		void D3D12Device::RenderTargetsSet(uint32_t count, Texture* const* colors, Texture* depth)
		{
			m_str_RenderTargets = RenderTargetState{};
			count = std::min(count, MAX_RENDER_TARGETS);
			for (uint32_t i = 0; i < count; ++i) {
				m_str_RenderTargets.p_Colors[i] = colors ? colors[i] : nullptr;
				BoundTextureUnbind(m_str_RenderTargets.p_Colors[i]);
			}
			// 末尾の nullptr は数えない
			while (count > 0 && m_str_RenderTargets.p_Colors[count - 1] == nullptr) --count;
			m_str_RenderTargets.i_Count = count;
			m_str_RenderTargets.p_Depth = depth;
			BoundTextureUnbind(depth);
			m_b_RenderTargetsDirty = true;
			m_b_ScissorDirty = true;
		}

		void D3D12Device::RenderTargetClear(Texture* target, const float color[4])
		{
			D3D12_LOCK;
			D3D12Texture* t = TexCast(target);
			if (!t || t->m_str_Rtv.ptr == 0) return;
			TextureTransition(t, D3D12_RESOURCE_STATE_RENDER_TARGET);
			BarriersFlush();
			m_cls_List->ClearRenderTargetView(t->m_str_Rtv, color, 0, nullptr);
		}

		void D3D12Device::DepthStencilClear(Texture* target, bool clear_depth, bool clear_stencil, float depth, uint8_t stencil)
		{
			D3D12_LOCK;
			D3D12Texture* t = TexCast(target);
			if (!t || t->m_str_Dsv.ptr == 0) return;
			D3D12_CLEAR_FLAGS flags = (D3D12_CLEAR_FLAGS)0;
			if (clear_depth) flags = (D3D12_CLEAR_FLAGS)(flags | D3D12_CLEAR_FLAG_DEPTH);
			if (clear_stencil && FormatInfoGet(t->GetFormat()).b_Stencil) flags = (D3D12_CLEAR_FLAGS)(flags | D3D12_CLEAR_FLAG_STENCIL);
			if (flags == 0) return;
			TextureTransition(t, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			BarriersFlush();
			m_cls_List->ClearDepthStencilView(t->m_str_Dsv, flags, depth, stencil, 0, nullptr);
		}

		// ---- ステート --------------------------------------------------

		void D3D12Device::BlendStateSet(BlendState* state, const float blend_factor[4], uint32_t sample_mask)
		{
			m_cls_Blend = state;
			m_i_SampleMask = sample_mask;
			const float zero[4] = { 0, 0, 0, 0 };
			std::memcpy(m_f_BlendFactor, blend_factor ? blend_factor : zero, sizeof(m_f_BlendFactor));
			m_b_BlendFactorDirty = true;
		}

		void D3D12Device::DepthStencilStateSet(DepthStencilState* state, uint32_t stencil_ref)
		{
			m_cls_Depth = state;
			m_i_StencilRef = stencil_ref;
			m_b_StencilRefDirty = true;
		}

		void D3D12Device::ShaderSet(ShaderStage stage, Shader* shader)
		{
			if ((uint32_t)stage >= STAGES) return;
			m_cls_Shaders[(uint32_t)stage] = static_cast<D3D12Shader*>(shader);
		}

		void D3D12Device::VertexBufferSet(uint32_t slot, Buffer* buffer, uint32_t stride, uint32_t offset)
		{
			if (slot >= 16) return;
			m_VertexBuffers[slot] = { BufCast(buffer), stride, offset };
			m_b_VertexBuffersDirty = true;
		}

		void D3D12Device::IndexBufferSet(Buffer* buffer, IndexFormat format, uint32_t offset)
		{
			m_p_IndexBuffer = BufCast(buffer);
			m_e_IndexFormat = format;
			m_i_IndexOffset = offset;
			m_b_IndexBufferDirty = true;
		}

		void D3D12Device::ConstantBufferSet(ShaderStage stage, uint32_t slot, Buffer* buffer)
		{
			if ((uint32_t)stage >= STAGES || slot >= CB_SLOTS) return;
			m_ConstantBuffers[(uint32_t)stage][slot] = BufCast(buffer);
		}

		void D3D12Device::TextureSet(ShaderStage stage, uint32_t slot, Texture* texture)
		{
			if ((uint32_t)stage >= STAGES || slot >= SRV_SLOTS) return;
			// 描画先に設定中のテクスチャは入力にできない（D3D11 は NULL を設定する）
			if (texture) {
				if (texture == m_str_RenderTargets.p_Depth) texture = nullptr;
				for (uint32_t i = 0; texture && i < m_str_RenderTargets.i_Count; ++i)
					if (m_str_RenderTargets.p_Colors[i] == texture) texture = nullptr;
			}
			if (texture && TexCast(texture)->m_str_Srv.ptr == 0) texture = nullptr;
			if (m_Textures[(uint32_t)stage][slot] == texture) return;
			m_Textures[(uint32_t)stage][slot] = texture;
			m_b_SrvDirty[(uint32_t)stage] = true;
		}

		void D3D12Device::SamplerSet(ShaderStage stage, uint32_t slot, Sampler* sampler)
		{
			if ((uint32_t)stage >= STAGES || slot >= SMP_SLOTS) return;
			D3D12Sampler* s = static_cast<D3D12Sampler*>(sampler);
			if (m_Samplers[(uint32_t)stage][slot] == s) return;
			m_Samplers[(uint32_t)stage][slot] = s;
			m_b_SamplerDirty[(uint32_t)stage] = true;
		}

		void D3D12Device::StateClear()
		{
			m_str_RenderTargets = RenderTargetState{};
			m_cls_Blend = nullptr;
			m_cls_Depth = nullptr;
			m_cls_Raster = nullptr;
			m_i_SampleMask = 0xFFFFFFFFu;
			m_i_StencilRef = 0;
			std::memset(m_f_BlendFactor, 0, sizeof(m_f_BlendFactor));
			for (auto& s : m_cls_Shaders) s = nullptr;
			m_cls_Layout = nullptr;
			m_e_Topology = Topology::TriangleList;
			for (auto& v : m_VertexBuffers) v = VertexBinding{};
			m_p_IndexBuffer = nullptr;
			std::memset(m_ConstantBuffers, 0, sizeof(m_ConstantBuffers));
			std::memset(m_Textures, 0, sizeof(m_Textures));
			std::memset(m_Samplers, 0, sizeof(m_Samplers));
			m_str_Viewport = Viewport{};
			m_str_Scissor = Rect{};
			AllDirtyMark();
		}

		// ---- 描画 ------------------------------------------------------

		/*
		* 関数名　PipelineGet
		* 引　数　なし
		* 戻り値　現在のステートに対応する PSO（作れなければ nullptr）
		* 説　明　ステートからキーを作り、キャッシュに無ければ作成する
		*/
		ID3D12PipelineState* D3D12Device::PipelineGet()
		{
			D3D12Shader* vs = m_cls_Shaders[0];
			D3D12Shader* ps = m_cls_Shaders[1];
			if (!vs) return nullptr;

			PipelineKey key;
			std::memset((void*)&key, 0, sizeof(key));   // パディングも 0 にしてバイト比較できるようにする
			key.i_Vs = vs->m_i_Id;
			key.i_Ps = ps ? ps->m_i_Id : 0;
			key.i_Layout = m_cls_Layout ? m_cls_Layout->m_i_Id : 0;
			key.blend = m_cls_Blend ? static_cast<D3D12BlendState*>(m_cls_Blend)->m_str_Native : DefaultBlendGet().m_str_Native;
			key.depth = m_cls_Depth ? static_cast<D3D12DepthStencilState*>(m_cls_Depth)->m_str_Native : DefaultDepthGet().m_str_Native;
			key.raster = m_cls_Raster ? static_cast<D3D12RasterizerState*>(m_cls_Raster)->m_str_Native : DefaultRasterGet().m_str_Native;
			key.i_SampleMask = m_i_SampleMask;
			key.i_TopologyType = (uint32_t)TopologyTypeGet(m_e_Topology);
			key.i_RtCount = m_str_RenderTargets.i_Count;
			for (uint32_t i = 0; i < m_str_RenderTargets.i_Count; ++i) {
				Texture* t = m_str_RenderTargets.p_Colors[i];
				key.e_Rtv[i] = t ? DxgiFormatGet(t->GetFormat()) : DXGI_FORMAT_R8G8B8A8_UNORM;
			}
			key.e_Dsv = m_str_RenderTargets.p_Depth ? DxgiFormatGet(m_str_RenderTargets.p_Depth->GetFormat()) : DXGI_FORMAT_UNKNOWN;
			if (key.e_Dsv == DXGI_FORMAT_UNKNOWN) {
				// 深度バッファ無しで深度テストを有効にはできない（D3D11 は無視される）
				key.depth.DepthEnable = FALSE;
				key.depth.StencilEnable = FALSE;
			}

			const std::string bytes((const char*)&key, sizeof(key));
			auto it = m_Pipelines.find(bytes);
			if (it != m_Pipelines.end()) return it->second.Get();

			D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {};
			pd.pRootSignature = m_cls_RootSignature.Get();
			pd.VS = { vs->BytecodeGet().data(), vs->BytecodeGet().size() };
			if (ps) pd.PS = { ps->BytecodeGet().data(), ps->BytecodeGet().size() };
			pd.BlendState = key.blend;
			pd.SampleMask = key.i_SampleMask;
			pd.RasterizerState = key.raster;
			pd.DepthStencilState = key.depth;
			if (m_cls_Layout) pd.InputLayout = { m_cls_Layout->m_Descs.data(), (UINT)m_cls_Layout->m_Descs.size() };
			pd.PrimitiveTopologyType = (D3D12_PRIMITIVE_TOPOLOGY_TYPE)key.i_TopologyType;
			pd.NumRenderTargets = key.i_RtCount;
			for (uint32_t i = 0; i < key.i_RtCount; ++i) pd.RTVFormats[i] = key.e_Rtv[i];
			pd.DSVFormat = key.e_Dsv;
			pd.SampleDesc.Count = 1;

			ComPtr<ID3D12PipelineState> pso;
			if (FAILED(m_cls_Core->cls_Device->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&pso)))) {
				LogWrite("D3D12: PSO 作成失敗 (VS=%s PS=%s)", vs->NameGet().c_str(), ps ? ps->NameGet().c_str() : "-");
				m_Pipelines[bytes] = nullptr;   // 毎回作り直さない
				return nullptr;
			}
			ID3D12PipelineState* raw = pso.Get();
			m_Pipelines[bytes] = pso;
			return raw;
		}

		/*
		* 関数名　SamplerTableGet
		* 引　数　stage：ステージ
		* 戻り値　シェーダー可視サンプラーヒープ上のテーブル
		* 説　明　サンプラーの組み合わせごとにヒープへ配置してキャッシュする（ヒープが小さいため）
		*/
		D3D12_GPU_DESCRIPTOR_HANDLE D3D12Device::SamplerTableGet(uint32_t stage)
		{
			std::string key(sizeof(uint64_t) * SMP_SLOTS, '\0');
			for (uint32_t i = 0; i < SMP_SLOTS; ++i) {
				const uint64_t id = m_Samplers[stage][i] ? m_Samplers[stage][i]->m_i_Id : 0;
				std::memcpy(&key[i * sizeof(uint64_t)], &id, sizeof(id));
			}
			auto it = m_SamplerTables.find(key);
			uint32_t index = 0;
			if (it != m_SamplerTables.end()) {
				index = it->second;
			}
			else {
				index = m_i_SamplerHeapUsed;
				m_i_SamplerHeapUsed += SMP_SLOTS;
				D3D12_CPU_DESCRIPTOR_HANDLE dst = m_cls_GpuSamplerHeap->GetCPUDescriptorHandleForHeapStart();
				dst.ptr += (SIZE_T)index * m_i_SamplerIncrement;
				for (uint32_t i = 0; i < SMP_SLOTS; ++i) {
					D3D12_CPU_DESCRIPTOR_HANDLE src = m_Samplers[stage][i] ? m_Samplers[stage][i]->m_str_Handle : m_str_NullSampler;
					D3D12_CPU_DESCRIPTOR_HANDLE d = { dst.ptr + (SIZE_T)i * m_i_SamplerIncrement };
					m_cls_Core->cls_Device->CopyDescriptorsSimple(1, d, src, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
				}
				m_SamplerTables[key] = index;
			}
			D3D12_GPU_DESCRIPTOR_HANDLE gpu = m_cls_GpuSamplerHeap->GetGPUDescriptorHandleForHeapStart();
			gpu.ptr += (UINT64)index * m_i_SamplerIncrement;
			return gpu;
		}

		/*
		* 関数名　DrawPrepare
		* 引　数　なし
		* 戻り値　描画できるなら true
		* 説　明　D3D11 方式で Set されたステートをコマンドリストへ反映する
		*/
		bool D3D12Device::DrawPrepare()
		{
			D3D12_LOCK;
			CommandsBegin();

			// ディスクリプタヒープの残りが足りなければ GPU を待って空ける
			const uint32_t segment = GPU_SRV_HEAP_SIZE / m_i_FrameCount;
			if (m_i_SrvRingHead + STAGES * SRV_SLOTS > m_Frames[m_i_FrameSlot].i_SrvRingStart + segment) IdleWait();
			if (m_i_SamplerHeapUsed + STAGES * SMP_SLOTS > GPU_SAMPLER_HEAP_SIZE) {
				IdleWait();
				m_SamplerTables.clear();
				m_i_SamplerHeapUsed = 0;
			}

			// ---- 状態遷移 ----
			for (uint32_t s = 0; s < STAGES; ++s)
				for (uint32_t i = 0; i < SRV_SLOTS; ++i)
					if (m_Textures[s][i]) TextureTransition(TexCast(m_Textures[s][i]), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			for (uint32_t i = 0; i < m_str_RenderTargets.i_Count; ++i)
				if (m_str_RenderTargets.p_Colors[i]) TextureTransition(TexCast(m_str_RenderTargets.p_Colors[i]), D3D12_RESOURCE_STATE_RENDER_TARGET);
			if (m_str_RenderTargets.p_Depth) TextureTransition(TexCast(m_str_RenderTargets.p_Depth), D3D12_RESOURCE_STATE_DEPTH_WRITE);
			for (auto& vb : m_VertexBuffers)
				if (vb.p_Buffer && !vb.p_Buffer->IsStreamed()) BufferTransition(vb.p_Buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
			if (m_p_IndexBuffer && !m_p_IndexBuffer->IsStreamed()) BufferTransition(m_p_IndexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);
			if (!m_Barriers.empty()) {
				BarriersFlush();
				m_b_VertexBuffersDirty = m_b_IndexBufferDirty = true;
			}

			ID3D12PipelineState* pso = PipelineGet();
			if (!pso) return false;

			if (m_b_RootDirty) {
				ID3D12DescriptorHeap* heaps[] = { m_cls_GpuSrvHeap.Get(), m_cls_GpuSamplerHeap.Get() };
				m_cls_List->SetDescriptorHeaps(2, heaps);
				m_cls_List->SetGraphicsRootSignature(m_cls_RootSignature.Get());
				m_b_RootDirty = false;
				for (uint32_t s = 0; s < STAGES; ++s) m_b_SrvDirty[s] = m_b_SamplerDirty[s] = true;
			}
			if (pso != m_p_CurrentPipeline) {
				m_cls_List->SetPipelineState(pso);
				m_p_CurrentPipeline = pso;
			}

			// ---- 出力 ----
			if (m_b_RenderTargetsDirty) {
				D3D12_CPU_DESCRIPTOR_HANDLE rtvs[MAX_RENDER_TARGETS];
				for (uint32_t i = 0; i < m_str_RenderTargets.i_Count; ++i) {
					Texture* t = m_str_RenderTargets.p_Colors[i];
					rtvs[i] = (t && TexCast(t)->m_str_Rtv.ptr) ? TexCast(t)->m_str_Rtv : m_str_NullRtv;
				}
				D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_str_RenderTargets.p_Depth ? TexCast(m_str_RenderTargets.p_Depth)->m_str_Dsv : D3D12_CPU_DESCRIPTOR_HANDLE{ 0 };
				m_cls_List->OMSetRenderTargets(m_str_RenderTargets.i_Count, rtvs, FALSE, dsv.ptr ? &dsv : nullptr);
				m_b_RenderTargetsDirty = false;
			}
			if (m_b_ViewportDirty) {
				const D3D12_VIEWPORT vp = { m_str_Viewport.f_X, m_str_Viewport.f_Y, m_str_Viewport.f_Width, m_str_Viewport.f_Height,
					m_str_Viewport.f_MinDepth, m_str_Viewport.f_MaxDepth };
				m_cls_List->RSSetViewports(1, &vp);
				m_b_ViewportDirty = false;
			}
			if (m_b_ScissorDirty) {
				// D3D12 はシザーが常に有効なので、無効指定のときは全域にする
				const bool scissor = m_cls_Raster && m_cls_Raster->DescGet().b_Scissor;
				D3D12_RECT rc = { 0, 0, D3D12_VIEWPORT_BOUNDS_MAX, D3D12_VIEWPORT_BOUNDS_MAX };
				if (scissor) rc = { m_str_Scissor.i_Left, m_str_Scissor.i_Top, m_str_Scissor.i_Right, m_str_Scissor.i_Bottom };
				m_cls_List->RSSetScissorRects(1, &rc);
				m_b_ScissorDirty = false;
			}
			if (m_b_BlendFactorDirty) { m_cls_List->OMSetBlendFactor(m_f_BlendFactor); m_b_BlendFactorDirty = false; }
			if (m_b_StencilRefDirty) { m_cls_List->OMSetStencilRef(m_i_StencilRef); m_b_StencilRefDirty = false; }
			if (m_b_TopologyDirty) { m_cls_List->IASetPrimitiveTopology(TopologyGet(m_e_Topology)); m_b_TopologyDirty = false; }

			// ---- 頂点・インデックス（Streamed は内容が変われば毎回アドレスが変わる）----
			{
				D3D12_VERTEX_BUFFER_VIEW views[16] = {};
				uint32_t count = 0;
				bool changed = m_b_VertexBuffersDirty;
				for (uint32_t i = 0; i < 16; ++i) {
					const VertexBinding& vb = m_VertexBuffers[i];
					if (!vb.p_Buffer) continue;
					count = i + 1;
					D3D12_GPU_VIRTUAL_ADDRESS addr;
					if (vb.p_Buffer->IsStreamed()) {
						if (vb.p_Buffer->m_b_Dirty || vb.p_Buffer->m_i_UploadFrame != m_i_Frame) changed = true;
						addr = StreamedBufferResolve(vb.p_Buffer);
					}
					else addr = vb.p_Buffer->m_cls_Resource->GetGPUVirtualAddress();
					views[i].BufferLocation = addr + vb.i_Offset;
					views[i].SizeInBytes = vb.p_Buffer->GetSize() > vb.i_Offset ? vb.p_Buffer->GetSize() - vb.i_Offset : 0;
					views[i].StrideInBytes = vb.i_Stride;
				}
				if (changed && count > 0) m_cls_List->IASetVertexBuffers(0, count, views);
				m_b_VertexBuffersDirty = false;
			}
			if (m_p_IndexBuffer) {
				bool changed = m_b_IndexBufferDirty;
				D3D12_GPU_VIRTUAL_ADDRESS addr;
				if (m_p_IndexBuffer->IsStreamed()) {
					if (m_p_IndexBuffer->m_b_Dirty || m_p_IndexBuffer->m_i_UploadFrame != m_i_Frame) changed = true;
					addr = StreamedBufferResolve(m_p_IndexBuffer);
				}
				else addr = m_p_IndexBuffer->m_cls_Resource->GetGPUVirtualAddress();
				if (changed) {
					D3D12_INDEX_BUFFER_VIEW iv = {};
					iv.BufferLocation = addr + m_i_IndexOffset;
					iv.SizeInBytes = m_p_IndexBuffer->GetSize() > m_i_IndexOffset ? m_p_IndexBuffer->GetSize() - m_i_IndexOffset : 0;
					iv.Format = (m_e_IndexFormat == IndexFormat::UInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
					m_cls_List->IASetIndexBuffer(&iv);
				}
				m_b_IndexBufferDirty = false;
			}

			// ---- 定数バッファ（ルート CBV は記録時に値が確定するので毎回設定する）----
			const D3D12_GPU_VIRTUAL_ADDRESS dummy = m_cls_DummyCb->GetGPUVirtualAddress();
			for (uint32_t s = 0; s < STAGES; ++s) {
				for (uint32_t i = 0; i < CB_SLOTS; ++i) {
					D3D12Buffer* cb = m_ConstantBuffers[s][i];
					const D3D12_GPU_VIRTUAL_ADDRESS addr = (cb && cb->IsStreamed()) ? StreamedBufferResolve(cb) : dummy;
					m_cls_List->SetGraphicsRootConstantBufferView(ROOT_CBV_BASE + s * CB_SLOTS + i, addr);
				}
			}

			// ---- テクスチャ・サンプラー ----
			ID3D12Device* dev = m_cls_Core->cls_Device.Get();
			for (uint32_t s = 0; s < STAGES; ++s) {
				if (m_b_SrvDirty[s]) {
					const uint32_t base = m_i_SrvRingHead;
					m_i_SrvRingHead += SRV_SLOTS;
					D3D12_CPU_DESCRIPTOR_HANDLE dst = m_cls_GpuSrvHeap->GetCPUDescriptorHandleForHeapStart();
					dst.ptr += (SIZE_T)base * m_i_SrvIncrement;
					for (uint32_t i = 0; i < SRV_SLOTS; ++i) {
						Texture* t = m_Textures[s][i];
						D3D12_CPU_DESCRIPTOR_HANDLE src = t ? TexCast(t)->m_str_Srv : m_str_NullSrv;
						D3D12_CPU_DESCRIPTOR_HANDLE d = { dst.ptr + (SIZE_T)i * m_i_SrvIncrement };
						dev->CopyDescriptorsSimple(1, d, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					}
					D3D12_GPU_DESCRIPTOR_HANDLE gpu = m_cls_GpuSrvHeap->GetGPUDescriptorHandleForHeapStart();
					gpu.ptr += (UINT64)base * m_i_SrvIncrement;
					m_cls_List->SetGraphicsRootDescriptorTable(ROOT_SRV_TABLE + s, gpu);
					m_b_SrvDirty[s] = false;
				}
				if (m_b_SamplerDirty[s]) {
					m_cls_List->SetGraphicsRootDescriptorTable(ROOT_SMP_TABLE + s, SamplerTableGet(s));
					m_b_SamplerDirty[s] = false;
				}
			}
			return true;
		}

		void D3D12Device::Draw(uint32_t vertex_count, uint32_t start_vertex)
		{
			if (DrawPrepare()) m_cls_List->DrawInstanced(vertex_count, 1, start_vertex, 0);
		}

		void D3D12Device::IndexedDraw(uint32_t index_count, uint32_t start_index, int32_t base_vertex)
		{
			if (DrawPrepare()) m_cls_List->DrawIndexedInstanced(index_count, 1, start_index, base_vertex, 0);
		}

		void D3D12Device::InstancedDraw(uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex, uint32_t start_instance)
		{
			if (DrawPrepare()) m_cls_List->DrawInstanced(vertex_count, instance_count, start_vertex, start_instance);
		}

		/*
		* 関数名　NativeCommandsBegin
		* 引　数　なし
		* 戻り値　ID3D12GraphicsCommandList*
		* 説　明　描画先の状態遷移と設定を済ませてからコマンドリストを渡す
		*/
		void* D3D12Device::NativeCommandsBegin()
		{
			D3D12_LOCK;
			CommandsBegin();
			for (uint32_t i = 0; i < m_str_RenderTargets.i_Count; ++i)
				if (m_str_RenderTargets.p_Colors[i]) TextureTransition(TexCast(m_str_RenderTargets.p_Colors[i]), D3D12_RESOURCE_STATE_RENDER_TARGET);
			if (m_str_RenderTargets.p_Depth) TextureTransition(TexCast(m_str_RenderTargets.p_Depth), D3D12_RESOURCE_STATE_DEPTH_WRITE);
			BarriersFlush();
			D3D12_CPU_DESCRIPTOR_HANDLE rtvs[MAX_RENDER_TARGETS];
			for (uint32_t i = 0; i < m_str_RenderTargets.i_Count; ++i) {
				Texture* t = m_str_RenderTargets.p_Colors[i];
				rtvs[i] = (t && TexCast(t)->m_str_Rtv.ptr) ? TexCast(t)->m_str_Rtv : m_str_NullRtv;
			}
			D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_str_RenderTargets.p_Depth ? TexCast(m_str_RenderTargets.p_Depth)->m_str_Dsv : D3D12_CPU_DESCRIPTOR_HANDLE{ 0 };
			m_cls_List->OMSetRenderTargets(m_str_RenderTargets.i_Count, rtvs, FALSE, dsv.ptr ? &dsv : nullptr);
			const D3D12_VIEWPORT vp = { m_str_Viewport.f_X, m_str_Viewport.f_Y, m_str_Viewport.f_Width, m_str_Viewport.f_Height,
				m_str_Viewport.f_MinDepth, m_str_Viewport.f_MaxDepth };
			m_cls_List->RSSetViewports(1, &vp);
			const D3D12_RECT rc = { 0, 0, (LONG)vp.Width + (LONG)vp.TopLeftX, (LONG)vp.Height + (LONG)vp.TopLeftY };
			m_cls_List->RSSetScissorRects(1, &rc);
			return m_cls_List.Get();
		}

		void D3D12Device::NativeCommandsEnd()
		{
			// 外部ライブラリがヒープ・ルートシグネチャ・PSO などを変更している
			AllDirtyMark();
		}

		void D3D12Device::MarkerBegin(const char* name)
		{
			if (!name || !m_b_ListOpen) return;
			wchar_t wname[128] = {};
			const int len = MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, 127);
			// メタデータ 0 = PIX の Unicode 文字列
			m_cls_List->BeginEvent(0, wname, (UINT)(len * sizeof(wchar_t)));
		}

		void D3D12Device::MarkerEnd()
		{
			if (m_b_ListOpen) m_cls_List->EndEvent();
		}
	}

	// ================================================================
	// 公開関数
	// ================================================================

	std::unique_ptr<Device> D3D12DeviceCreate(const DeviceCreateInfo& info, std::string* out_error)
	{
		auto device = std::make_unique<D3D12Device>();
		if (!device->Init(info, out_error)) return nullptr;
		LogWrite("D3D12: %s", device->AdapterNameGet());
		return device;
	}

	ID3D12Resource* D3D12ResourceGet(Texture* texture)
	{
		return texture ? TexCast(texture)->m_cls_Resource.Get() : nullptr;
	}

	uint32_t D3D12FramesInFlightGet(Device* device)
	{
		if (!device || device->BackendGet() != Backend::D3D12) return 0;
		return static_cast<D3D12Device*>(device)->FramesInFlightGet();
	}
}
