/*
* ファイル名　RHITypes
* 説　　　明　RHI (Render Hardware Interface) の共通型定義
*             D3D11 / D3D12 / Vulkan のどれにも依存しない記述子と列挙型
*/
#ifndef RHI_TYPES_H
#define RHI_TYPES_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace RHI {

	// ================================================================
	// フォーマット
	// ================================================================
	enum class Format : uint8_t {
		Unknown = 0,
		// カラー
		R8_UNorm,
		R8G8B8A8_UNorm,
		R8G8B8A8_UNorm_sRGB,
		B8G8R8A8_UNorm,
		B8G8R8A8_UNorm_sRGB,
		R16_Float,
		R16G16_Float,
		R16G16B16A16_Float,
		R32_Float,
		R32G32_Float,
		R32G32B32_Float,        // 頂点属性専用
		R32G32B32A32_Float,
		R16_UInt,               // インデックス用
		R32_UInt,
		R32G32B32A32_UInt,
		// 深度
		D24_UNorm_S8_UInt,
		D32_Float,
		// ブロック圧縮 (DDS)
		BC1_UNorm, BC1_UNorm_sRGB,
		BC2_UNorm, BC2_UNorm_sRGB,
		BC3_UNorm, BC3_UNorm_sRGB,
		BC4_UNorm,
		BC5_UNorm,
		BC6H_UF16,
		BC7_UNorm, BC7_UNorm_sRGB,
		Count
	};

	struct FormatInfo {
		uint32_t i_BlockSize;      // 1 ブロックあたりのバイト数（非圧縮は 1 ピクセル）
		uint32_t i_BlockWidth;     // 1 ブロックのピクセル幅（非圧縮は 1）
		bool     b_Depth;
		bool     b_Stencil;
		bool     b_Compressed;
	};
	const FormatInfo& FormatInfoGet(Format format);
	const char* FormatNameGet(Format format);
	// 指定幅の 1 行 (ブロック行) のバイト数
	uint32_t FormatRowPitchGet(Format format, uint32_t width);
	// 指定サイズのブロック行数
	uint32_t FormatRowCountGet(Format format, uint32_t height);

	// ================================================================
	// バッファ
	// ================================================================
	enum class Usage : uint8_t {
		Default,     // GPU 読み書き。UpdateSubresource 相当で更新可
		Immutable,   // 生成時のデータのみ
		Dynamic,     // CPU から毎フレーム書き換える（Map は WRITE_DISCARD）
	};

	enum BindFlag : uint32_t {
		Bind_None = 0,
		Bind_VertexBuffer = 1 << 0,
		Bind_IndexBuffer = 1 << 1,
		Bind_ConstantBuffer = 1 << 2,
		Bind_ShaderResource = 1 << 3,
		Bind_RenderTarget = 1 << 4,
		Bind_DepthStencil = 1 << 5,
	};

	struct BufferDesc {
		uint32_t i_Size = 0;
		uint32_t i_BindFlags = Bind_None;
		Usage    e_Usage = Usage::Default;
		uint32_t i_Stride = 0;       // 情報用（頂点バッファのストライドなど）
	};

	enum class IndexFormat : uint8_t { UInt16, UInt32 };

	// ================================================================
	// テクスチャ
	// ================================================================
	struct TextureDesc {
		uint32_t i_Width = 1;
		uint32_t i_Height = 1;
		uint32_t i_MipLevels = 1;
		uint32_t i_ArraySize = 1;
		Format   e_Format = Format::R8G8B8A8_UNorm;
		uint32_t i_BindFlags = Bind_ShaderResource;   // ShaderResource / RenderTarget / DepthStencil
		Usage    e_Usage = Usage::Default;
		float    f_ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };   // 最適化ヒント (D3D12)
		float    f_ClearDepth = 1.0f;
		const char* c_DebugName = nullptr;
	};

	// 初期データ（i_MipLevels * i_ArraySize 個。D3D11 のサブリソース順:
	//   配列要素 0 のミップ 0, 1, ... → 配列要素 1 のミップ 0, 1, ...）
	struct SubresourceData {
		const void* p_Data = nullptr;
		uint32_t i_RowPitch = 0;
		uint32_t i_SlicePitch = 0;
	};

	// ================================================================
	// サンプラー
	// ================================================================
	enum class Filter : uint8_t {
		Point,
		Linear,
		MinMagLinearMipPoint,
		Anisotropic,
		ComparisonPoint,
		ComparisonLinear,              // シャドウマップ (PCF)
		ComparisonMinMagLinearMipPoint,
	};
	enum class AddressMode : uint8_t { Wrap, Mirror, Clamp, Border };
	enum class CompareFunc : uint8_t { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };

	struct SamplerDesc {
		Filter      e_Filter = Filter::Linear;
		AddressMode e_AddressU = AddressMode::Wrap;
		AddressMode e_AddressV = AddressMode::Wrap;
		AddressMode e_AddressW = AddressMode::Wrap;
		float       f_MipLodBias = 0.0f;
		uint32_t    i_MaxAnisotropy = 1;
		CompareFunc e_Compare = CompareFunc::Never;
		float       f_BorderColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		float       f_MinLod = 0.0f;
		float       f_MaxLod = 3.402823466e+38f;
	};

	// ================================================================
	// パイプラインステート
	// ================================================================
	enum class Blend : uint8_t {
		Zero, One,
		SrcColor, InvSrcColor, SrcAlpha, InvSrcAlpha,
		DestAlpha, InvDestAlpha, DestColor, InvDestColor,
		SrcAlphaSat, BlendFactor, InvBlendFactor,
	};
	enum class BlendOp : uint8_t { Add, Subtract, RevSubtract, Min, Max };
	enum ColorWrite : uint8_t {
		ColorWrite_R = 1, ColorWrite_G = 2, ColorWrite_B = 4, ColorWrite_A = 8, ColorWrite_All = 15,
	};

	struct RenderTargetBlendDesc {
		bool    b_BlendEnable = false;
		Blend   e_SrcBlend = Blend::One;
		Blend   e_DestBlend = Blend::Zero;
		BlendOp e_BlendOp = BlendOp::Add;
		Blend   e_SrcBlendAlpha = Blend::One;
		Blend   e_DestBlendAlpha = Blend::Zero;
		BlendOp e_BlendOpAlpha = BlendOp::Add;
		uint8_t i_WriteMask = ColorWrite_All;
	};

	constexpr uint32_t MAX_RENDER_TARGETS = 8;

	struct BlendDesc {
		bool b_AlphaToCoverage = false;
		bool b_IndependentBlend = false;
		RenderTargetBlendDesc renderTarget[MAX_RENDER_TARGETS];
	};

	enum class StencilOp : uint8_t { Keep, Zero, Replace, IncrSat, DecrSat, Invert, Incr, Decr };
	struct StencilFaceDesc {
		StencilOp   e_FailOp = StencilOp::Keep;
		StencilOp   e_DepthFailOp = StencilOp::Keep;
		StencilOp   e_PassOp = StencilOp::Keep;
		CompareFunc e_Func = CompareFunc::Always;
	};

	struct DepthStencilDesc {
		bool        b_DepthEnable = true;
		bool        b_DepthWrite = true;
		CompareFunc e_DepthFunc = CompareFunc::Less;
		bool        b_StencilEnable = false;
		uint8_t     i_StencilReadMask = 0xFF;
		uint8_t     i_StencilWriteMask = 0xFF;
		StencilFaceDesc frontFace;
		StencilFaceDesc backFace;
	};

	enum class FillMode : uint8_t { Solid, Wireframe };
	enum class CullMode : uint8_t { None, Front, Back };

	struct RasterizerDesc {
		FillMode e_Fill = FillMode::Solid;
		CullMode e_Cull = CullMode::Back;
		bool     b_FrontCounterClockwise = false;
		int32_t  i_DepthBias = 0;
		float    f_DepthBiasClamp = 0.0f;
		float    f_SlopeScaledDepthBias = 0.0f;
		bool     b_DepthClip = true;
		bool     b_Scissor = false;
	};

	enum class Topology : uint8_t { TriangleList, TriangleStrip, LineList, LineStrip, PointList };

	// 頂点レイアウト
	enum class InputRate : uint8_t { PerVertex, PerInstance };
	constexpr uint32_t APPEND_ALIGNED = 0xFFFFFFFFu;
	struct InputElement {
		const char* c_Semantic = "POSITION";
		uint32_t    i_SemanticIndex = 0;
		Format      e_Format = Format::R32G32B32_Float;
		uint32_t    i_Slot = 0;
		uint32_t    i_Offset = APPEND_ALIGNED;
		InputRate   e_Rate = InputRate::PerVertex;
		uint32_t    i_InstanceStep = 0;
	};

	// ================================================================
	// シェーダー
	// ================================================================
	enum class ShaderStage : uint8_t { Vertex = 0, Pixel = 1, Count = 2 };

	// 反射情報（ShaderManager が定数バッファの変数名からオフセットを引くのに使う）
	struct ShaderVariable {
		std::string name;
		uint32_t i_Offset = 0;
		uint32_t i_Size = 0;
	};
	struct ShaderConstantBuffer {
		std::string name;
		uint32_t i_Slot = 0;
		uint32_t i_Size = 0;
		std::vector<ShaderVariable> variables;
	};
	struct ShaderResourceBinding {
		std::string name;
		uint32_t i_Slot = 0;
	};
	struct ShaderInputAttribute {
		std::string semantic;          // 大文字・末尾の数字を除いたもの（"TEXCOORD"）
		uint32_t i_SemanticIndex = 0;
		uint32_t i_Location = 0;       // Vulkan の location
	};
	struct ShaderReflection {
		std::vector<ShaderConstantBuffer> constantBuffers;
		std::vector<ShaderResourceBinding> textures;
		std::vector<ShaderResourceBinding> samplers;
		std::vector<ShaderInputAttribute> inputs;    // VS 入力 / PS 入力
		std::vector<ShaderInputAttribute> outputs;   // VS 出力
	};

	// ================================================================
	// ビューポート・矩形
	// ================================================================
	struct Viewport {
		float f_X = 0.0f, f_Y = 0.0f;
		float f_Width = 0.0f, f_Height = 0.0f;
		float f_MinDepth = 0.0f, f_MaxDepth = 1.0f;
	};
	struct Rect {
		int32_t i_Left = 0, i_Top = 0, i_Right = 0, i_Bottom = 0;
	};

	// ================================================================
	// デバイス生成
	// ================================================================
	enum class Backend : uint8_t { D3D11 = 0, D3D12 = 1, Vulkan = 2 };
	const char* BackendNameGet(Backend backend);

	struct DeviceCreateInfo {
		void*    p_WindowHandle = nullptr;    // HWND。nullptr ならスワップチェーン無し（オフスクリーン）
		uint32_t i_Width = 1280;
		uint32_t i_Height = 720;
		bool     b_Fullscreen = false;
		bool     b_VSync = true;
		bool     b_Debug = false;             // 検証レイヤ / デバッグレイヤ
		uint32_t i_FramesInFlight = 2;
	};

	// ログ出力先（既定は OutputDebugString / stderr）
	using LogCallback = void(*)(const char* message);
	void LogCallbackSet(LogCallback callback);
	void LogWrite(const char* format, ...);
}

#endif // !RHI_TYPES_H
