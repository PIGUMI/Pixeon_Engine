/*
* ファイル名　RHI
* 説　　　明　描画 API の抽象化層 (Render Hardware Interface)
*
* 設計
*   エンジンの描画コードは D3D11 のイミディエイトコンテキスト方式で書かれているため、
*   RHI も「ステートオブジェクトを Set して Draw する」同じ形の API にしている。
*   これにより既存コードの移植が機械的になり、D3D11 バックエンドは薄い対応付けで済む。
*
*   D3D12 / Vulkan バックエンドはこの方式を内部でエミュレートする
*     - Draw 時点のステートからパイプライン (PSO) を生成してキャッシュ
*     - ディスクリプタはフレームごとのリングから描画単位で確保
*     - 定数バッファ / Dynamic バッファは更新のたびに新しい領域へ書き込み（リネーム）、
*       D3D11 の UpdateSubresource / Map(WRITE_DISCARD) と同じ結果になる
*     - リソースの状態遷移 (バリア / レイアウト) は自動で追跡
*
* 所有権
*   リソースは std::shared_ptr (〜Ref) で保持する。Set 系には生ポインタを渡す（所有しない）。
*   D3D12 / Vulkan では GPU が使い終わるまで実体の破棄を遅延させる。
*/
#ifndef RHI_H
#define RHI_H

#include "RHITypes.h"

struct ImDrawData;

namespace RHI {

	// ================================================================
	// リソース
	// ================================================================
	class Resource {
	public:
		virtual ~Resource() = default;
		Resource(const Resource&) = delete;
		Resource& operator=(const Resource&) = delete;
	protected:
		Resource() = default;
	};

	class Buffer : public Resource {
	public:
		const BufferDesc& DescGet() const { return m_str_Desc; }
		uint32_t GetSize() const { return m_str_Desc.i_Size; }
	protected:
		explicit Buffer(const BufferDesc& desc) : m_str_Desc(desc) {}
		BufferDesc m_str_Desc;
	};

	class Texture : public Resource {
	public:
		const TextureDesc& DescGet() const { return m_str_Desc; }
		uint32_t GetWidth() const { return m_str_Desc.i_Width; }
		uint32_t GetHeight() const { return m_str_Desc.i_Height; }
		Format GetFormat() const { return m_str_Desc.e_Format; }
	protected:
		explicit Texture(const TextureDesc& desc) : m_str_Desc(desc) {}
		TextureDesc m_str_Desc;
	};

	class Sampler : public Resource {
	public:
		const SamplerDesc& DescGet() const { return m_str_Desc; }
	protected:
		explicit Sampler(const SamplerDesc& desc) : m_str_Desc(desc) {}
		SamplerDesc m_str_Desc;
	};

	class Shader : public Resource {
	public:
		ShaderStage GetStage() const { return m_e_Stage; }
		const std::string& NameGet() const { return m_Name; }
		const ShaderReflection& ReflectionGet() const { return m_str_Reflection; }
		const std::vector<uint8_t>& BytecodeGet() const { return m_Bytecode; }
	protected:
		Shader(ShaderStage stage, const void* bytecode, size_t size, const char* name)
			: m_e_Stage(stage), m_Name(name ? name : ""),
			m_Bytecode((const uint8_t*)bytecode, (const uint8_t*)bytecode + size) {}
		ShaderStage m_e_Stage;
		std::string m_Name;
		std::vector<uint8_t> m_Bytecode;
		ShaderReflection m_str_Reflection;
	};

	class InputLayout : public Resource {
	public:
		const std::vector<InputElement>& ElementsGet() const { return m_Elements; }
	protected:
		InputLayout(const InputElement* elements, uint32_t count);
		std::vector<InputElement> m_Elements;
		std::vector<std::string> m_SemanticStorage;   // c_Semantic の実体
	};

	class BlendState : public Resource {
	public:
		const BlendDesc& DescGet() const { return m_str_Desc; }
	protected:
		explicit BlendState(const BlendDesc& desc) : m_str_Desc(desc) {}
		BlendDesc m_str_Desc;
	};

	class DepthStencilState : public Resource {
	public:
		const DepthStencilDesc& DescGet() const { return m_str_Desc; }
	protected:
		explicit DepthStencilState(const DepthStencilDesc& desc) : m_str_Desc(desc) {}
		DepthStencilDesc m_str_Desc;
	};

	class RasterizerState : public Resource {
	public:
		const RasterizerDesc& DescGet() const { return m_str_Desc; }
	protected:
		explicit RasterizerState(const RasterizerDesc& desc) : m_str_Desc(desc) {}
		RasterizerDesc m_str_Desc;
	};

	using BufferRef = std::shared_ptr<Buffer>;
	using TextureRef = std::shared_ptr<Texture>;
	using SamplerRef = std::shared_ptr<Sampler>;
	using ShaderRef = std::shared_ptr<Shader>;
	using InputLayoutRef = std::shared_ptr<InputLayout>;
	using BlendStateRef = std::shared_ptr<BlendState>;
	using DepthStencilStateRef = std::shared_ptr<DepthStencilState>;
	using RasterizerStateRef = std::shared_ptr<RasterizerState>;

	// レンダーターゲットの退避・復帰用
	struct RenderTargetState {
		uint32_t i_Count = 0;
		Texture* p_Colors[MAX_RENDER_TARGETS] = {};
		Texture* p_Depth = nullptr;
	};

	// 外部ライブラリ (Effekseer など) 連携用のネイティブハンドル
	struct NativeHandles {
		void* p_Device = nullptr;         // ID3D11Device / ID3D12Device / VkDevice
		void* p_Context = nullptr;        // ID3D11DeviceContext / ID3D12GraphicsCommandList(現在のフレーム) / VkCommandBuffer
		void* p_Queue = nullptr;          // ID3D12CommandQueue / VkQueue
		void* p_PhysicalDevice = nullptr; // VkPhysicalDevice
		void* p_Instance = nullptr;       // VkInstance
	};

	// ================================================================
	// デバイス（イミディエイトコンテキストを兼ねる）
	// ================================================================
	class Device {
	public:
		virtual ~Device() = default;

		// ---- 情報 ----
		virtual Backend BackendGet() const = 0;
		virtual const char* AdapterNameGet() const = 0;
		virtual NativeHandles NativeHandlesGet() const = 0;
		virtual uint64_t FrameIndexGet() const = 0;

		// ---- リソース生成 ----
		virtual BufferRef BufferCreate(const BufferDesc& desc, const void* initial_data = nullptr) = 0;
		virtual TextureRef TextureCreate(const TextureDesc& desc, const SubresourceData* initial_data = nullptr) = 0;
		virtual SamplerRef SamplerCreate(const SamplerDesc& desc) = 0;
		// D3D11 / D3D12 は DXBC、Vulkan は SPIR-V
		virtual ShaderRef ShaderCreate(ShaderStage stage, const void* bytecode, size_t size, const char* name = nullptr) = 0;
		virtual InputLayoutRef InputLayoutCreate(const InputElement* elements, uint32_t count, const Shader* vertex_shader) = 0;
		virtual BlendStateRef BlendStateCreate(const BlendDesc& desc) = 0;
		virtual DepthStencilStateRef DepthStencilStateCreate(const DepthStencilDesc& desc) = 0;
		virtual RasterizerStateRef RasterizerStateCreate(const RasterizerDesc& desc) = 0;

		// ---- リソース更新 ----
		// 範囲を指定しない場合はバッファ全体（UpdateSubresource 相当）
		virtual void BufferUpdate(Buffer* buffer, const void* data, uint32_t size, uint32_t offset = 0) = 0;
		// Dynamic バッファ用。前回の内容は破棄される（WRITE_DISCARD 相当）
		virtual void* BufferMap(Buffer* buffer) = 0;
		virtual void BufferUnmap(Buffer* buffer) = 0;
		virtual void TextureUpdate(Texture* texture, uint32_t mip, const void* data, uint32_t row_pitch) = 0;
		virtual void TextureCopy(Texture* dst, Texture* src) = 0;
		// GPU から読み戻す（RGBA8 / RGBA16F 等の非圧縮のみ、テスト・スクリーンショット用）
		virtual bool TextureRead(Texture* texture, std::vector<uint8_t>& out_pixels, uint32_t& out_row_pitch) = 0;

		// ---- フレーム ----
		virtual Texture* BackBufferGet() = 0;
		virtual void Present() = 0;
		virtual void SwapChainResize(uint32_t width, uint32_t height) = 0;
		virtual void VSyncSet(bool enable) = 0;
		virtual void IdleWait() = 0;

		// ---- 出力 ----
		virtual void RenderTargetsSet(uint32_t count, Texture* const* colors, Texture* depth) = 0;
		virtual RenderTargetState RenderTargetsGet() const = 0;
		virtual void RenderTargetClear(Texture* target, const float color[4]) = 0;
		virtual void DepthStencilClear(Texture* target, bool clear_depth, bool clear_stencil, float depth, uint8_t stencil) = 0;
		virtual void ViewportSet(const Viewport& viewport) = 0;
		virtual Viewport ViewportGet() const = 0;
		virtual void ScissorSet(const Rect& rect) = 0;

		// ---- ステート ----
		virtual void BlendStateSet(BlendState* state, const float blend_factor[4] = nullptr, uint32_t sample_mask = 0xFFFFFFFFu) = 0;
		virtual BlendState* BlendStateGet() const = 0;
		virtual void DepthStencilStateSet(DepthStencilState* state, uint32_t stencil_ref = 0) = 0;
		virtual DepthStencilState* DepthStencilStateGet() const = 0;
		virtual void RasterizerStateSet(RasterizerState* state) = 0;
		virtual RasterizerState* RasterizerStateGet() const = 0;

		virtual void ShaderSet(ShaderStage stage, Shader* shader) = 0;
		virtual void InputLayoutSet(InputLayout* layout) = 0;
		virtual void TopologySet(Topology topology) = 0;
		virtual void VertexBufferSet(uint32_t slot, Buffer* buffer, uint32_t stride, uint32_t offset = 0) = 0;
		virtual void IndexBufferSet(Buffer* buffer, IndexFormat format, uint32_t offset = 0) = 0;
		virtual void ConstantBufferSet(ShaderStage stage, uint32_t slot, Buffer* buffer) = 0;
		virtual void TextureSet(ShaderStage stage, uint32_t slot, Texture* texture) = 0;
		virtual void SamplerSet(ShaderStage stage, uint32_t slot, Sampler* sampler) = 0;
		virtual void StateClear() = 0;

		// ---- 描画 ----
		virtual void Draw(uint32_t vertex_count, uint32_t start_vertex = 0) = 0;
		virtual void IndexedDraw(uint32_t index_count, uint32_t start_index = 0, int32_t base_vertex = 0) = 0;
		virtual void InstancedDraw(uint32_t vertex_count, uint32_t instance_count, uint32_t start_vertex = 0, uint32_t start_instance = 0) = 0;

		// ---- 外部ライブラリとの連携 ----
		// ここまでに Set したステート（描画先・リソース状態）を確定させ、ネイティブのコマンド記録先を返す
		//   D3D11: ID3D11DeviceContext*  D3D12: ID3D12GraphicsCommandList*  Vulkan: VkCommandBuffer
		// 外部ライブラリが描画した後は NativeCommandsEnd で RHI 側のキャッシュを無効化する
		virtual void* NativeCommandsBegin() = 0;
		virtual void NativeCommandsEnd() = 0;

		// ---- デバッグ ----
		virtual void MarkerBegin(const char* name) { (void)name; }
		virtual void MarkerEnd() {}
	};

	// ================================================================
	// ImGui 描画（エディタ用。バックエンドごとに実装）
	// ================================================================
	class ImGuiRenderer {
	public:
		virtual ~ImGuiRenderer() = default;
		virtual bool Init() = 0;
		virtual void Shutdown() = 0;
		virtual void NewFrame() = 0;
		// 現在のレンダーターゲット（通常はバックバッファ）へ描画する
		virtual void Render(ImDrawData* draw_data) = 0;
		// ImGui::Image に渡す ID を取得する（フレームをまたいで有効）
		virtual void* TextureIdGet(Texture* texture) = 0;
		virtual void DeviceObjectsInvalidate() {}
		virtual void DeviceObjectsCreate() {}
	};

	// ================================================================
	// 生成
	// ================================================================
	std::unique_ptr<Device> DeviceCreate(Backend backend, const DeviceCreateInfo& info, std::string* out_error = nullptr);
	std::unique_ptr<ImGuiRenderer> ImGuiRendererCreate(Device* device);

	// エンジン全体で使う現在のデバイス
	Device* DeviceGet();
	void DeviceSet(Device* device);

	// バックエンドがビルドに含まれているか
	bool IsBackendAvailable(Backend backend);
}

#endif // !RHI_H
