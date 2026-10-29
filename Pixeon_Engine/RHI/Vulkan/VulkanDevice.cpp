/*
* ファイル名　VulkanDevice
* 説　　　明　RHI の Vulkan 1.3 バックエンド
*
* D3D11 方式のイミディエイト API を次の仕組みでエミュレートする
*   レンダーパス  : Dynamic Rendering (1.3)。描画先が変わったとき・転送処理の前に開始 / 終了する
*   座標系        : ビューポートの高さを負にして D3D と同じ上向き Y・左上原点にする
*   定数バッファ  : CPU 側に内容を持ち、Draw 時に変更があればアップロードリングへ書き出す
*   ディスクリプタ: 全シェーダー共通のセット 1 つ（ShaderBindings の binding 配置）を、
*                   バインド内容が変わったときだけフレーム用プールから確保して書き込む
*   PSO           : Draw 時点のステートからキーを作ってキャッシュ。頂点ストライドは動的ステート
*   入出力        : 頂点レイアウトはセマンティクス名で VS 入力の location に対応付け、
*                   PS 入力の location は VS 出力に合わせて SPIR-V を書き換える
*   状態遷移      : 画像レイアウトを追跡して自動でバリアを張る
*   破棄          : タイムラインセマフォで GPU の完了を確認してから解放する
*/
// Vulkan SDK が無い環境 (PIXEON_RHI_VULKAN 未定義) では何もコンパイルしない
#if defined(PIXEON_RHI_VULKAN)

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

#include "../RHI.h"
#include "../../../Common/ShaderBindings.h"
#include "SpirvReflect.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_map>

#if defined(_MSC_VER)
#pragma comment(lib, "vulkan-1.lib")
#endif

namespace RHI {
	namespace {
		constexpr uint32_t STAGES = ShaderBindings::STAGE_COUNT;
		constexpr uint32_t CB_SLOTS = ShaderBindings::CB_SLOT_COUNT;
		constexpr uint32_t SRV_SLOTS = ShaderBindings::SRV_SLOT_COUNT;
		constexpr uint32_t SMP_SLOTS = ShaderBindings::SAMPLER_SLOT_COUNT;
		constexpr uint32_t BINDING_COUNT = ShaderBindings::STAGE_STRIDE * STAGES;
		constexpr uint64_t UPLOAD_PAGE_SIZE = 4ull * 1024 * 1024;
		constexpr uint32_t DUMMY_UBO_SIZE = 65536;
		constexpr uint32_t MAX_VERTEX_SLOTS = 16;

		uint64_t ObjectIdNext()
		{
			static uint64_t s_id = 0;
			return ++s_id;
		}

		const char* VkResultName(VkResult r)
		{
			switch (r) {
			case VK_SUCCESS: return "VK_SUCCESS";
			case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
			case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
			case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
			case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
			case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
			case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
			case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
			case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
			case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
			case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
			case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
			default: return "VkResult";
			}
		}

		// ---------------------------------------------------------------
		// 列挙型の変換
		// ---------------------------------------------------------------
		VkFormat VkFormatGet(Format f)
		{
			switch (f) {
			case Format::R8_UNorm:              return VK_FORMAT_R8_UNORM;
			case Format::R8G8B8A8_UNorm:        return VK_FORMAT_R8G8B8A8_UNORM;
			case Format::R8G8B8A8_UNorm_sRGB:   return VK_FORMAT_R8G8B8A8_SRGB;
			case Format::B8G8R8A8_UNorm:        return VK_FORMAT_B8G8R8A8_UNORM;
			case Format::B8G8R8A8_UNorm_sRGB:   return VK_FORMAT_B8G8R8A8_SRGB;
			case Format::R16_Float:             return VK_FORMAT_R16_SFLOAT;
			case Format::R16G16_Float:          return VK_FORMAT_R16G16_SFLOAT;
			case Format::R16G16B16A16_Float:    return VK_FORMAT_R16G16B16A16_SFLOAT;
			case Format::R32_Float:             return VK_FORMAT_R32_SFLOAT;
			case Format::R32G32_Float:          return VK_FORMAT_R32G32_SFLOAT;
			case Format::R32G32B32_Float:       return VK_FORMAT_R32G32B32_SFLOAT;
			case Format::R32G32B32A32_Float:    return VK_FORMAT_R32G32B32A32_SFLOAT;
			case Format::R16_UInt:              return VK_FORMAT_R16_UINT;
			case Format::R32_UInt:              return VK_FORMAT_R32_UINT;
			case Format::R32G32B32A32_UInt:     return VK_FORMAT_R32G32B32A32_UINT;
			case Format::D24_UNorm_S8_UInt:     return VK_FORMAT_D24_UNORM_S8_UINT;
			case Format::D32_Float:             return VK_FORMAT_D32_SFLOAT;
			case Format::BC1_UNorm:             return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
			case Format::BC1_UNorm_sRGB:        return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
			case Format::BC2_UNorm:             return VK_FORMAT_BC2_UNORM_BLOCK;
			case Format::BC2_UNorm_sRGB:        return VK_FORMAT_BC2_SRGB_BLOCK;
			case Format::BC3_UNorm:             return VK_FORMAT_BC3_UNORM_BLOCK;
			case Format::BC3_UNorm_sRGB:        return VK_FORMAT_BC3_SRGB_BLOCK;
			case Format::BC4_UNorm:             return VK_FORMAT_BC4_UNORM_BLOCK;
			case Format::BC5_UNorm:             return VK_FORMAT_BC5_UNORM_BLOCK;
			case Format::BC6H_UF16:             return VK_FORMAT_BC6H_UFLOAT_BLOCK;
			case Format::BC7_UNorm:             return VK_FORMAT_BC7_UNORM_BLOCK;
			case Format::BC7_UNorm_sRGB:        return VK_FORMAT_BC7_SRGB_BLOCK;
			default:                            return VK_FORMAT_UNDEFINED;
			}
		}
		VkBlendFactor BlendGet(Blend b)
		{
			static const VkBlendFactor table[] = {
				VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
				VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
				VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
				VK_BLEND_FACTOR_CONSTANT_COLOR, VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
			};
			return table[(int)b];
		}
		VkBlendOp BlendOpGet(BlendOp o)
		{
			static const VkBlendOp table[] = { VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT, VK_BLEND_OP_REVERSE_SUBTRACT, VK_BLEND_OP_MIN, VK_BLEND_OP_MAX };
			return table[(int)o];
		}
		VkCompareOp CompareGet(CompareFunc c)
		{
			static const VkCompareOp table[] = {
				VK_COMPARE_OP_NEVER, VK_COMPARE_OP_LESS, VK_COMPARE_OP_EQUAL, VK_COMPARE_OP_LESS_OR_EQUAL,
				VK_COMPARE_OP_GREATER, VK_COMPARE_OP_NOT_EQUAL, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_ALWAYS,
			};
			return table[(int)c];
		}
		VkStencilOp StencilOpGet(StencilOp o)
		{
			static const VkStencilOp table[] = {
				VK_STENCIL_OP_KEEP, VK_STENCIL_OP_ZERO, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_INCREMENT_AND_CLAMP,
				VK_STENCIL_OP_DECREMENT_AND_CLAMP, VK_STENCIL_OP_INVERT, VK_STENCIL_OP_INCREMENT_AND_WRAP, VK_STENCIL_OP_DECREMENT_AND_WRAP,
			};
			return table[(int)o];
		}
		VkSamplerAddressMode AddressGet(AddressMode a)
		{
			static const VkSamplerAddressMode table[] = {
				VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
				VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
			};
			return table[(int)a];
		}
		VkPrimitiveTopology TopologyGet(Topology t)
		{
			switch (t) {
			case Topology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			case Topology::LineList:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			case Topology::LineStrip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
			case Topology::PointList:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			default:                      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			}
		}
		VkImageAspectFlags AspectGet(Format f)
		{
			const FormatInfo& info = FormatInfoGet(f);
			if (!info.b_Depth) return VK_IMAGE_ASPECT_COLOR_BIT;
			return info.b_Stencil ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) : VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		std::string UpperGet(const char* s)
		{
			std::string r = s ? s : "";
			std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return (char)std::toupper(c); });
			return r;
		}

		// PSO キー用のバイト列ビルダー（構造体のパディングを含めないため 1 項目ずつ書く）
		class KeyWriter {
		public:
			template<typename T> KeyWriter& operator<<(const T& v)
			{
				static_assert(std::is_arithmetic<T>::value || std::is_enum<T>::value, "スカラーのみ");
				m_Bytes.append((const char*)&v, sizeof(T));
				return *this;
			}
			const std::string& Get() const { return m_Bytes; }
		private:
			std::string m_Bytes;
		};

		// ---------------------------------------------------------------
		// デバイス本体の共有部分（リソースが shared_ptr で保持する）
		// ---------------------------------------------------------------
		struct VulkanCore {
			VkInstance p_Instance = VK_NULL_HANDLE;
			VkDebugUtilsMessengerEXT p_Messenger = VK_NULL_HANDLE;
			VkPhysicalDevice p_Physical = VK_NULL_HANDLE;
			VkDevice p_Device = VK_NULL_HANDLE;
			VkQueue p_Queue = VK_NULL_HANDLE;
			uint32_t i_QueueFamily = 0;
			VkSemaphore p_Timeline = VK_NULL_HANDLE;
			uint64_t i_NextValue = 1;     // 次の提出で Signal する値
			VkPhysicalDeviceProperties str_Props = {};
			VkPhysicalDeviceMemoryProperties str_MemProps = {};
			VkPhysicalDeviceFeatures str_Features = {};
			VkFormat e_D24Format = VK_FORMAT_D24_UNORM_S8_UINT;   // 非対応 GPU では D32S8 に置き換える

			std::recursive_mutex deviceMutex;
			void* p_Owner = nullptr;
			void (*p_UnbindFn)(void* owner, const void* resource) = nullptr;

			struct Pending { uint64_t i_Value; std::function<void()> fn_Destroy; };
			std::deque<Pending> deferred;
			std::mutex mutex;

			~VulkanCore()
			{
				if (p_Device) {
					vkDeviceWaitIdle(p_Device);
					for (auto& p : deferred) p.fn_Destroy();
					deferred.clear();
					if (p_Timeline) vkDestroySemaphore(p_Device, p_Timeline, nullptr);
					vkDestroyDevice(p_Device, nullptr);
				}
				if (p_Messenger) {
					auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(p_Instance, "vkDestroyDebugUtilsMessengerEXT");
					if (fn) fn(p_Instance, p_Messenger, nullptr);
				}
				if (p_Instance) vkDestroyInstance(p_Instance, nullptr);
			}

			void Defer(std::function<void()> fn)
			{
				std::lock_guard<std::mutex> lock(mutex);
				deferred.push_back({ i_NextValue, std::move(fn) });
			}
			uint64_t CompletedGet() const
			{
				uint64_t v = 0;
				if (p_Timeline) vkGetSemaphoreCounterValue(p_Device, p_Timeline, &v);
				return v;
			}
			void Collect()
			{
				const uint64_t done = CompletedGet();
				std::vector<std::function<void()>> ready;
				{
					std::lock_guard<std::mutex> lock(mutex);
					while (!deferred.empty() && deferred.front().i_Value <= done) {
						ready.push_back(std::move(deferred.front().fn_Destroy));
						deferred.pop_front();
					}
				}
				for (auto& fn : ready) fn();
			}
			void Wait(uint64_t value)
			{
				if (value == 0 || CompletedGet() >= value) return;
				VkSemaphoreWaitInfo wi = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
				wi.semaphoreCount = 1;
				wi.pSemaphores = &p_Timeline;
				wi.pValues = &value;
				vkWaitSemaphores(p_Device, &wi, UINT64_MAX);
			}
			void Unbind(const void* resource)
			{
				std::lock_guard<std::recursive_mutex> lock(deviceMutex);
				if (p_Owner && p_UnbindFn) p_UnbindFn(p_Owner, resource);
			}
			int32_t MemoryTypeFind(uint32_t bits, VkMemoryPropertyFlags flags) const
			{
				for (uint32_t i = 0; i < str_MemProps.memoryTypeCount; ++i)
					if ((bits & (1u << i)) && (str_MemProps.memoryTypes[i].propertyFlags & flags) == flags) return (int32_t)i;
				return -1;
			}
			VkFormat FormatGet(Format f) const
			{
				return f == Format::D24_UNorm_S8_UInt ? e_D24Format : VkFormatGet(f);
			}
		};
		using CoreRef = std::shared_ptr<VulkanCore>;

		// ---------------------------------------------------------------
		// リソース
		// ---------------------------------------------------------------
		class VulkanBuffer final : public Buffer {
		public:
			VulkanBuffer(const BufferDesc& desc, CoreRef core) : Buffer(desc), m_cls_Core(std::move(core)) {}
			~VulkanBuffer() override
			{
				m_cls_Core->Unbind(static_cast<Buffer*>(this));
				if (m_p_Buffer || m_p_Memory) {
					VkDevice dev = m_cls_Core->p_Device;
					VkBuffer b = m_p_Buffer;
					VkDeviceMemory m = m_p_Memory;
					m_cls_Core->Defer([dev, b, m]() { vkDestroyBuffer(dev, b, nullptr); vkFreeMemory(dev, m, nullptr); });
				}
			}
			bool IsStreamed() const { return !m_Shadow.empty(); }

			CoreRef m_cls_Core;
			VkBuffer m_p_Buffer = VK_NULL_HANDLE;          // Device local（Streamed では空）
			VkDeviceMemory m_p_Memory = VK_NULL_HANDLE;
			std::vector<uint8_t> m_Shadow;
			bool m_b_Dirty = true;
			uint64_t m_i_UploadFrame = UINT64_MAX;
			VkBuffer m_p_StreamBuffer = VK_NULL_HANDLE;    // 最後に書き出したアップロードページ
			VkDeviceSize m_i_StreamOffset = 0;
		};

		class VulkanTexture final : public Texture {
		public:
			VulkanTexture(const TextureDesc& desc, CoreRef core) : Texture(desc), m_cls_Core(std::move(core)) {}
			~VulkanTexture() override
			{
				m_cls_Core->Unbind(static_cast<Texture*>(this));
				if (m_b_External) return;
				VkDevice dev = m_cls_Core->p_Device;
				VkImage img = m_p_Image;
				VkDeviceMemory mem = m_p_Memory;
				VkImageView srv = m_p_SrvView, att = m_p_AttachView;
				m_cls_Core->Defer([dev, img, mem, srv, att]() {
					if (srv) vkDestroyImageView(dev, srv, nullptr);
					if (att) vkDestroyImageView(dev, att, nullptr);
					if (img) vkDestroyImage(dev, img, nullptr);
					if (mem) vkFreeMemory(dev, mem, nullptr);
				});
			}
			void SizeSet(uint32_t w, uint32_t h) { m_str_Desc.i_Width = w; m_str_Desc.i_Height = h; }
			void FormatSet(Format f) { m_str_Desc.e_Format = f; }

			CoreRef m_cls_Core;
			VkImage m_p_Image = VK_NULL_HANDLE;
			VkDeviceMemory m_p_Memory = VK_NULL_HANDLE;
			VkImageView m_p_SrvView = VK_NULL_HANDLE;      // サンプリング用（深度はデプス面のみ）
			VkImageView m_p_AttachView = VK_NULL_HANDLE;   // 描画先用（ミップ 0）
			VkFormat m_e_VkFormat = VK_FORMAT_UNDEFINED;
			VkImageAspectFlags m_i_Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
			VkImageLayout m_e_Layout = VK_IMAGE_LAYOUT_UNDEFINED;
			bool m_b_External = false;   // スワップチェーンの画像（デバイスが管理）
		};

		class VulkanSampler final : public Sampler {
		public:
			VulkanSampler(const SamplerDesc& desc, CoreRef core) : Sampler(desc), m_cls_Core(std::move(core)), m_i_Id(ObjectIdNext()) {}
			~VulkanSampler() override
			{
				m_cls_Core->Unbind(static_cast<Sampler*>(this));
				VkDevice dev = m_cls_Core->p_Device;
				VkSampler s = m_p_Sampler;
				if (s) m_cls_Core->Defer([dev, s]() { vkDestroySampler(dev, s, nullptr); });
			}
			CoreRef m_cls_Core;
			VkSampler m_p_Sampler = VK_NULL_HANDLE;
			uint64_t m_i_Id;
		};

		class VulkanShader final : public Shader {
		public:
			VulkanShader(ShaderStage stage, const void* code, size_t size, const char* name, CoreRef core)
				: Shader(stage, code, size, name), m_cls_Core(std::move(core)), m_i_Id(ObjectIdNext())
			{
				m_Words.resize(size / 4);
				std::memcpy(m_Words.data(), code, m_Words.size() * 4);
				m_b_Valid = SpirvReflect(m_Words.data(), m_Words.size(), stage, m_str_Reflection);
			}
			~VulkanShader() override
			{
				VkDevice dev = m_cls_Core->p_Device;
				VkShaderModule mod = m_p_Module;
				if (mod) m_cls_Core->Defer([dev, mod]() { vkDestroyShaderModule(dev, mod, nullptr); });
			}
			CoreRef m_cls_Core;
			std::vector<uint32_t> m_Words;
			VkShaderModule m_p_Module = VK_NULL_HANDLE;    // VS のみ（PS は VS に合わせて書き換えるため PSO ごとに作る）
			uint64_t m_i_Id;
			bool m_b_Valid = false;
		};

		class VulkanInputLayout final : public InputLayout {
		public:
			VulkanInputLayout(const InputElement* e, uint32_t n) : InputLayout(e, n), m_i_Id(ObjectIdNext()) {}
			uint64_t m_i_Id;
		};

		class VulkanBlendState final : public BlendState { public: explicit VulkanBlendState(const BlendDesc& d) : BlendState(d) {} };
		class VulkanDepthStencilState final : public DepthStencilState { public: explicit VulkanDepthStencilState(const DepthStencilDesc& d) : DepthStencilState(d) {} };
		class VulkanRasterizerState final : public RasterizerState { public: explicit VulkanRasterizerState(const RasterizerDesc& d) : RasterizerState(d) {} };

		VulkanTexture* TexCast(Texture* t) { return static_cast<VulkanTexture*>(t); }
		VulkanBuffer* BufCast(Buffer* b) { return static_cast<VulkanBuffer*>(b); }

#define VK_LOCK std::lock_guard<std::recursive_mutex> lock_(m_cls_Core->deviceMutex)

		struct Pipeline {
			VkPipeline p_Pipeline = VK_NULL_HANDLE;
			uint32_t i_VertexSlotMask = 0;   // PSO が参照する頂点バッファのスロット
		};

		// ---------------------------------------------------------------
		// デバイス
		// ---------------------------------------------------------------
		class VulkanDevice final : public Device {
		public:
			~VulkanDevice() override;
			bool Init(const DeviceCreateInfo& info, std::string* out_error);

			Backend BackendGet() const override { return Backend::Vulkan; }
			const char* AdapterNameGet() const override { return m_cls_Core->str_Props.deviceName; }
			NativeHandles NativeHandlesGet() const override;
			uint64_t FrameIndexGet() const override { return m_i_Frame; }

			BufferRef BufferCreate(const BufferDesc& desc, const void* initial_data) override;
			TextureRef TextureCreate(const TextureDesc& desc, const SubresourceData* initial_data) override;
			SamplerRef SamplerCreate(const SamplerDesc& desc) override;
			ShaderRef ShaderCreate(ShaderStage stage, const void* bytecode, size_t size, const char* name) override;
			InputLayoutRef InputLayoutCreate(const InputElement* elements, uint32_t count, const Shader* vs) override;
			BlendStateRef BlendStateCreate(const BlendDesc& desc) override { return std::make_shared<VulkanBlendState>(desc); }
			DepthStencilStateRef DepthStencilStateCreate(const DepthStencilDesc& desc) override { return std::make_shared<VulkanDepthStencilState>(desc); }
			RasterizerStateRef RasterizerStateCreate(const RasterizerDesc& desc) override { return std::make_shared<VulkanRasterizerState>(desc); }

			void BufferUpdate(Buffer* buffer, const void* data, uint32_t size, uint32_t offset) override;
			void* BufferMap(Buffer* buffer) override;
			void BufferUnmap(Buffer* buffer) override;
			void TextureUpdate(Texture* texture, uint32_t mip, const void* data, uint32_t row_pitch) override;
			void TextureCopy(Texture* dst, Texture* src) override;
			bool TextureRead(Texture* texture, std::vector<uint8_t>& out_pixels, uint32_t& out_row_pitch) override;

			Texture* BackBufferGet() override { return m_cls_BackBuffer.get(); }
			void Present() override;
			void SwapChainResize(uint32_t width, uint32_t height) override;
			void VSyncSet(bool enable) override;
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
			void InputLayoutSet(InputLayout* layout) override { m_cls_Layout = static_cast<VulkanInputLayout*>(layout); }
			void TopologySet(Topology topology) override { m_e_Topology = topology; }
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

			void ResourceUnbind(const void* resource);

		private:
			struct UploadPage {
				VkBuffer p_Buffer = VK_NULL_HANDLE;
				VkDeviceMemory p_Memory = VK_NULL_HANDLE;
				uint8_t* p_Cpu = nullptr;
				VkDeviceSize i_Size = 0;
				VkDeviceSize i_Offset = 0;
			};
			struct UploadAlloc {
				VkBuffer p_Buffer = VK_NULL_HANDLE;
				VkDeviceSize i_Offset = 0;
				uint8_t* p_Cpu = nullptr;
			};
			struct FrameContext {
				VkCommandPool p_Pool = VK_NULL_HANDLE;
				VkCommandBuffer p_Cmd = VK_NULL_HANDLE;
				VkSemaphore p_Acquire = VK_NULL_HANDLE;
				uint64_t i_Value = 0;                        // このフレームの提出の完了値
				std::vector<UploadPage> uploadPages;
				std::vector<VkDescriptorPool> descriptorPools;
				size_t i_PoolIndex = 0;
			};
			// 書き込んだディスクリプタセットの内容（変化がなければ使い回す）
			struct DescriptorEntry {
				VkBuffer p_Buffer = VK_NULL_HANDLE;
				VkDeviceSize i_Offset = 0;
				VkDeviceSize i_Range = 0;
				VkImageView p_View = VK_NULL_HANDLE;
				VkSampler p_Sampler = VK_NULL_HANDLE;
				bool operator==(const DescriptorEntry& o) const
				{
					return p_Buffer == o.p_Buffer && i_Offset == o.i_Offset && i_Range == o.i_Range && p_View == o.p_View && p_Sampler == o.p_Sampler;
				}
			};

			bool InstanceCreate(const DeviceCreateInfo& info, std::string& error);
			bool PhysicalDeviceSelect(std::string& error);
			bool LogicalDeviceCreate(std::string& error);
			bool CommonObjectsCreate(std::string& error);
			bool SwapchainCreate(uint32_t width, uint32_t height);
			void SwapchainDestroy();
			bool ImageAcquire();

			void FrameBegin();
			void CommandsBegin();
			void CommandsSubmit(bool present_signal);
			UploadAlloc UploadAllocate(VkDeviceSize size, VkDeviceSize alignment);
			void StreamedBufferResolve(VulkanBuffer* buffer, VkBuffer& out_buffer, VkDeviceSize& out_offset);

			void RenderingEnd();
			void ImageTransition(VulkanTexture* tex, VkImageLayout layout);
			void BarriersFlush();
			void TransferBarrier(bool before);
			bool ImageCreate(VulkanTexture* tex);
			void BoundTextureUnbind(Texture* texture);

			bool DrawPrepare();
			const Pipeline* PipelineGet();
			bool DescriptorSetUpdate();
			VkDescriptorSet DescriptorSetAllocate();
			void AllDirtyMark();

			CoreRef m_cls_Core;
			bool m_b_DebugUtils = false;
			PFN_vkCmdBeginDebugUtilsLabelEXT m_fn_LabelBegin = nullptr;
			PFN_vkCmdEndDebugUtilsLabelEXT m_fn_LabelEnd = nullptr;

			VkDescriptorSetLayout m_p_SetLayout = VK_NULL_HANDLE;
			VkPipelineLayout m_p_PipelineLayout = VK_NULL_HANDLE;
			VkPipelineCache m_p_PipelineCache = VK_NULL_HANDLE;
			std::unordered_map<std::string, Pipeline> m_Pipelines;
			const Pipeline* m_p_CurrentPipeline = nullptr;

			// 未設定スロット用
			std::shared_ptr<VulkanTexture> m_cls_NullTexture;
			VkImageView m_p_NullArrayView = VK_NULL_HANDLE;
			VkSampler m_p_NullSampler = VK_NULL_HANDLE;
			VkBuffer m_p_DummyUbo = VK_NULL_HANDLE;
			VkDeviceMemory m_p_DummyUboMemory = VK_NULL_HANDLE;

			// フレーム
			std::vector<FrameContext> m_Frames;
			uint32_t m_i_FrameCount = 2;
			uint32_t m_i_FrameSlot = 0;
			uint64_t m_i_Frame = 0;
			VkCommandBuffer m_p_Cmd = VK_NULL_HANDLE;
			bool m_b_Recording = false;
			bool m_b_AcquirePending = false;   // 取得したスワップチェーン画像の待機をまだ提出していない
			std::vector<UploadPage> m_FreePages;

			// スワップチェーン
			VkSurfaceKHR m_p_Surface = VK_NULL_HANDLE;
			VkSwapchainKHR m_p_Swapchain = VK_NULL_HANDLE;
			std::vector<VkImage> m_SwapImages;
			std::vector<VkImageView> m_SwapViews;
			std::vector<VkSemaphore> m_SwapRenderDone;
			VkFormat m_e_SwapFormat = VK_FORMAT_UNDEFINED;
			uint32_t m_i_ImageIndex = 0;
			bool m_b_ImageAcquired = false;
			bool m_b_SwapchainDirty = false;
			std::shared_ptr<VulkanTexture> m_cls_BackBuffer;
			bool m_b_Headless = false;
			bool m_b_VSync = true;

			// D3D11 方式のステート
			std::vector<VkImageMemoryBarrier> m_Barriers;
			bool m_b_Rendering = false;
			RenderTargetState m_str_RenderTargets;
			Viewport m_str_Viewport;
			Rect m_str_Scissor;
			VkExtent2D m_str_RenderExtent = { 1, 1 };
			BlendState* m_cls_Blend = nullptr;
			DepthStencilState* m_cls_Depth = nullptr;
			RasterizerState* m_cls_Raster = nullptr;
			float m_f_BlendFactor[4] = {};
			uint32_t m_i_SampleMask = 0xFFFFFFFFu;
			uint32_t m_i_StencilRef = 0;
			VulkanShader* m_cls_Shaders[STAGES] = {};
			VulkanInputLayout* m_cls_Layout = nullptr;
			Topology m_e_Topology = Topology::TriangleList;
			struct VertexBinding { VulkanBuffer* p_Buffer = nullptr; uint32_t i_Stride = 0; uint32_t i_Offset = 0; };
			VertexBinding m_VertexBuffers[MAX_VERTEX_SLOTS];
			VulkanBuffer* m_p_IndexBuffer = nullptr;
			IndexFormat m_e_IndexFormat = IndexFormat::UInt16;
			uint32_t m_i_IndexOffset = 0;
			VulkanBuffer* m_ConstantBuffers[STAGES][CB_SLOTS] = {};
			Texture* m_Textures[STAGES][SRV_SLOTS] = {};
			VulkanSampler* m_Samplers[STAGES][SMP_SLOTS] = {};

			DescriptorEntry m_LastDescriptors[BINDING_COUNT];
			bool m_b_DescriptorValid = false;
			bool m_b_RenderTargetsDirty = true;
			bool m_b_ViewportDirty = true;
			bool m_b_ScissorDirty = true;
			bool m_b_DynamicDirty = true;
		};

		// ================================================================
		// 初期化
		// ================================================================

		VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
			const VkDebugUtilsMessengerCallbackDataEXT* data, void*)
		{
			if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) LogWrite("Vulkan: %s", data->pMessage);
			return VK_FALSE;
		}

		bool VulkanDevice::Init(const DeviceCreateInfo& info, std::string* out_error)
		{
			std::string error;
			m_cls_Core = std::make_shared<VulkanCore>();
			m_cls_Core->p_Owner = this;
			m_cls_Core->p_UnbindFn = [](void* owner, const void* r) { static_cast<VulkanDevice*>(owner)->ResourceUnbind(r); };
			m_b_Headless = (info.p_WindowHandle == nullptr);
			m_b_VSync = info.b_VSync;
			m_i_FrameCount = std::max<uint32_t>(2, std::min<uint32_t>(info.i_FramesInFlight, 3));

			bool ok = InstanceCreate(info, error) && PhysicalDeviceSelect(error) && LogicalDeviceCreate(error) && CommonObjectsCreate(error);
			if (ok) {
				if (m_b_Headless) {
					TextureDesc bd;
					bd.i_Width = info.i_Width;
					bd.i_Height = info.i_Height;
					bd.e_Format = Format::R8G8B8A8_UNorm;
					bd.i_BindFlags = Bind_RenderTarget | Bind_ShaderResource;
					bd.c_DebugName = "BackBuffer";
					m_cls_BackBuffer = std::static_pointer_cast<VulkanTexture>(TextureCreate(bd, nullptr));
					ok = (m_cls_BackBuffer != nullptr);
					if (!ok) error = "オフスクリーンバッファの作成に失敗しました";
				}
				else if (!SwapchainCreate(info.i_Width, info.i_Height)) {
					ok = false;
					error = "スワップチェーンの作成に失敗しました";
				}
			}
			if (!ok) {
				if (out_error) *out_error = error;
				return false;
			}
			// コマンドは CommonObjectsCreate で開始済み（ダミーテクスチャのコピーを含む）
			if (m_p_Surface) ImageAcquire();
			return true;
		}

		bool VulkanDevice::InstanceCreate(const DeviceCreateInfo& info, std::string& error)
		{
			uint32_t version = VK_API_VERSION_1_0;
			vkEnumerateInstanceVersion(&version);
			if (version < VK_API_VERSION_1_3) {
				error = "Vulkan 1.3 に対応したドライバーが必要です";
				return false;
			}
			uint32_t ext_count = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
			std::vector<VkExtensionProperties> exts(ext_count);
			vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, exts.data());
			auto has_ext = [&](const char* name) {
				for (auto& e : exts) if (std::strcmp(e.extensionName, name) == 0) return true;
				return false;
			};

			std::vector<const char*> enabled;
			if (!m_b_Headless) {
				enabled.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(_WIN32)
				enabled.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
				error = "このプラットフォームではウィンドウ表示に対応していません（オフスクリーンのみ）";
				return false;
#endif
			}
			if (has_ext(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
				enabled.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				m_b_DebugUtils = true;
			}
			std::vector<const char*> layers;
			if (info.b_Debug) {
				uint32_t layer_count = 0;
				vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
				std::vector<VkLayerProperties> props(layer_count);
				vkEnumerateInstanceLayerProperties(&layer_count, props.data());
				for (auto& l : props) if (std::strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0) layers.push_back("VK_LAYER_KHRONOS_validation");
			}

			VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
			app.pApplicationName = "Pixeon";
			app.pEngineName = "Pixeon Engine";
			app.apiVersion = VK_API_VERSION_1_3;
			VkInstanceCreateInfo ci = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
			ci.pApplicationInfo = &app;
			ci.enabledExtensionCount = (uint32_t)enabled.size();
			ci.ppEnabledExtensionNames = enabled.data();
			ci.enabledLayerCount = (uint32_t)layers.size();
			ci.ppEnabledLayerNames = layers.data();
			VkResult r = vkCreateInstance(&ci, nullptr, &m_cls_Core->p_Instance);
			if (r != VK_SUCCESS) {
				error = std::string("vkCreateInstance に失敗しました: ") + VkResultName(r);
				return false;
			}
			VkInstance inst = m_cls_Core->p_Instance;
			if (m_b_DebugUtils) {
				m_fn_LabelBegin = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(inst, "vkCmdBeginDebugUtilsLabelEXT");
				m_fn_LabelEnd = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(inst, "vkCmdEndDebugUtilsLabelEXT");
				if (!layers.empty()) {
					auto create = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(inst, "vkCreateDebugUtilsMessengerEXT");
					VkDebugUtilsMessengerCreateInfoEXT mi = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
					mi.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
					mi.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
					mi.pfnUserCallback = DebugCallback;
					if (create) create(inst, &mi, nullptr, &m_cls_Core->p_Messenger);
				}
			}
#if defined(_WIN32)
			if (!m_b_Headless) {
				VkWin32SurfaceCreateInfoKHR si = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
				si.hinstance = GetModuleHandleW(nullptr);
				si.hwnd = (HWND)info.p_WindowHandle;
				if (vkCreateWin32SurfaceKHR(inst, &si, nullptr, &m_p_Surface) != VK_SUCCESS) {
					error = "サーフェスの作成に失敗しました";
					return false;
				}
			}
#endif
			return true;
		}

		/*
		* 関数名　PhysicalDeviceSelect
		* 引　数　error：エラー内容
		* 戻り値　成功なら true
		* 説　明　Vulkan 1.3 対応・グラフィックス (+表示) キューを持つ GPU を選ぶ。外部 GPU を優先
		*/
		bool VulkanDevice::PhysicalDeviceSelect(std::string& error)
		{
			uint32_t count = 0;
			vkEnumeratePhysicalDevices(m_cls_Core->p_Instance, &count, nullptr);
			std::vector<VkPhysicalDevice> devices(count);
			vkEnumeratePhysicalDevices(m_cls_Core->p_Instance, &count, devices.data());
			int best_score = -1;
			for (VkPhysicalDevice pd : devices) {
				VkPhysicalDeviceProperties props;
				vkGetPhysicalDeviceProperties(pd, &props);
				if (props.apiVersion < VK_API_VERSION_1_3) continue;
				uint32_t qcount = 0;
				vkGetPhysicalDeviceQueueFamilyProperties(pd, &qcount, nullptr);
				std::vector<VkQueueFamilyProperties> qprops(qcount);
				vkGetPhysicalDeviceQueueFamilyProperties(pd, &qcount, qprops.data());
				for (uint32_t q = 0; q < qcount; ++q) {
					if (!(qprops[q].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
					if (m_p_Surface) {
						VkBool32 present = VK_FALSE;
						vkGetPhysicalDeviceSurfaceSupportKHR(pd, q, m_p_Surface, &present);
						if (!present) continue;
					}
					int score = 1;
					if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score = 3;
					else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score = 2;
					if (score > best_score) {
						best_score = score;
						m_cls_Core->p_Physical = pd;
						m_cls_Core->i_QueueFamily = q;
					}
					break;
				}
			}
			if (!m_cls_Core->p_Physical) {
				error = "Vulkan 1.3 に対応した GPU が見つかりません";
				return false;
			}
			vkGetPhysicalDeviceProperties(m_cls_Core->p_Physical, &m_cls_Core->str_Props);
			vkGetPhysicalDeviceMemoryProperties(m_cls_Core->p_Physical, &m_cls_Core->str_MemProps);
			vkGetPhysicalDeviceFeatures(m_cls_Core->p_Physical, &m_cls_Core->str_Features);

			// D24S8 は一部の GPU で非対応なので D32S8 で代用する
			VkFormatProperties fp;
			vkGetPhysicalDeviceFormatProperties(m_cls_Core->p_Physical, VK_FORMAT_D24_UNORM_S8_UINT, &fp);
			if (!(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) m_cls_Core->e_D24Format = VK_FORMAT_D32_SFLOAT_S8_UINT;
			return true;
		}

		bool VulkanDevice::LogicalDeviceCreate(std::string& error)
		{
			const float priority = 1.0f;
			VkDeviceQueueCreateInfo qi = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
			qi.queueFamilyIndex = m_cls_Core->i_QueueFamily;
			qi.queueCount = 1;
			qi.pQueuePriorities = &priority;

			const VkPhysicalDeviceFeatures& avail = m_cls_Core->str_Features;
			VkPhysicalDeviceFeatures features = {};
			features.samplerAnisotropy = avail.samplerAnisotropy;
			features.depthClamp = avail.depthClamp;
			features.fillModeNonSolid = avail.fillModeNonSolid;
			features.independentBlend = avail.independentBlend;
			features.textureCompressionBC = avail.textureCompressionBC;
			m_cls_Core->str_Features = features;

			VkPhysicalDeviceVulkan12Features f12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
			f12.timelineSemaphore = VK_TRUE;
			VkPhysicalDeviceVulkan13Features f13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
			f13.dynamicRendering = VK_TRUE;
			f13.pNext = &f12;

			std::vector<const char*> exts;
			if (m_p_Surface) exts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

			VkDeviceCreateInfo ci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
			ci.pNext = &f13;
			ci.queueCreateInfoCount = 1;
			ci.pQueueCreateInfos = &qi;
			ci.pEnabledFeatures = &features;
			ci.enabledExtensionCount = (uint32_t)exts.size();
			ci.ppEnabledExtensionNames = exts.data();
			VkResult r = vkCreateDevice(m_cls_Core->p_Physical, &ci, nullptr, &m_cls_Core->p_Device);
			if (r != VK_SUCCESS) {
				error = std::string("vkCreateDevice に失敗しました: ") + VkResultName(r);
				return false;
			}
			vkGetDeviceQueue(m_cls_Core->p_Device, m_cls_Core->i_QueueFamily, 0, &m_cls_Core->p_Queue);

			VkSemaphoreTypeCreateInfo ti = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
			ti.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
			VkSemaphoreCreateInfo si = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			si.pNext = &ti;
			if (vkCreateSemaphore(m_cls_Core->p_Device, &si, nullptr, &m_cls_Core->p_Timeline) != VK_SUCCESS) {
				error = "タイムラインセマフォの作成に失敗しました";
				return false;
			}
			return true;
		}

		/*
		* 関数名　CommonObjectsCreate
		* 引　数　error：エラー内容
		* 戻り値　成功なら true
		* 説　明　ディスクリプタセットレイアウト・パイプラインレイアウト・フレーム資源・ダミーリソースを作る
		*/
		bool VulkanDevice::CommonObjectsCreate(std::string& error)
		{
			VkDevice dev = m_cls_Core->p_Device;
			using namespace ShaderBindings;
			std::vector<VkDescriptorSetLayoutBinding> bindings;
			for (uint32_t s = 0; s < STAGES; ++s) {
				const VkShaderStageFlags flags = (s == 0) ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
				for (uint32_t i = 0; i < CB_SLOTS; ++i)
					bindings.push_back({ VulkanCbBinding((Stage)s, i), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, flags, nullptr });
				for (uint32_t i = 0; i < SRV_SLOTS; ++i)
					bindings.push_back({ VulkanSrvBinding((Stage)s, i), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, flags, nullptr });
				for (uint32_t i = 0; i < SMP_SLOTS; ++i)
					bindings.push_back({ VulkanSamplerBinding((Stage)s, i), VK_DESCRIPTOR_TYPE_SAMPLER, 1, flags, nullptr });
			}
			VkDescriptorSetLayoutCreateInfo li = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			li.bindingCount = (uint32_t)bindings.size();
			li.pBindings = bindings.data();
			VkPipelineLayoutCreateInfo pli = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
			pli.setLayoutCount = 1;
			pli.pSetLayouts = &m_p_SetLayout;
			VkPipelineCacheCreateInfo pci = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
			if (vkCreateDescriptorSetLayout(dev, &li, nullptr, &m_p_SetLayout) != VK_SUCCESS ||
				vkCreatePipelineLayout(dev, &pli, nullptr, &m_p_PipelineLayout) != VK_SUCCESS ||
				vkCreatePipelineCache(dev, &pci, nullptr, &m_p_PipelineCache) != VK_SUCCESS) {
				error = "パイプラインレイアウトの作成に失敗しました";
				return false;
			}

			m_Frames.resize(m_i_FrameCount);
			for (auto& f : m_Frames) {
				VkCommandPoolCreateInfo cpi = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
				cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
				cpi.queueFamilyIndex = m_cls_Core->i_QueueFamily;
				VkSemaphoreCreateInfo si = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
				if (vkCreateCommandPool(dev, &cpi, nullptr, &f.p_Pool) != VK_SUCCESS ||
					vkCreateSemaphore(dev, &si, nullptr, &f.p_Acquire) != VK_SUCCESS) {
					error = "コマンドプールの作成に失敗しました";
					return false;
				}
				VkCommandBufferAllocateInfo ai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
				ai.commandPool = f.p_Pool;
				ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				ai.commandBufferCount = 1;
				if (vkAllocateCommandBuffers(dev, &ai, &f.p_Cmd) != VK_SUCCESS) {
					error = "コマンドバッファの確保に失敗しました";
					return false;
				}
			}

			// ダミー UBO（ゼロ埋め、ホスト可視）
			const VkDeviceSize ubo_size = std::min<VkDeviceSize>(DUMMY_UBO_SIZE, m_cls_Core->str_Props.limits.maxUniformBufferRange);
			VkBufferCreateInfo bi = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			bi.size = ubo_size;
			bi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			if (vkCreateBuffer(dev, &bi, nullptr, &m_p_DummyUbo) != VK_SUCCESS) { error = "バッファの作成に失敗しました"; return false; }
			VkMemoryRequirements req;
			vkGetBufferMemoryRequirements(dev, m_p_DummyUbo, &req);
			VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			mai.allocationSize = req.size;
			mai.memoryTypeIndex = (uint32_t)m_cls_Core->MemoryTypeFind(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			if (vkAllocateMemory(dev, &mai, nullptr, &m_p_DummyUboMemory) != VK_SUCCESS) { error = "メモリの確保に失敗しました"; return false; }
			vkBindBufferMemory(dev, m_p_DummyUbo, m_p_DummyUboMemory, 0);
			void* mapped = nullptr;
			if (vkMapMemory(dev, m_p_DummyUboMemory, 0, VK_WHOLE_SIZE, 0, &mapped) == VK_SUCCESS) {
				std::memset(mapped, 0, (size_t)ubo_size);
				vkUnmapMemory(dev, m_p_DummyUboMemory);
			}

			VkSamplerCreateInfo smi = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
			smi.magFilter = smi.minFilter = VK_FILTER_LINEAR;
			smi.addressModeU = smi.addressModeV = smi.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			smi.maxLod = VK_LOD_CLAMP_NONE;
			if (vkCreateSampler(dev, &smi, nullptr, &m_p_NullSampler) != VK_SUCCESS) { error = "サンプラーの作成に失敗しました"; return false; }

			// ダミーテクスチャ（作成時のコピーを記録するためにコマンドを開始しておく）
			CommandsBegin();
			TextureDesc td;
			td.c_DebugName = "Null";
			const uint32_t zero = 0;
			SubresourceData init{ &zero, 4, 4 };
			m_cls_NullTexture = std::static_pointer_cast<VulkanTexture>(TextureCreate(td, &init));
			if (!m_cls_NullTexture) { error = "ダミーテクスチャの作成に失敗しました"; return false; }
			VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			vi.image = m_cls_NullTexture->m_p_Image;
			vi.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			vi.format = m_cls_NullTexture->m_e_VkFormat;
			vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			if (vkCreateImageView(dev, &vi, nullptr, &m_p_NullArrayView) != VK_SUCCESS) { error = "ダミービューの作成に失敗しました"; return false; }
			return true;
		}

		/*
		* 関数名　SwapchainCreate
		* 引　数　width / height：希望サイズ
		* 戻り値　成功なら true
		* 説　明　スワップチェーンを作り直し、代理のバックバッファテクスチャを更新する
		*/
		bool VulkanDevice::SwapchainCreate(uint32_t width, uint32_t height)
		{
			VkPhysicalDevice pd = m_cls_Core->p_Physical;
			VkDevice dev = m_cls_Core->p_Device;
			VkSurfaceCapabilitiesKHR caps;
			if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, m_p_Surface, &caps) != VK_SUCCESS) return false;

			uint32_t fcount = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(pd, m_p_Surface, &fcount, nullptr);
			std::vector<VkSurfaceFormatKHR> formats(fcount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(pd, m_p_Surface, &fcount, formats.data());
			VkSurfaceFormatKHR chosen = formats.empty() ? VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR } : formats[0];
			for (const auto& f : formats) if (f.format == VK_FORMAT_R8G8B8A8_UNORM) { chosen = f; break; }
			if (chosen.format != VK_FORMAT_R8G8B8A8_UNORM)
				for (const auto& f : formats) if (f.format == VK_FORMAT_B8G8R8A8_UNORM) { chosen = f; break; }

			uint32_t mcount = 0;
			vkGetPhysicalDeviceSurfacePresentModesKHR(pd, m_p_Surface, &mcount, nullptr);
			std::vector<VkPresentModeKHR> modes(mcount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(pd, m_p_Surface, &mcount, modes.data());
			VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
			if (!m_b_VSync) {
				for (auto m : modes) if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) mode = m;
				for (auto m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR) mode = m;
			}

			VkExtent2D extent = caps.currentExtent;
			if (extent.width == UINT32_MAX) {
				extent.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
				extent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
			}
			if (extent.width == 0 || extent.height == 0) return false;   // 最小化中
			uint32_t image_count = std::max(caps.minImageCount, m_i_FrameCount);
			if (caps.maxImageCount > 0) image_count = std::min(image_count, caps.maxImageCount);

			VkSwapchainCreateInfoKHR ci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
			ci.surface = m_p_Surface;
			ci.minImageCount = image_count;
			ci.imageFormat = chosen.format;
			ci.imageColorSpace = chosen.colorSpace;
			ci.imageExtent = extent;
			ci.imageArrayLayers = 1;
			ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
				(caps.supportedUsageFlags & (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
			ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			ci.preTransform = caps.currentTransform;
			ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
			ci.presentMode = mode;
			ci.clipped = VK_TRUE;
			ci.oldSwapchain = m_p_Swapchain;
			VkSwapchainKHR swapchain = VK_NULL_HANDLE;
			VkResult r = vkCreateSwapchainKHR(dev, &ci, nullptr, &swapchain);
			SwapchainDestroy();
			if (r != VK_SUCCESS) {
				LogWrite("Vulkan: vkCreateSwapchainKHR 失敗 %s", VkResultName(r));
				return false;
			}
			m_p_Swapchain = swapchain;
			m_e_SwapFormat = chosen.format;

			uint32_t count = 0;
			vkGetSwapchainImagesKHR(dev, m_p_Swapchain, &count, nullptr);
			m_SwapImages.resize(count);
			vkGetSwapchainImagesKHR(dev, m_p_Swapchain, &count, m_SwapImages.data());
			for (VkImage img : m_SwapImages) {
				VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
				vi.image = img;
				vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
				vi.format = chosen.format;
				vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
				VkImageView view = VK_NULL_HANDLE;
				vkCreateImageView(dev, &vi, nullptr, &view);
				m_SwapViews.push_back(view);
				VkSemaphoreCreateInfo si = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
				VkSemaphore sem = VK_NULL_HANDLE;
				vkCreateSemaphore(dev, &si, nullptr, &sem);
				m_SwapRenderDone.push_back(sem);
			}

			if (!m_cls_BackBuffer) {
				TextureDesc bd;
				bd.i_BindFlags = Bind_RenderTarget;
				m_cls_BackBuffer = std::make_shared<VulkanTexture>(bd, m_cls_Core);
				m_cls_BackBuffer->m_b_External = true;
			}
			m_cls_BackBuffer->SizeSet(extent.width, extent.height);
			m_cls_BackBuffer->FormatSet(chosen.format == VK_FORMAT_B8G8R8A8_UNORM ? Format::B8G8R8A8_UNorm : Format::R8G8B8A8_UNorm);
			m_cls_BackBuffer->m_e_VkFormat = chosen.format;
			m_cls_BackBuffer->m_p_Image = VK_NULL_HANDLE;
			m_cls_BackBuffer->m_p_AttachView = VK_NULL_HANDLE;
			m_b_ImageAcquired = false;
			m_b_SwapchainDirty = false;
			return true;
		}

		void VulkanDevice::SwapchainDestroy()
		{
			VkDevice dev = m_cls_Core->p_Device;
			if (!m_SwapViews.empty() || m_p_Swapchain) vkDeviceWaitIdle(dev);
			for (VkImageView v : m_SwapViews) vkDestroyImageView(dev, v, nullptr);
			for (VkSemaphore s : m_SwapRenderDone) vkDestroySemaphore(dev, s, nullptr);
			m_SwapViews.clear();
			m_SwapRenderDone.clear();
			m_SwapImages.clear();
			if (m_p_Swapchain) vkDestroySwapchainKHR(dev, m_p_Swapchain, nullptr);
			m_p_Swapchain = VK_NULL_HANDLE;
		}

		/*
		* 関数名　ImageAcquire
		* 引　数　なし
		* 戻り値　画像を取得できたら true
		* 説　明　次に描くスワップチェーン画像を取得し、代理テクスチャをその画像に向ける
		*/
		bool VulkanDevice::ImageAcquire()
		{
			if (!m_p_Swapchain || m_b_ImageAcquired) return m_b_ImageAcquired;
			FrameContext& f = m_Frames[m_i_FrameSlot];
			VkResult r = vkAcquireNextImageKHR(m_cls_Core->p_Device, m_p_Swapchain, UINT64_MAX, f.p_Acquire, VK_NULL_HANDLE, &m_i_ImageIndex);
			if (r == VK_ERROR_OUT_OF_DATE_KHR) {
				m_b_SwapchainDirty = true;
				return false;
			}
			if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) return false;
			m_b_ImageAcquired = true;
			m_b_AcquirePending = true;
			m_cls_BackBuffer->m_p_Image = m_SwapImages[m_i_ImageIndex];
			m_cls_BackBuffer->m_p_AttachView = m_SwapViews[m_i_ImageIndex];
			m_cls_BackBuffer->m_e_Layout = VK_IMAGE_LAYOUT_UNDEFINED;   // フリップ後の内容は破棄（D3D の FLIP_DISCARD と同じ）
			return true;
		}

		VulkanDevice::~VulkanDevice()
		{
			if (!m_cls_Core || !m_cls_Core->p_Device) return;
			m_cls_Core->p_Owner = nullptr;
			VkDevice dev = m_cls_Core->p_Device;
			if (m_b_Recording) {
				RenderingEnd();
				vkEndCommandBuffer(m_p_Cmd);
				m_b_Recording = false;
			}
			vkDeviceWaitIdle(dev);
			m_cls_BackBuffer.reset();
			m_cls_NullTexture.reset();
			SwapchainDestroy();
			if (m_p_Surface) vkDestroySurfaceKHR(m_cls_Core->p_Instance, m_p_Surface, nullptr);
			for (auto& p : m_Pipelines) if (p.second.p_Pipeline) vkDestroyPipeline(dev, p.second.p_Pipeline, nullptr);
			for (auto& f : m_Frames) {
				for (auto& page : f.uploadPages) { vkDestroyBuffer(dev, page.p_Buffer, nullptr); vkFreeMemory(dev, page.p_Memory, nullptr); }
				for (auto pool : f.descriptorPools) vkDestroyDescriptorPool(dev, pool, nullptr);
				if (f.p_Acquire) vkDestroySemaphore(dev, f.p_Acquire, nullptr);
				if (f.p_Pool) vkDestroyCommandPool(dev, f.p_Pool, nullptr);
			}
			for (auto& page : m_FreePages) { vkDestroyBuffer(dev, page.p_Buffer, nullptr); vkFreeMemory(dev, page.p_Memory, nullptr); }
			if (m_p_NullArrayView) vkDestroyImageView(dev, m_p_NullArrayView, nullptr);
			if (m_p_NullSampler) vkDestroySampler(dev, m_p_NullSampler, nullptr);
			if (m_p_DummyUbo) vkDestroyBuffer(dev, m_p_DummyUbo, nullptr);
			if (m_p_DummyUboMemory) vkFreeMemory(dev, m_p_DummyUboMemory, nullptr);
			if (m_p_PipelineCache) vkDestroyPipelineCache(dev, m_p_PipelineCache, nullptr);
			if (m_p_PipelineLayout) vkDestroyPipelineLayout(dev, m_p_PipelineLayout, nullptr);
			if (m_p_SetLayout) vkDestroyDescriptorSetLayout(dev, m_p_SetLayout, nullptr);
			m_cls_Core->Collect();
			// VkDevice 自体は残っているリソースがすべて破棄された後に VulkanCore が破棄する
		}

		NativeHandles VulkanDevice::NativeHandlesGet() const
		{
			NativeHandles h;
			h.p_Device = m_cls_Core->p_Device;
			h.p_Context = m_p_Cmd;
			h.p_Queue = m_cls_Core->p_Queue;
			h.p_PhysicalDevice = m_cls_Core->p_Physical;
			h.p_Instance = m_cls_Core->p_Instance;
			return h;
		}

		// ================================================================
		// コマンド・フレーム
		// ================================================================

		void VulkanDevice::CommandsBegin()
		{
			if (m_b_Recording) return;
			m_p_Cmd = m_Frames[m_i_FrameSlot].p_Cmd;
			vkResetCommandBuffer(m_p_Cmd, 0);
			VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			vkBeginCommandBuffer(m_p_Cmd, &bi);
			m_b_Recording = true;
			AllDirtyMark();
		}

		/*
		* 関数名　CommandsSubmit
		* 引　数　present_signal：表示用のセマフォも Signal するか
		* 戻り値　なし
		* 説　明　記録したコマンドを提出し、タイムラインセマフォに完了値を書く
		*/
		void VulkanDevice::CommandsSubmit(bool present_signal)
		{
			if (!m_b_Recording) CommandsBegin();
			RenderingEnd();
			BarriersFlush();
			vkEndCommandBuffer(m_p_Cmd);
			m_b_Recording = false;

			const uint64_t value = m_cls_Core->i_NextValue++;
			VkSemaphore signals[2] = { m_cls_Core->p_Timeline, VK_NULL_HANDLE };
			uint64_t signal_values[2] = { value, 0 };
			uint32_t signal_count = 1;
			if (present_signal) signals[signal_count++] = m_SwapRenderDone[m_i_ImageIndex];
			VkSemaphore wait = m_Frames[m_i_FrameSlot].p_Acquire;
			const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			const uint64_t wait_value = 0;

			VkTimelineSemaphoreSubmitInfo ti = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
			ti.signalSemaphoreValueCount = signal_count;
			ti.pSignalSemaphoreValues = signal_values;
			ti.waitSemaphoreValueCount = m_b_AcquirePending ? 1 : 0;
			ti.pWaitSemaphoreValues = &wait_value;
			VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
			si.pNext = &ti;
			si.waitSemaphoreCount = m_b_AcquirePending ? 1 : 0;
			si.pWaitSemaphores = &wait;
			si.pWaitDstStageMask = &wait_stage;
			si.commandBufferCount = 1;
			si.pCommandBuffers = &m_p_Cmd;
			si.signalSemaphoreCount = signal_count;
			si.pSignalSemaphores = signals;
			VkResult r = vkQueueSubmit(m_cls_Core->p_Queue, 1, &si, VK_NULL_HANDLE);
			if (r != VK_SUCCESS) LogWrite("Vulkan: vkQueueSubmit 失敗 %s", VkResultName(r));
			m_b_AcquirePending = false;
			m_Frames[m_i_FrameSlot].i_Value = value;
		}

		/*
		* 関数名　FrameBegin
		* 引　数　なし
		* 戻り値　なし
		* 説　明　このスロットを前回使ったフレームの完了を待ち、プール・アップロードページを再利用する
		*/
		void VulkanDevice::FrameBegin()
		{
			FrameContext& f = m_Frames[m_i_FrameSlot];
			m_cls_Core->Wait(f.i_Value);
			VkDevice dev = m_cls_Core->p_Device;
			for (auto& page : f.uploadPages) {
				if (page.i_Size == UPLOAD_PAGE_SIZE) {
					page.i_Offset = 0;
					m_FreePages.push_back(page);
				}
				else {
					vkDestroyBuffer(dev, page.p_Buffer, nullptr);
					vkFreeMemory(dev, page.p_Memory, nullptr);
				}
			}
			f.uploadPages.clear();
			for (auto pool : f.descriptorPools) vkResetDescriptorPool(dev, pool, 0);
			f.i_PoolIndex = 0;
			m_cls_Core->Collect();
			CommandsBegin();

			if (m_p_Surface) {
				if (m_b_SwapchainDirty) SwapchainCreate(m_cls_BackBuffer->GetWidth(), m_cls_BackBuffer->GetHeight());
				ImageAcquire();
			}
		}

		void VulkanDevice::Present()
		{
			VK_LOCK;
			const bool presenting = m_p_Swapchain && m_b_ImageAcquired;
			if (presenting) ImageTransition(m_cls_BackBuffer.get(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			CommandsSubmit(presenting);
			if (presenting) {
				VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
				pi.waitSemaphoreCount = 1;
				pi.pWaitSemaphores = &m_SwapRenderDone[m_i_ImageIndex];
				pi.swapchainCount = 1;
				pi.pSwapchains = &m_p_Swapchain;
				pi.pImageIndices = &m_i_ImageIndex;
				VkResult r = vkQueuePresentKHR(m_cls_Core->p_Queue, &pi);
				if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) m_b_SwapchainDirty = true;
				m_b_ImageAcquired = false;
			}
			++m_i_Frame;
			m_i_FrameSlot = (m_i_FrameSlot + 1) % m_i_FrameCount;
			FrameBegin();
		}

		/*
		* 関数名　IdleWait
		* 引　数　なし
		* 戻り値　なし
		* 説　明　記録済みのコマンドを提出して GPU の完了を待ち、続けて記録できる状態に戻す
		*/
		void VulkanDevice::IdleWait()
		{
			VK_LOCK;
			CommandsSubmit(false);
			m_cls_Core->Wait(m_Frames[m_i_FrameSlot].i_Value);
			m_cls_Core->Collect();
			CommandsBegin();
		}

		void VulkanDevice::SwapChainResize(uint32_t width, uint32_t height)
		{
			VK_LOCK;
			if (width == 0 || height == 0) return;
			IdleWait();
			m_str_RenderTargets = RenderTargetState{};
			m_b_RenderTargetsDirty = true;
			if (m_b_Headless) {
				TextureDesc bd = m_cls_BackBuffer->DescGet();
				bd.i_Width = width;
				bd.i_Height = height;
				m_cls_BackBuffer = std::static_pointer_cast<VulkanTexture>(TextureCreate(bd, nullptr));
				return;
			}
			// 取得済みの画像は描かずに作り直す（取得待ちのセマフォは空の提出で消費する）
			if (m_b_AcquirePending) IdleWait();
			if (SwapchainCreate(width, height)) ImageAcquire();
		}

		void VulkanDevice::VSyncSet(bool enable)
		{
			if (m_b_VSync == enable) return;
			m_b_VSync = enable;
			m_b_SwapchainDirty = true;
		}

		void VulkanDevice::AllDirtyMark()
		{
			m_b_RenderTargetsDirty = m_b_ViewportDirty = m_b_ScissorDirty = m_b_DynamicDirty = true;
			m_b_DescriptorValid = false;
			m_p_CurrentPipeline = nullptr;
			m_b_Rendering = false;
		}

		// ================================================================
		// アップロード
		// ================================================================

		VulkanDevice::UploadAlloc VulkanDevice::UploadAllocate(VkDeviceSize size, VkDeviceSize alignment)
		{
			FrameContext& f = m_Frames[m_i_FrameSlot];
			auto try_page = [&](UploadPage& page, UploadAlloc& out) {
				const VkDeviceSize offset = (page.i_Offset + alignment - 1) / alignment * alignment;
				if (offset + size > page.i_Size) return false;
				page.i_Offset = offset + size;
				out.p_Buffer = page.p_Buffer;
				out.i_Offset = offset;
				out.p_Cpu = page.p_Cpu + offset;
				return true;
			};
			UploadAlloc out;
			if (!f.uploadPages.empty() && try_page(f.uploadPages.back(), out)) return out;

			UploadPage page;
			if (size + alignment <= UPLOAD_PAGE_SIZE && !m_FreePages.empty()) {
				page = m_FreePages.back();
				m_FreePages.pop_back();
			}
			else {
				VkDevice dev = m_cls_Core->p_Device;
				page.i_Size = std::max<VkDeviceSize>(UPLOAD_PAGE_SIZE, (size + alignment + 0xFFFF) & ~(VkDeviceSize)0xFFFF);
				VkBufferCreateInfo bi = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
				bi.size = page.i_Size;
				bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
				if (vkCreateBuffer(dev, &bi, nullptr, &page.p_Buffer) != VK_SUCCESS) return out;
				VkMemoryRequirements req;
				vkGetBufferMemoryRequirements(dev, page.p_Buffer, &req);
				VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
				ai.allocationSize = req.size;
				const int32_t type = m_cls_Core->MemoryTypeFind(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				if (type < 0) { vkDestroyBuffer(dev, page.p_Buffer, nullptr); return out; }
				ai.memoryTypeIndex = (uint32_t)type;
				if (vkAllocateMemory(dev, &ai, nullptr, &page.p_Memory) != VK_SUCCESS) {
					LogWrite("Vulkan: アップロードメモリの確保に失敗 (%llu bytes)", (unsigned long long)page.i_Size);
					vkDestroyBuffer(dev, page.p_Buffer, nullptr);
					return out;
				}
				vkBindBufferMemory(dev, page.p_Buffer, page.p_Memory, 0);
				void* cpu = nullptr;
				vkMapMemory(dev, page.p_Memory, 0, VK_WHOLE_SIZE, 0, &cpu);
				page.p_Cpu = (uint8_t*)cpu;
			}
			f.uploadPages.push_back(page);
			try_page(f.uploadPages.back(), out);
			return out;
		}

		void VulkanDevice::StreamedBufferResolve(VulkanBuffer* b, VkBuffer& out_buffer, VkDeviceSize& out_offset)
		{
			if (b->m_b_Dirty || b->m_i_UploadFrame != m_i_Frame || !b->m_p_StreamBuffer) {
				const VkDeviceSize align = (b->DescGet().i_BindFlags & Bind_ConstantBuffer)
					? std::max<VkDeviceSize>(16, m_cls_Core->str_Props.limits.minUniformBufferOffsetAlignment) : 16;
				UploadAlloc a = UploadAllocate(b->m_Shadow.size(), align);
				if (a.p_Cpu) {
					std::memcpy(a.p_Cpu, b->m_Shadow.data(), b->m_Shadow.size());
					b->m_p_StreamBuffer = a.p_Buffer;
					b->m_i_StreamOffset = a.i_Offset;
					b->m_i_UploadFrame = m_i_Frame;
					b->m_b_Dirty = false;
				}
			}
			out_buffer = b->m_p_StreamBuffer;
			out_offset = b->m_i_StreamOffset;
		}

		// ================================================================
		// 状態遷移
		// ================================================================

		void VulkanDevice::RenderingEnd()
		{
			if (!m_b_Rendering) return;
			vkCmdEndRendering(m_p_Cmd);
			m_b_Rendering = false;
			m_b_RenderTargetsDirty = true;
		}

		void VulkanDevice::ImageTransition(VulkanTexture* t, VkImageLayout layout)
		{
			if (!t || !t->m_p_Image || t->m_e_Layout == layout) return;
			VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
			b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
			b.oldLayout = t->m_e_Layout;
			b.newLayout = layout;
			b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			b.image = t->m_p_Image;
			b.subresourceRange = { t->m_i_Aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
			m_Barriers.push_back(b);
			t->m_e_Layout = layout;
		}

		void VulkanDevice::BarriersFlush()
		{
			if (m_Barriers.empty()) return;
			RenderingEnd();
			vkCmdPipelineBarrier(m_p_Cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
				0, nullptr, 0, nullptr, (uint32_t)m_Barriers.size(), m_Barriers.data());
			m_Barriers.clear();
		}

		// 転送の前後のメモリ依存（前: 先行する読み取りの完了待ち、後: 書き込みを後続の読み取りへ見せる）
		void VulkanDevice::TransferBarrier(bool before)
		{
			VkMemoryBarrier mb = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
			mb.srcAccessMask = before ? VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT : VK_ACCESS_TRANSFER_WRITE_BIT;
			mb.dstAccessMask = before ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
			vkCmdPipelineBarrier(m_p_Cmd, before ? VK_PIPELINE_STAGE_ALL_COMMANDS_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT,
				before ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
		}

		// ================================================================
		// リソース生成
		// ================================================================

		BufferRef VulkanDevice::BufferCreate(const BufferDesc& desc, const void* initial_data)
		{
			VK_LOCK;
			auto buf = std::make_shared<VulkanBuffer>(desc, m_cls_Core);
			const bool streamed = (desc.i_BindFlags & Bind_ConstantBuffer) || desc.e_Usage == Usage::Dynamic;
			if (streamed) {
				const uint32_t size = (desc.i_BindFlags & Bind_ConstantBuffer) ? ((desc.i_Size + 15) & ~15u) : desc.i_Size;
				buf->m_Shadow.assign(std::max<uint32_t>(size, 16), 0);
				if (initial_data) std::memcpy(buf->m_Shadow.data(), initial_data, desc.i_Size);
				return buf;
			}
			VkDevice dev = m_cls_Core->p_Device;
			VkBufferCreateInfo bi = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			bi.size = std::max<uint32_t>(desc.i_Size, 16);
			bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			if (desc.i_BindFlags & Bind_VertexBuffer) bi.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			if (desc.i_BindFlags & Bind_IndexBuffer) bi.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			if (vkCreateBuffer(dev, &bi, nullptr, &buf->m_p_Buffer) != VK_SUCCESS) return nullptr;
			VkMemoryRequirements req;
			vkGetBufferMemoryRequirements(dev, buf->m_p_Buffer, &req);
			VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			ai.allocationSize = req.size;
			int32_t type = m_cls_Core->MemoryTypeFind(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			if (type < 0) type = m_cls_Core->MemoryTypeFind(req.memoryTypeBits, 0);
			ai.memoryTypeIndex = (uint32_t)type;
			if (vkAllocateMemory(dev, &ai, nullptr, &buf->m_p_Memory) != VK_SUCCESS) {
				LogWrite("Vulkan: バッファメモリの確保に失敗 (size=%u)", desc.i_Size);
				return nullptr;
			}
			vkBindBufferMemory(dev, buf->m_p_Buffer, buf->m_p_Memory, 0);
			if (initial_data) BufferUpdate(buf.get(), initial_data, desc.i_Size, 0);
			return buf;
		}

		bool VulkanDevice::ImageCreate(VulkanTexture* tex)
		{
			VkDevice dev = m_cls_Core->p_Device;
			const TextureDesc& d = tex->DescGet();
			tex->m_e_VkFormat = m_cls_Core->FormatGet(d.e_Format);
			tex->m_i_Aspect = AspectGet(d.e_Format);
			if (tex->m_e_VkFormat == VK_FORMAT_UNDEFINED) return false;

			VkImageCreateInfo ii = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			ii.imageType = VK_IMAGE_TYPE_2D;
			ii.format = tex->m_e_VkFormat;
			ii.extent = { d.i_Width, d.i_Height, 1 };
			ii.mipLevels = d.i_MipLevels;
			ii.arrayLayers = d.i_ArraySize;
			ii.samples = VK_SAMPLE_COUNT_1_BIT;
			ii.tiling = VK_IMAGE_TILING_OPTIMAL;
			ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			if (d.i_BindFlags & Bind_ShaderResource) ii.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
			if (d.i_BindFlags & Bind_RenderTarget) ii.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			if (d.i_BindFlags & Bind_DepthStencil) ii.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			if (vkCreateImage(dev, &ii, nullptr, &tex->m_p_Image) != VK_SUCCESS) return false;

			VkMemoryRequirements req;
			vkGetImageMemoryRequirements(dev, tex->m_p_Image, &req);
			VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			ai.allocationSize = req.size;
			int32_t type = m_cls_Core->MemoryTypeFind(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			if (type < 0) type = m_cls_Core->MemoryTypeFind(req.memoryTypeBits, 0);
			ai.memoryTypeIndex = (uint32_t)type;
			if (vkAllocateMemory(dev, &ai, nullptr, &tex->m_p_Memory) != VK_SUCCESS) return false;
			vkBindImageMemory(dev, tex->m_p_Image, tex->m_p_Memory, 0);

			if (d.i_BindFlags & Bind_ShaderResource) {
				VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
				vi.image = tex->m_p_Image;
				vi.viewType = (d.i_ArraySize > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
				vi.format = tex->m_e_VkFormat;
				// 深度テクスチャをシェーダーで読むときはデプス面だけ（D3D の R24_UNORM_X8 / R32_FLOAT と同じ）
				const VkImageAspectFlags aspect = FormatInfoGet(d.e_Format).b_Depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
				vi.subresourceRange = { aspect, 0, d.i_MipLevels, 0, d.i_ArraySize };
				if (vkCreateImageView(dev, &vi, nullptr, &tex->m_p_SrvView) != VK_SUCCESS) return false;
			}
			if (d.i_BindFlags & (Bind_RenderTarget | Bind_DepthStencil)) {
				VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
				vi.image = tex->m_p_Image;
				vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
				vi.format = tex->m_e_VkFormat;
				vi.subresourceRange = { tex->m_i_Aspect, 0, 1, 0, 1 };
				if (vkCreateImageView(dev, &vi, nullptr, &tex->m_p_AttachView) != VK_SUCCESS) return false;
			}
			return true;
		}

		/*
		* 関数名　TextureCreate
		* 引　数　desc：記述　initial_data：初期データ（D3D11 のサブリソース順）
		* 戻り値　テクスチャ
		* 説　明　画像を作り、初期データがあればアップロードページからコピーしてシェーダー読み取り状態にする
		*/
		TextureRef VulkanDevice::TextureCreate(const TextureDesc& desc, const SubresourceData* initial_data)
		{
			VK_LOCK;
			auto tex = std::make_shared<VulkanTexture>(desc, m_cls_Core);
			if (!ImageCreate(tex.get())) {
				LogWrite("Vulkan: テクスチャ作成失敗 (%ux%u %s)", desc.i_Width, desc.i_Height, FormatNameGet(desc.e_Format));
				return nullptr;
			}
			if (initial_data) {
				CommandsBegin();
				ImageTransition(tex.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
				BarriersFlush();
				RenderingEnd();
				for (uint32_t slice = 0; slice < desc.i_ArraySize; ++slice) {
					for (uint32_t mip = 0; mip < desc.i_MipLevels; ++mip) {
						const SubresourceData& src = initial_data[slice * desc.i_MipLevels + mip];
						if (!src.p_Data) continue;
						const uint32_t w = std::max(1u, desc.i_Width >> mip);
						const uint32_t h = std::max(1u, desc.i_Height >> mip);
						const uint32_t row = FormatRowPitchGet(desc.e_Format, w);
						const uint32_t rows = FormatRowCountGet(desc.e_Format, h);
						const uint32_t pitch = src.i_RowPitch ? src.i_RowPitch : row;
						UploadAlloc a = UploadAllocate((VkDeviceSize)row * rows, 16);
						if (!a.p_Cpu) break;
						for (uint32_t y = 0; y < rows; ++y)
							std::memcpy(a.p_Cpu + (size_t)y * row, (const uint8_t*)src.p_Data + (size_t)y * pitch, row);
						VkBufferImageCopy c = {};
						c.bufferOffset = a.i_Offset;
						c.imageSubresource = { (VkImageAspectFlags)(tex->m_i_Aspect & ~(VkImageAspectFlags)VK_IMAGE_ASPECT_STENCIL_BIT), mip, slice, 1 };
						c.imageExtent = { w, h, 1 };
						vkCmdCopyBufferToImage(m_p_Cmd, a.p_Buffer, tex->m_p_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c);
					}
				}
				ImageTransition(tex.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}
			return tex;
		}

		SamplerRef VulkanDevice::SamplerCreate(const SamplerDesc& desc)
		{
			VK_LOCK;
			auto s = std::make_shared<VulkanSampler>(desc, m_cls_Core);
			VkSamplerCreateInfo si = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
			VkFilter mag = VK_FILTER_LINEAR, min = VK_FILTER_LINEAR;
			VkSamplerMipmapMode mip = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			switch (desc.e_Filter) {
			case Filter::Point:
			case Filter::ComparisonPoint:
				mag = min = VK_FILTER_NEAREST; mip = VK_SAMPLER_MIPMAP_MODE_NEAREST; break;
			case Filter::MinMagLinearMipPoint:
			case Filter::ComparisonMinMagLinearMipPoint:
				mip = VK_SAMPLER_MIPMAP_MODE_NEAREST; break;
			default: break;
			}
			si.magFilter = mag;
			si.minFilter = min;
			si.mipmapMode = mip;
			si.addressModeU = AddressGet(desc.e_AddressU);
			si.addressModeV = AddressGet(desc.e_AddressV);
			si.addressModeW = AddressGet(desc.e_AddressW);
			si.mipLodBias = desc.f_MipLodBias;
			if (desc.e_Filter == Filter::Anisotropic && m_cls_Core->str_Features.samplerAnisotropy) {
				si.anisotropyEnable = VK_TRUE;
				si.maxAnisotropy = std::min((float)desc.i_MaxAnisotropy, m_cls_Core->str_Props.limits.maxSamplerAnisotropy);
			}
			const bool compare = desc.e_Filter == Filter::ComparisonPoint || desc.e_Filter == Filter::ComparisonLinear ||
				desc.e_Filter == Filter::ComparisonMinMagLinearMipPoint;
			si.compareEnable = compare ? VK_TRUE : VK_FALSE;
			si.compareOp = CompareGet(desc.e_Compare);
			si.minLod = desc.f_MinLod;
			si.maxLod = desc.f_MaxLod >= 1000.0f ? VK_LOD_CLAMP_NONE : desc.f_MaxLod;
			// Vulkan の境界色は 3 種類しかないので近いものを選ぶ
			const float* bc = desc.f_BorderColor;
			if (bc[0] >= 0.5f && bc[1] >= 0.5f && bc[2] >= 0.5f) si.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			else if (bc[3] >= 0.5f) si.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
			else si.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
			if (vkCreateSampler(m_cls_Core->p_Device, &si, nullptr, &s->m_p_Sampler) != VK_SUCCESS) return nullptr;
			return s;
		}

		ShaderRef VulkanDevice::ShaderCreate(ShaderStage stage, const void* bytecode, size_t size, const char* name)
		{
			VK_LOCK;
			if (!bytecode || size < 20 || (size % 4) != 0 || *(const uint32_t*)bytecode != 0x07230203u) {
				LogWrite("Vulkan: SPIR-V ではないシェーダーです %s", name ? name : "");
				return nullptr;
			}
			auto sh = std::make_shared<VulkanShader>(stage, bytecode, size, name, m_cls_Core);
			if (!sh->m_b_Valid) {
				LogWrite("Vulkan: SPIR-V の解析に失敗 %s", name ? name : "");
				return nullptr;
			}
			if (stage == ShaderStage::Vertex) {
				const std::vector<uint32_t> code = SpirvSemanticsStrip(sh->m_Words);
				VkShaderModuleCreateInfo mi = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
				mi.codeSize = code.size() * 4;
				mi.pCode = code.data();
				if (vkCreateShaderModule(m_cls_Core->p_Device, &mi, nullptr, &sh->m_p_Module) != VK_SUCCESS) return nullptr;
			}
			return sh;
		}

		InputLayoutRef VulkanDevice::InputLayoutCreate(const InputElement* elements, uint32_t count, const Shader* vs)
		{
			if (!vs) return nullptr;
			auto layout = std::make_shared<VulkanInputLayout>(elements, count);
			// D3D11 と同じく、シェーダーが要求する入力が揃っているか確認する
			for (const auto& in : vs->ReflectionGet().inputs) {
				bool found = false;
				for (const auto& e : layout->ElementsGet())
					if (UpperGet(e.c_Semantic) == in.semantic && e.i_SemanticIndex == in.i_SemanticIndex) { found = true; break; }
				if (!found) {
					LogWrite("Vulkan: 頂点レイアウトに %s%u がありません (%s)", in.semantic.c_str(), in.i_SemanticIndex, vs->NameGet().c_str());
					return nullptr;
				}
			}
			return layout;
		}

		// ================================================================
		// 更新
		// ================================================================

		void VulkanDevice::BufferUpdate(Buffer* buffer, const void* data, uint32_t size, uint32_t offset)
		{
			VK_LOCK;
			VulkanBuffer* b = BufCast(buffer);
			if (!b || !data) return;
			if (b->IsStreamed()) {
				const uint32_t n = (offset < b->m_Shadow.size()) ? std::min<uint32_t>(size, (uint32_t)b->m_Shadow.size() - offset) : 0;
				std::memcpy(b->m_Shadow.data() + offset, data, n);
				b->m_b_Dirty = true;
				return;
			}
			if (offset >= b->GetSize()) return;
			size = std::min(size, b->GetSize() - offset);
			CommandsBegin();
			UploadAlloc a = UploadAllocate(size, 16);
			if (!a.p_Cpu) return;
			std::memcpy(a.p_Cpu, data, size);
			BarriersFlush();
			RenderingEnd();
			TransferBarrier(true);
			VkBufferCopy c = { a.i_Offset, offset, size };
			vkCmdCopyBuffer(m_p_Cmd, a.p_Buffer, b->m_p_Buffer, 1, &c);
			TransferBarrier(false);
		}

		void* VulkanDevice::BufferMap(Buffer* buffer)
		{
			VulkanBuffer* b = BufCast(buffer);
			return (b && b->IsStreamed()) ? b->m_Shadow.data() : nullptr;
		}

		void VulkanDevice::BufferUnmap(Buffer* buffer)
		{
			if (VulkanBuffer* b = BufCast(buffer)) b->m_b_Dirty = true;
		}

		void VulkanDevice::TextureUpdate(Texture* texture, uint32_t mip, const void* data, uint32_t row_pitch)
		{
			VK_LOCK;
			VulkanTexture* t = TexCast(texture);
			if (!t || !data || !t->m_p_Image || mip >= t->DescGet().i_MipLevels) return;
			CommandsBegin();
			const uint32_t w = std::max(1u, t->GetWidth() >> mip);
			const uint32_t h = std::max(1u, t->GetHeight() >> mip);
			const uint32_t row = FormatRowPitchGet(t->GetFormat(), w);
			const uint32_t rows = FormatRowCountGet(t->GetFormat(), h);
			if (row_pitch == 0) row_pitch = row;
			UploadAlloc a = UploadAllocate((VkDeviceSize)row * rows, 16);
			if (!a.p_Cpu) return;
			for (uint32_t y = 0; y < rows; ++y) std::memcpy(a.p_Cpu + (size_t)y * row, (const uint8_t*)data + (size_t)y * row_pitch, row);
			VkImageLayout prev = t->m_e_Layout;
			if (prev == VK_IMAGE_LAYOUT_UNDEFINED) prev = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			ImageTransition(t, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			BarriersFlush();
			RenderingEnd();
			VkBufferImageCopy c = {};
			c.bufferOffset = a.i_Offset;
			c.imageSubresource = { (VkImageAspectFlags)(t->m_i_Aspect & ~(VkImageAspectFlags)VK_IMAGE_ASPECT_STENCIL_BIT), mip, 0, 1 };
			c.imageExtent = { w, h, 1 };
			vkCmdCopyBufferToImage(m_p_Cmd, a.p_Buffer, t->m_p_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c);
			ImageTransition(t, prev);
		}

		void VulkanDevice::TextureCopy(Texture* dst, Texture* src)
		{
			VK_LOCK;
			VulkanTexture* d = TexCast(dst);
			VulkanTexture* s = TexCast(src);
			if (!d || !s || !d->m_p_Image || !s->m_p_Image) return;
			CommandsBegin();
			const VkImageLayout prev_s = s->m_e_Layout;
			ImageTransition(d, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			ImageTransition(s, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			BarriersFlush();
			RenderingEnd();
			std::vector<VkImageCopy> regions;
			const uint32_t mips = std::min(d->DescGet().i_MipLevels, s->DescGet().i_MipLevels);
			for (uint32_t m = 0; m < mips; ++m) {
				VkImageCopy c = {};
				c.srcSubresource = { s->m_i_Aspect, m, 0, std::min(d->DescGet().i_ArraySize, s->DescGet().i_ArraySize) };
				c.dstSubresource = { d->m_i_Aspect, m, 0, c.srcSubresource.layerCount };
				c.extent = { std::max(1u, std::min(d->GetWidth(), s->GetWidth()) >> m), std::max(1u, std::min(d->GetHeight(), s->GetHeight()) >> m), 1 };
				regions.push_back(c);
			}
			vkCmdCopyImage(m_p_Cmd, s->m_p_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, d->m_p_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				(uint32_t)regions.size(), regions.data());
			if (prev_s != VK_IMAGE_LAYOUT_UNDEFINED) ImageTransition(s, prev_s);
		}

		/*
		* 関数名　TextureRead
		* 引　数　texture：読み戻すテクスチャ　out_pixels / out_row_pitch：出力
		* 戻り値　成功なら true
		* 説　明　ホスト可視バッファへコピーし、GPU の完了を待って読む（ミップ 0・配列 0 のみ）
		*/
		bool VulkanDevice::TextureRead(Texture* texture, std::vector<uint8_t>& out_pixels, uint32_t& out_row_pitch)
		{
			VK_LOCK;
			VulkanTexture* t = TexCast(texture);
			if (!t || !t->m_p_Image) return false;
			const FormatInfo& fi = FormatInfoGet(t->GetFormat());
			if (fi.b_Compressed || fi.b_Depth) return false;
			VkDevice dev = m_cls_Core->p_Device;
			const uint32_t w = t->GetWidth(), h = t->GetHeight();
			out_row_pitch = FormatRowPitchGet(t->GetFormat(), w);
			const VkDeviceSize size = (VkDeviceSize)out_row_pitch * h;

			VkBufferCreateInfo bi = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			bi.size = size;
			bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			VkBuffer buffer = VK_NULL_HANDLE;
			if (vkCreateBuffer(dev, &bi, nullptr, &buffer) != VK_SUCCESS) return false;
			VkMemoryRequirements req;
			vkGetBufferMemoryRequirements(dev, buffer, &req);
			VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			ai.allocationSize = req.size;
			int32_t type = m_cls_Core->MemoryTypeFind(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
			if (type < 0) type = m_cls_Core->MemoryTypeFind(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			ai.memoryTypeIndex = (uint32_t)type;
			VkDeviceMemory memory = VK_NULL_HANDLE;
			if (type < 0 || vkAllocateMemory(dev, &ai, nullptr, &memory) != VK_SUCCESS) { vkDestroyBuffer(dev, buffer, nullptr); return false; }
			vkBindBufferMemory(dev, buffer, memory, 0);

			CommandsBegin();
			const VkImageLayout prev = t->m_e_Layout;
			ImageTransition(t, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			BarriersFlush();
			RenderingEnd();
			VkBufferImageCopy c = {};
			c.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			c.imageExtent = { w, h, 1 };
			vkCmdCopyImageToBuffer(m_p_Cmd, t->m_p_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &c);
			VkMemoryBarrier mb = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
			mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
			vkCmdPipelineBarrier(m_p_Cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
			if (prev != VK_IMAGE_LAYOUT_UNDEFINED) ImageTransition(t, prev);
			IdleWait();

			void* mapped = nullptr;
			bool ok = false;
			if (vkMapMemory(dev, memory, 0, VK_WHOLE_SIZE, 0, &mapped) == VK_SUCCESS) {
				VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
				range.memory = memory;
				range.size = VK_WHOLE_SIZE;
				vkInvalidateMappedMemoryRanges(dev, 1, &range);
				out_pixels.assign((const uint8_t*)mapped, (const uint8_t*)mapped + size);
				vkUnmapMemory(dev, memory);
				ok = true;
			}
			vkDestroyBuffer(dev, buffer, nullptr);
			vkFreeMemory(dev, memory, nullptr);
			return ok;
		}

		// ================================================================
		// 出力・ステート
		// ================================================================

		void VulkanDevice::ResourceUnbind(const void* r)
		{
			for (uint32_t s = 0; s < STAGES; ++s) {
				for (uint32_t i = 0; i < SRV_SLOTS; ++i)
					if ((const void*)m_Textures[s][i] == r) { m_Textures[s][i] = nullptr; m_b_DescriptorValid = false; }
				for (uint32_t i = 0; i < CB_SLOTS; ++i)
					if (m_ConstantBuffers[s][i] && (const void*)static_cast<Buffer*>(m_ConstantBuffers[s][i]) == r) m_ConstantBuffers[s][i] = nullptr;
				for (uint32_t i = 0; i < SMP_SLOTS; ++i)
					if (m_Samplers[s][i] && (const void*)static_cast<Sampler*>(m_Samplers[s][i]) == r) { m_Samplers[s][i] = nullptr; m_b_DescriptorValid = false; }
			}
			bool rt = false;
			for (uint32_t i = 0; i < m_str_RenderTargets.i_Count; ++i)
				if ((const void*)m_str_RenderTargets.p_Colors[i] == r) { m_str_RenderTargets.p_Colors[i] = nullptr; rt = true; }
			if ((const void*)m_str_RenderTargets.p_Depth == r) { m_str_RenderTargets.p_Depth = nullptr; rt = true; }
			if (rt) {
				// 描画中のレンダリングは破棄されるビューを参照しているので、ここで閉じる
				if (m_b_Recording) RenderingEnd();
				m_b_RenderTargetsDirty = true;
			}
			for (auto& vb : m_VertexBuffers)
				if (vb.p_Buffer && (const void*)static_cast<Buffer*>(vb.p_Buffer) == r) vb = VertexBinding{};
			if (m_p_IndexBuffer && (const void*)static_cast<Buffer*>(m_p_IndexBuffer) == r) m_p_IndexBuffer = nullptr;
		}

		void VulkanDevice::BoundTextureUnbind(Texture* texture)
		{
			if (!texture) return;
			for (uint32_t s = 0; s < STAGES; ++s)
				for (uint32_t i = 0; i < SRV_SLOTS; ++i)
					if (m_Textures[s][i] == texture) { m_Textures[s][i] = nullptr; m_b_DescriptorValid = false; }
		}

		void VulkanDevice::RenderTargetsSet(uint32_t count, Texture* const* colors, Texture* depth)
		{
			RenderTargetState next;
			count = std::min(count, MAX_RENDER_TARGETS);
			for (uint32_t i = 0; i < count; ++i) next.p_Colors[i] = colors ? colors[i] : nullptr;
			while (count > 0 && next.p_Colors[count - 1] == nullptr) --count;
			next.i_Count = count;
			next.p_Depth = depth;
			for (uint32_t i = 0; i < count; ++i) BoundTextureUnbind(next.p_Colors[i]);
			BoundTextureUnbind(depth);
			bool same = next.i_Count == m_str_RenderTargets.i_Count && next.p_Depth == m_str_RenderTargets.p_Depth;
			for (uint32_t i = 0; same && i < count; ++i) same = (next.p_Colors[i] == m_str_RenderTargets.p_Colors[i]);
			if (same) return;
			m_str_RenderTargets = next;
			m_b_RenderTargetsDirty = true;
			m_b_ScissorDirty = true;
		}

		void VulkanDevice::RenderTargetClear(Texture* target, const float color[4])
		{
			VK_LOCK;
			VulkanTexture* t = TexCast(target);
			if (!t || !t->m_p_Image || FormatInfoGet(t->GetFormat()).b_Depth) return;
			CommandsBegin();
			ImageTransition(t, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			BarriersFlush();
			RenderingEnd();
			VkClearColorValue cv;
			std::memcpy(cv.float32, color, sizeof(cv.float32));
			const VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			vkCmdClearColorImage(m_p_Cmd, t->m_p_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &cv, 1, &range);
		}

		void VulkanDevice::DepthStencilClear(Texture* target, bool clear_depth, bool clear_stencil, float depth, uint8_t stencil)
		{
			VK_LOCK;
			VulkanTexture* t = TexCast(target);
			if (!t || !t->m_p_Image || !FormatInfoGet(t->GetFormat()).b_Depth) return;
			VkImageAspectFlags aspect = 0;
			if (clear_depth) aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
			if (clear_stencil && FormatInfoGet(t->GetFormat()).b_Stencil) aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
			if (!aspect) return;
			CommandsBegin();
			ImageTransition(t, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			BarriersFlush();
			RenderingEnd();
			const VkClearDepthStencilValue dv = { depth, stencil };
			const VkImageSubresourceRange range = { aspect, 0, 1, 0, 1 };
			vkCmdClearDepthStencilImage(m_p_Cmd, t->m_p_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &dv, 1, &range);
		}

		void VulkanDevice::BlendStateSet(BlendState* state, const float blend_factor[4], uint32_t sample_mask)
		{
			m_cls_Blend = state;
			m_i_SampleMask = sample_mask;
			const float zero[4] = { 0, 0, 0, 0 };
			std::memcpy(m_f_BlendFactor, blend_factor ? blend_factor : zero, sizeof(m_f_BlendFactor));
			m_b_DynamicDirty = true;
		}

		void VulkanDevice::DepthStencilStateSet(DepthStencilState* state, uint32_t stencil_ref)
		{
			m_cls_Depth = state;
			m_i_StencilRef = stencil_ref;
			m_b_DynamicDirty = true;
		}

		void VulkanDevice::ShaderSet(ShaderStage stage, Shader* shader)
		{
			if ((uint32_t)stage >= STAGES) return;
			m_cls_Shaders[(uint32_t)stage] = static_cast<VulkanShader*>(shader);
		}

		void VulkanDevice::VertexBufferSet(uint32_t slot, Buffer* buffer, uint32_t stride, uint32_t offset)
		{
			if (slot >= MAX_VERTEX_SLOTS) return;
			m_VertexBuffers[slot] = { BufCast(buffer), stride, offset };
		}

		void VulkanDevice::IndexBufferSet(Buffer* buffer, IndexFormat format, uint32_t offset)
		{
			m_p_IndexBuffer = BufCast(buffer);
			m_e_IndexFormat = format;
			m_i_IndexOffset = offset;
		}

		void VulkanDevice::ConstantBufferSet(ShaderStage stage, uint32_t slot, Buffer* buffer)
		{
			if ((uint32_t)stage >= STAGES || slot >= CB_SLOTS) return;
			m_ConstantBuffers[(uint32_t)stage][slot] = BufCast(buffer);
		}

		void VulkanDevice::TextureSet(ShaderStage stage, uint32_t slot, Texture* texture)
		{
			if ((uint32_t)stage >= STAGES || slot >= SRV_SLOTS) return;
			if (texture) {
				if (texture == m_str_RenderTargets.p_Depth) texture = nullptr;
				for (uint32_t i = 0; texture && i < m_str_RenderTargets.i_Count; ++i)
					if (m_str_RenderTargets.p_Colors[i] == texture) texture = nullptr;
			}
			if (texture && !TexCast(texture)->m_p_SrvView) texture = nullptr;
			m_Textures[(uint32_t)stage][slot] = texture;
		}

		void VulkanDevice::SamplerSet(ShaderStage stage, uint32_t slot, Sampler* sampler)
		{
			if ((uint32_t)stage >= STAGES || slot >= SMP_SLOTS) return;
			m_Samplers[(uint32_t)stage][slot] = static_cast<VulkanSampler*>(sampler);
		}

		void VulkanDevice::StateClear()
		{
			VK_LOCK;
			if (m_b_Recording) RenderingEnd();
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

		// ================================================================
		// 描画
		// ================================================================

		/*
		* 関数名　PipelineGet
		* 引　数　なし
		* 戻り値　現在のステートに対応するパイプライン（作れなければ nullptr）
		* 説　明　ステートからキーを作り、キャッシュに無ければ作成する
		*/
		const Pipeline* VulkanDevice::PipelineGet()
		{
			VulkanShader* vs = m_cls_Shaders[0];
			VulkanShader* ps = m_cls_Shaders[1];
			if (!vs || !vs->m_p_Module) return nullptr;
			static const BlendDesc s_DefaultBlend;
			static const DepthStencilDesc s_DefaultDepth;
			static const RasterizerDesc s_DefaultRaster;
			const BlendDesc& bd = m_cls_Blend ? m_cls_Blend->DescGet() : s_DefaultBlend;
			const DepthStencilDesc& dd = m_cls_Depth ? m_cls_Depth->DescGet() : s_DefaultDepth;
			const RasterizerDesc& rd = m_cls_Raster ? m_cls_Raster->DescGet() : s_DefaultRaster;

			VkFormat color_formats[MAX_RENDER_TARGETS] = {};
			for (uint32_t i = 0; i < m_str_RenderTargets.i_Count; ++i)
				color_formats[i] = m_str_RenderTargets.p_Colors[i] ? TexCast(m_str_RenderTargets.p_Colors[i])->m_e_VkFormat : VK_FORMAT_UNDEFINED;
			VulkanTexture* depth_tex = m_str_RenderTargets.p_Depth ? TexCast(m_str_RenderTargets.p_Depth) : nullptr;
			const VkFormat depth_format = depth_tex ? depth_tex->m_e_VkFormat : VK_FORMAT_UNDEFINED;
			const bool has_stencil = depth_tex && FormatInfoGet(depth_tex->GetFormat()).b_Stencil;

			KeyWriter k;
			k << vs->m_i_Id << (ps ? ps->m_i_Id : 0ull) << (m_cls_Layout ? m_cls_Layout->m_i_Id : 0ull) << (uint32_t)m_e_Topology;
			k << m_str_RenderTargets.i_Count << (uint32_t)depth_format << m_i_SampleMask;
			for (uint32_t i = 0; i < m_str_RenderTargets.i_Count; ++i) k << (uint32_t)color_formats[i];
			k << bd.b_AlphaToCoverage << bd.b_IndependentBlend;
			for (uint32_t i = 0; i < (bd.b_IndependentBlend ? MAX_RENDER_TARGETS : 1u); ++i) {
				const auto& r = bd.renderTarget[i];
				k << r.b_BlendEnable << r.e_SrcBlend << r.e_DestBlend << r.e_BlendOp << r.e_SrcBlendAlpha << r.e_DestBlendAlpha << r.e_BlendOpAlpha << r.i_WriteMask;
			}
			k << dd.b_DepthEnable << dd.b_DepthWrite << dd.e_DepthFunc << dd.b_StencilEnable << dd.i_StencilReadMask << dd.i_StencilWriteMask;
			for (const StencilFaceDesc* f : { &dd.frontFace, &dd.backFace }) k << f->e_FailOp << f->e_DepthFailOp << f->e_PassOp << f->e_Func;
			k << rd.e_Fill << rd.e_Cull << rd.b_FrontCounterClockwise << rd.i_DepthBias << rd.f_DepthBiasClamp << rd.f_SlopeScaledDepthBias << rd.b_DepthClip;

			auto it = m_Pipelines.find(k.Get());
			if (it != m_Pipelines.end()) return it->second.p_Pipeline ? &it->second : nullptr;

			VkDevice dev = m_cls_Core->p_Device;
			Pipeline pipeline;

			// ---- シェーダー（PS は VS の出力に合わせて location を書き換える）----
			VkShaderModule ps_module = VK_NULL_HANDLE;
			VkPipelineShaderStageCreateInfo stages[2] = {};
			stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vs->m_p_Module, "main", nullptr };
			uint32_t stage_count = 1;
			if (ps) {
				const std::vector<uint32_t> code = SpirvSemanticsStrip(SpirvInputLocationsRemap(ps->m_Words, vs->ReflectionGet().outputs));
				VkShaderModuleCreateInfo mi = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
				mi.codeSize = code.size() * 4;
				mi.pCode = code.data();
				if (vkCreateShaderModule(dev, &mi, nullptr, &ps_module) == VK_SUCCESS) {
					stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, ps_module, "main", nullptr };
					stage_count = 2;
				}
			}

			// ---- 頂点入力（セマンティクス名で VS 入力の location に対応付け）----
			std::vector<VkVertexInputBindingDescription> vbindings;
			std::vector<VkVertexInputAttributeDescription> attributes;
			if (m_cls_Layout) {
				for (const auto& e : m_cls_Layout->ElementsGet()) {
					const std::string sem = UpperGet(e.c_Semantic);
					for (const auto& in : vs->ReflectionGet().inputs) {
						if (in.semantic != sem || in.i_SemanticIndex != e.i_SemanticIndex) continue;
						attributes.push_back({ in.i_Location, e.i_Slot, m_cls_Core->FormatGet(e.e_Format), e.i_Offset });
						if (!(pipeline.i_VertexSlotMask & (1u << e.i_Slot))) {
							pipeline.i_VertexSlotMask |= 1u << e.i_Slot;
							vbindings.push_back({ e.i_Slot, 0, e.e_Rate == InputRate::PerInstance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX });
						}
						break;
					}
				}
			}
			VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
			vi.vertexBindingDescriptionCount = (uint32_t)vbindings.size();
			vi.pVertexBindingDescriptions = vbindings.data();
			vi.vertexAttributeDescriptionCount = (uint32_t)attributes.size();
			vi.pVertexAttributeDescriptions = attributes.data();

			VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
			ia.topology = TopologyGet(m_e_Topology);
			VkPipelineViewportStateCreateInfo vps = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
			vps.viewportCount = 1;
			vps.scissorCount = 1;

			VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
			rs.polygonMode = (rd.e_Fill == FillMode::Wireframe && m_cls_Core->str_Features.fillModeNonSolid) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
			rs.cullMode = rd.e_Cull == CullMode::None ? VK_CULL_MODE_NONE : rd.e_Cull == CullMode::Front ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
			// ビューポートを上下反転しているため、画面上の回り方は D3D と同じになる
			rs.frontFace = rd.b_FrontCounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
			rs.depthClampEnable = (!rd.b_DepthClip && m_cls_Core->str_Features.depthClamp) ? VK_TRUE : VK_FALSE;
			rs.depthBiasEnable = (rd.i_DepthBias != 0 || rd.f_SlopeScaledDepthBias != 0.0f) ? VK_TRUE : VK_FALSE;
			// D3D の DepthBias は深度形式の最小単位の倍数。Vulkan の constantFactor も同じ解釈
			rs.depthBiasConstantFactor = (float)rd.i_DepthBias;
			rs.depthBiasClamp = rd.f_DepthBiasClamp;
			rs.depthBiasSlopeFactor = rd.f_SlopeScaledDepthBias;
			rs.lineWidth = 1.0f;

			VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
			ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			const VkSampleMask sample_mask = m_i_SampleMask;
			ms.pSampleMask = &sample_mask;
			ms.alphaToCoverageEnable = bd.b_AlphaToCoverage;

			VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
			if (depth_format != VK_FORMAT_UNDEFINED) {
				ds.depthTestEnable = dd.b_DepthEnable;
				ds.depthWriteEnable = dd.b_DepthEnable && dd.b_DepthWrite;
				ds.depthCompareOp = CompareGet(dd.e_DepthFunc);
				ds.stencilTestEnable = has_stencil && dd.b_StencilEnable;
				auto face = [&](const StencilFaceDesc& f) {
					VkStencilOpState s = {};
					s.failOp = StencilOpGet(f.e_FailOp);
					s.passOp = StencilOpGet(f.e_PassOp);
					s.depthFailOp = StencilOpGet(f.e_DepthFailOp);
					s.compareOp = CompareGet(f.e_Func);
					s.compareMask = dd.i_StencilReadMask;
					s.writeMask = dd.i_StencilWriteMask;
					return s;
				};
				ds.front = face(dd.frontFace);
				ds.back = face(dd.backFace);
			}
			ds.maxDepthBounds = 1.0f;

			VkPipelineColorBlendAttachmentState attachments[MAX_RENDER_TARGETS] = {};
			for (uint32_t i = 0; i < m_str_RenderTargets.i_Count; ++i) {
				const auto& r = bd.renderTarget[bd.b_IndependentBlend && m_cls_Core->str_Features.independentBlend ? i : 0];
				auto& a = attachments[i];
				a.blendEnable = r.b_BlendEnable;
				a.srcColorBlendFactor = BlendGet(r.e_SrcBlend);
				a.dstColorBlendFactor = BlendGet(r.e_DestBlend);
				a.colorBlendOp = BlendOpGet(r.e_BlendOp);
				a.srcAlphaBlendFactor = BlendGet(r.e_SrcBlendAlpha);
				a.dstAlphaBlendFactor = BlendGet(r.e_DestBlendAlpha);
				a.alphaBlendOp = BlendOpGet(r.e_BlendOpAlpha);
				a.colorWriteMask = r.i_WriteMask;   // R/G/B/A のビット配置は D3D と同じ
				// 整数形式はブレンドできない
				if (color_formats[i] == VK_FORMAT_R32G32B32A32_UINT || color_formats[i] == VK_FORMAT_R32_UINT || color_formats[i] == VK_FORMAT_R16_UINT)
					a.blendEnable = VK_FALSE;
			}
			VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
			cb.attachmentCount = m_str_RenderTargets.i_Count;
			cb.pAttachments = attachments;

			const VkDynamicState dyn[] = {
				VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_BLEND_CONSTANTS,
				VK_DYNAMIC_STATE_STENCIL_REFERENCE, VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,
			};
			VkPipelineDynamicStateCreateInfo dy = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
			dy.dynamicStateCount = (uint32_t)(sizeof(dyn) / sizeof(dyn[0]));
			dy.pDynamicStates = dyn;

			VkPipelineRenderingCreateInfo ri = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
			ri.colorAttachmentCount = m_str_RenderTargets.i_Count;
			ri.pColorAttachmentFormats = color_formats;
			ri.depthAttachmentFormat = depth_format;
			ri.stencilAttachmentFormat = has_stencil ? depth_format : VK_FORMAT_UNDEFINED;

			VkGraphicsPipelineCreateInfo pi = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
			pi.pNext = &ri;
			pi.stageCount = stage_count;
			pi.pStages = stages;
			pi.pVertexInputState = &vi;
			pi.pInputAssemblyState = &ia;
			pi.pViewportState = &vps;
			pi.pRasterizationState = &rs;
			pi.pMultisampleState = &ms;
			pi.pDepthStencilState = &ds;
			pi.pColorBlendState = &cb;
			pi.pDynamicState = &dy;
			pi.layout = m_p_PipelineLayout;
			VkResult r = vkCreateGraphicsPipelines(dev, m_p_PipelineCache, 1, &pi, nullptr, &pipeline.p_Pipeline);
			if (ps_module) vkDestroyShaderModule(dev, ps_module, nullptr);
			if (r != VK_SUCCESS) {
				LogWrite("Vulkan: パイプライン作成失敗 (VS=%s PS=%s) %s", vs->NameGet().c_str(), ps ? ps->NameGet().c_str() : "-", VkResultName(r));
				pipeline.p_Pipeline = VK_NULL_HANDLE;
			}
			auto& stored = m_Pipelines[k.Get()];
			stored = pipeline;
			return stored.p_Pipeline ? &stored : nullptr;
		}

		VkDescriptorSet VulkanDevice::DescriptorSetAllocate()
		{
			FrameContext& f = m_Frames[m_i_FrameSlot];
			VkDevice dev = m_cls_Core->p_Device;
			for (;;) {
				if (f.i_PoolIndex >= f.descriptorPools.size()) {
					constexpr uint32_t SETS = 1024;
					const VkDescriptorPoolSize sizes[] = {
						{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SETS * 8 },
						{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, SETS * 16 },
						{ VK_DESCRIPTOR_TYPE_SAMPLER, SETS * 4 },
					};
					VkDescriptorPoolCreateInfo pi = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
					pi.maxSets = SETS;
					pi.poolSizeCount = 3;
					pi.pPoolSizes = sizes;
					VkDescriptorPool pool = VK_NULL_HANDLE;
					if (vkCreateDescriptorPool(dev, &pi, nullptr, &pool) != VK_SUCCESS) return VK_NULL_HANDLE;
					f.descriptorPools.push_back(pool);
				}
				VkDescriptorSetAllocateInfo ai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
				ai.descriptorPool = f.descriptorPools[f.i_PoolIndex];
				ai.descriptorSetCount = 1;
				ai.pSetLayouts = &m_p_SetLayout;
				VkDescriptorSet set = VK_NULL_HANDLE;
				const VkResult r = vkAllocateDescriptorSets(dev, &ai, &set);
				if (r == VK_SUCCESS) return set;
				if (r != VK_ERROR_OUT_OF_POOL_MEMORY && r != VK_ERROR_FRAGMENTED_POOL) return VK_NULL_HANDLE;
				++f.i_PoolIndex;   // このプールは満杯
			}
		}

		/*
		* 関数名　DescriptorSetUpdate
		* 引　数　なし
		* 戻り値　成功なら true
		* 説　明　シェーダーが使うバインドだけを集め、前回と変わっていればセットを確保して書き込む
		*/
		bool VulkanDevice::DescriptorSetUpdate()
		{
			using namespace ShaderBindings;
			DescriptorEntry entries[BINDING_COUNT] = {};
			VkDescriptorType types[BINDING_COUNT] = {};
			bool used[BINDING_COUNT] = {};
			const VkDeviceSize max_range = m_cls_Core->str_Props.limits.maxUniformBufferRange;

			for (uint32_t s = 0; s < STAGES; ++s) {
				VulkanShader* sh = m_cls_Shaders[s];
				if (!sh) continue;
				const ShaderReflection& ref = sh->ReflectionGet();
				for (const auto& c : ref.constantBuffers) {
					const uint32_t b = VulkanCbBinding((Stage)s, c.i_Slot);
					DescriptorEntry& e = entries[b];
					VulkanBuffer* buf = m_ConstantBuffers[s][c.i_Slot];
					if (buf && buf->IsStreamed()) {
						StreamedBufferResolve(buf, e.p_Buffer, e.i_Offset);
						e.i_Range = std::min<VkDeviceSize>(buf->m_Shadow.size(), max_range);
					}
					if (!e.p_Buffer) {
						e.p_Buffer = m_p_DummyUbo;
						e.i_Offset = 0;
						e.i_Range = std::min<VkDeviceSize>(DUMMY_UBO_SIZE, max_range);
					}
					types[b] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					used[b] = true;
				}
				for (const auto& t : ref.textures) {
					const uint32_t b = VulkanSrvBinding((Stage)s, t.i_Slot);
					Texture* tex = m_Textures[s][t.i_Slot];
					entries[b].p_View = tex ? TexCast(tex)->m_p_SrvView : (t.b_Array ? m_p_NullArrayView : m_cls_NullTexture->m_p_SrvView);
					types[b] = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
					used[b] = true;
				}
				for (const auto& smp : ref.samplers) {
					const uint32_t b = VulkanSamplerBinding((Stage)s, smp.i_Slot);
					VulkanSampler* sm = m_Samplers[s][smp.i_Slot];
					entries[b].p_Sampler = sm ? sm->m_p_Sampler : m_p_NullSampler;
					types[b] = VK_DESCRIPTOR_TYPE_SAMPLER;
					used[b] = true;
				}
			}

			if (m_b_DescriptorValid) {
				bool same = true;
				for (uint32_t b = 0; b < BINDING_COUNT && same; ++b) same = (entries[b] == m_LastDescriptors[b]);
				if (same) return true;
			}

			VkDescriptorSet set = DescriptorSetAllocate();
			if (!set) return false;
			VkWriteDescriptorSet writes[BINDING_COUNT];
			VkDescriptorBufferInfo buffer_infos[BINDING_COUNT];
			VkDescriptorImageInfo image_infos[BINDING_COUNT];
			uint32_t count = 0;
			for (uint32_t b = 0; b < BINDING_COUNT; ++b) {
				if (!used[b]) continue;
				VkWriteDescriptorSet& w = writes[count++];
				w = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				w.dstSet = set;
				w.dstBinding = b;
				w.descriptorCount = 1;
				w.descriptorType = types[b];
				if (types[b] == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
					buffer_infos[b] = { entries[b].p_Buffer, entries[b].i_Offset, entries[b].i_Range };
					w.pBufferInfo = &buffer_infos[b];
				}
				else {
					image_infos[b] = { entries[b].p_Sampler, entries[b].p_View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
					w.pImageInfo = &image_infos[b];
				}
			}
			vkUpdateDescriptorSets(m_cls_Core->p_Device, count, writes, 0, nullptr);
			vkCmdBindDescriptorSets(m_p_Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_p_PipelineLayout, 0, 1, &set, 0, nullptr);
			std::memcpy(m_LastDescriptors, entries, sizeof(entries));
			m_b_DescriptorValid = true;
			return true;
		}

		/*
		* 関数名　DrawPrepare
		* 引　数　なし
		* 戻り値　描画できるなら true
		* 説　明　画像レイアウトの遷移 → レンダリング開始 → パイプライン・動的ステート・バッファ・ディスクリプタの順に反映する
		*/
		bool VulkanDevice::DrawPrepare()
		{
			CommandsBegin();
			if (m_str_RenderTargets.i_Count == 0 && !m_str_RenderTargets.p_Depth) return false;

			// ---- レイアウト遷移（レンダリング中は張れないので先に済ませる）----
			for (uint32_t s = 0; s < STAGES; ++s)
				for (uint32_t i = 0; i < SRV_SLOTS; ++i)
					if (m_Textures[s][i]) ImageTransition(TexCast(m_Textures[s][i]), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			for (uint32_t i = 0; i < m_str_RenderTargets.i_Count; ++i)
				if (m_str_RenderTargets.p_Colors[i]) ImageTransition(TexCast(m_str_RenderTargets.p_Colors[i]), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			if (m_str_RenderTargets.p_Depth) ImageTransition(TexCast(m_str_RenderTargets.p_Depth), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
			BarriersFlush();

			const Pipeline* pipeline = PipelineGet();
			if (!pipeline) return false;

			// ---- レンダリング開始 ----
			if (!m_b_Rendering || m_b_RenderTargetsDirty) {
				RenderingEnd();
				// 前のレンダリングでの書き込みを今回の読み込み (LOAD) から見えるようにする
				VkMemoryBarrier mb = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
				mb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				mb.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				const VkPipelineStageFlags stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				vkCmdPipelineBarrier(m_p_Cmd, stages, stages, 0, 1, &mb, 0, nullptr, 0, nullptr);

				VkRenderingAttachmentInfo colors[MAX_RENDER_TARGETS] = {};
				uint32_t w = UINT32_MAX, h = UINT32_MAX;
				for (uint32_t i = 0; i < m_str_RenderTargets.i_Count; ++i) {
					colors[i] = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
					colors[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					colors[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
					colors[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
					if (Texture* t = m_str_RenderTargets.p_Colors[i]) {
						colors[i].imageView = TexCast(t)->m_p_AttachView;
						w = std::min(w, t->GetWidth());
						h = std::min(h, t->GetHeight());
					}
				}
				VkRenderingAttachmentInfo depth = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
				const bool has_depth = m_str_RenderTargets.p_Depth != nullptr;
				const bool has_stencil = has_depth && FormatInfoGet(m_str_RenderTargets.p_Depth->GetFormat()).b_Stencil;
				if (has_depth) {
					depth.imageView = TexCast(m_str_RenderTargets.p_Depth)->m_p_AttachView;
					depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
					depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
					w = std::min(w, m_str_RenderTargets.p_Depth->GetWidth());
					h = std::min(h, m_str_RenderTargets.p_Depth->GetHeight());
				}
				if (w == UINT32_MAX) w = h = 1;
				m_str_RenderExtent = { w, h };
				VkRenderingInfo ri = { VK_STRUCTURE_TYPE_RENDERING_INFO };
				ri.renderArea = { { 0, 0 }, m_str_RenderExtent };
				ri.layerCount = 1;
				ri.colorAttachmentCount = m_str_RenderTargets.i_Count;
				ri.pColorAttachments = colors;
				ri.pDepthAttachment = has_depth ? &depth : nullptr;
				ri.pStencilAttachment = has_stencil ? &depth : nullptr;
				vkCmdBeginRendering(m_p_Cmd, &ri);
				m_b_Rendering = true;
				m_b_RenderTargetsDirty = false;
				m_b_ScissorDirty = true;
			}

			if (pipeline != m_p_CurrentPipeline) {
				vkCmdBindPipeline(m_p_Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->p_Pipeline);
				m_p_CurrentPipeline = pipeline;
			}

			// ---- 動的ステート ----
			if (m_b_ViewportDirty) {
				// 高さを負にして D3D と同じ向き（+Y が上、原点は左上）にする
				const Viewport& v = m_str_Viewport;
				const float vh = (v.f_Height != 0.0f) ? v.f_Height : 1.0f;
				const VkViewport vp = { v.f_X, v.f_Y + vh, v.f_Width > 0.0f ? v.f_Width : 1.0f, -vh, v.f_MinDepth, v.f_MaxDepth };
				vkCmdSetViewport(m_p_Cmd, 0, 1, &vp);
				m_b_ViewportDirty = false;
			}
			if (m_b_ScissorDirty) {
				VkRect2D rc = { { 0, 0 }, m_str_RenderExtent };
				if (m_cls_Raster && m_cls_Raster->DescGet().b_Scissor) {
					const int32_t l = std::max(0, m_str_Scissor.i_Left), t = std::max(0, m_str_Scissor.i_Top);
					const int32_t r = std::max(l, m_str_Scissor.i_Right), b = std::max(t, m_str_Scissor.i_Bottom);
					rc = { { l, t }, { (uint32_t)(r - l), (uint32_t)(b - t) } };
				}
				vkCmdSetScissor(m_p_Cmd, 0, 1, &rc);
				m_b_ScissorDirty = false;
			}
			if (m_b_DynamicDirty) {
				vkCmdSetBlendConstants(m_p_Cmd, m_f_BlendFactor);
				vkCmdSetStencilReference(m_p_Cmd, VK_STENCIL_FACE_FRONT_AND_BACK, m_i_StencilRef);
				m_b_DynamicDirty = false;
			}

			// ---- 頂点・インデックス ----
			for (uint32_t slot = 0; slot < MAX_VERTEX_SLOTS; ++slot) {
				if (!(pipeline->i_VertexSlotMask & (1u << slot))) continue;
				const VertexBinding& vb = m_VertexBuffers[slot];
				if (!vb.p_Buffer) return false;   // PSO が使うスロットにバッファが無い
				VkBuffer buffer = vb.p_Buffer->m_p_Buffer;
				VkDeviceSize offset = vb.i_Offset;
				if (vb.p_Buffer->IsStreamed()) {
					VkDeviceSize base = 0;
					StreamedBufferResolve(vb.p_Buffer, buffer, base);
					offset += base;
				}
				const VkDeviceSize stride = vb.i_Stride;
				vkCmdBindVertexBuffers2(m_p_Cmd, slot, 1, &buffer, &offset, nullptr, &stride);
			}
			if (m_p_IndexBuffer) {
				VkBuffer buffer = m_p_IndexBuffer->m_p_Buffer;
				VkDeviceSize offset = m_i_IndexOffset;
				if (m_p_IndexBuffer->IsStreamed()) {
					VkDeviceSize base = 0;
					StreamedBufferResolve(m_p_IndexBuffer, buffer, base);
					offset += base;
				}
				vkCmdBindIndexBuffer(m_p_Cmd, buffer, offset, m_e_IndexFormat == IndexFormat::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
			}

			return DescriptorSetUpdate();
		}

		void VulkanDevice::Draw(uint32_t vertex_count, uint32_t start_vertex)
		{
			VK_LOCK;
			if (DrawPrepare()) vkCmdDraw(m_p_Cmd, vertex_count, 1, start_vertex, 0);
		}

		void VulkanDevice::IndexedDraw(uint32_t index_count, uint32_t start_index, int32_t base_vertex)
		{
			VK_LOCK;
			if (!m_p_IndexBuffer) return;
			if (DrawPrepare()) vkCmdDrawIndexed(m_p_Cmd, index_count, 1, start_index, base_vertex, 0);
		}

		void VulkanDevice::InstancedDraw(uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex, uint32_t start_instance)
		{
			VK_LOCK;
			if (DrawPrepare()) vkCmdDraw(m_p_Cmd, vertex_count, instance_count, start_vertex, start_instance);
		}

		void* VulkanDevice::NativeCommandsBegin()
		{
			VK_LOCK;
			CommandsBegin();
			RenderingEnd();
			BarriersFlush();
			return m_p_Cmd;
		}

		void VulkanDevice::NativeCommandsEnd()
		{
			AllDirtyMark();
			m_b_Rendering = false;
		}

		void VulkanDevice::MarkerBegin(const char* name)
		{
			if (!m_fn_LabelBegin || !m_b_Recording || !name) return;
			VkDebugUtilsLabelEXT label = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
			label.pLabelName = name;
			m_fn_LabelBegin(m_p_Cmd, &label);
		}

		void VulkanDevice::MarkerEnd()
		{
			if (m_fn_LabelEnd && m_b_Recording) m_fn_LabelEnd(m_p_Cmd);
		}
	}

	// ================================================================
	// 公開関数
	// ================================================================

	std::unique_ptr<Device> VulkanDeviceCreate(const DeviceCreateInfo& info, std::string* out_error)
	{
		auto device = std::make_unique<VulkanDevice>();
		if (!device->Init(info, out_error)) return nullptr;
		LogWrite("Vulkan: %s", device->AdapterNameGet());
		return device;
	}
}

#endif // PIXEON_RHI_VULKAN
