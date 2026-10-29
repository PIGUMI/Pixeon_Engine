/*
* ファイル名　RHICommon
* 説　　　明　RHI のバックエンド非依存部分（フォーマット表・ログ・生成の振り分け）
*
* ビルドに含めるバックエンドはマクロで選ぶ
*   PIXEON_RHI_D3D11 / PIXEON_RHI_D3D12 / PIXEON_RHI_VULKAN
*/
#include "RHI.h"
#include <cstdarg>
#include <cstdio>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace RHI {

#if defined(PIXEON_RHI_D3D11)
	std::unique_ptr<Device> D3D11DeviceCreate(const DeviceCreateInfo& info, std::string* out_error);
	std::unique_ptr<ImGuiRenderer> D3D11ImGuiRendererCreate(Device* device);
#endif
#if defined(PIXEON_RHI_D3D12)
	std::unique_ptr<Device> D3D12DeviceCreate(const DeviceCreateInfo& info, std::string* out_error);
	std::unique_ptr<ImGuiRenderer> D3D12ImGuiRendererCreate(Device* device);
#endif
#if defined(PIXEON_RHI_VULKAN)
	std::unique_ptr<Device> VulkanDeviceCreate(const DeviceCreateInfo& info, std::string* out_error);
#if !defined(PIXEON_RHI_NO_IMGUI)
	std::unique_ptr<ImGuiRenderer> VulkanImGuiRendererCreate(Device* device);
#endif
#endif

	namespace {
		Device* g_cls_CurrentDevice = nullptr;
		LogCallback g_LogCallback = nullptr;

		struct FormatEntry {
			FormatInfo info;
			const char* name;
		};
		// Format の並び順と一致させること
		const FormatEntry FORMAT_TABLE[] = {
			{ { 0, 1, false, false, false }, "Unknown" },
			{ { 1, 1, false, false, false }, "R8_UNorm" },
			{ { 4, 1, false, false, false }, "R8G8B8A8_UNorm" },
			{ { 4, 1, false, false, false }, "R8G8B8A8_UNorm_sRGB" },
			{ { 4, 1, false, false, false }, "B8G8R8A8_UNorm" },
			{ { 4, 1, false, false, false }, "B8G8R8A8_UNorm_sRGB" },
			{ { 2, 1, false, false, false }, "R16_Float" },
			{ { 4, 1, false, false, false }, "R16G16_Float" },
			{ { 8, 1, false, false, false }, "R16G16B16A16_Float" },
			{ { 4, 1, false, false, false }, "R32_Float" },
			{ { 8, 1, false, false, false }, "R32G32_Float" },
			{ { 12, 1, false, false, false }, "R32G32B32_Float" },
			{ { 16, 1, false, false, false }, "R32G32B32A32_Float" },
			{ { 2, 1, false, false, false }, "R16_UInt" },
			{ { 4, 1, false, false, false }, "R32_UInt" },
			{ { 16, 1, false, false, false }, "R32G32B32A32_UInt" },
			{ { 4, 1, true, true, false }, "D24_UNorm_S8_UInt" },
			{ { 4, 1, true, false, false }, "D32_Float" },
			{ { 8, 4, false, false, true }, "BC1_UNorm" },
			{ { 8, 4, false, false, true }, "BC1_UNorm_sRGB" },
			{ { 16, 4, false, false, true }, "BC2_UNorm" },
			{ { 16, 4, false, false, true }, "BC2_UNorm_sRGB" },
			{ { 16, 4, false, false, true }, "BC3_UNorm" },
			{ { 16, 4, false, false, true }, "BC3_UNorm_sRGB" },
			{ { 8, 4, false, false, true }, "BC4_UNorm" },
			{ { 16, 4, false, false, true }, "BC5_UNorm" },
			{ { 16, 4, false, false, true }, "BC6H_UF16" },
			{ { 16, 4, false, false, true }, "BC7_UNorm" },
			{ { 16, 4, false, false, true }, "BC7_UNorm_sRGB" },
		};
		static_assert(sizeof(FORMAT_TABLE) / sizeof(FORMAT_TABLE[0]) == (size_t)Format::Count, "FORMAT_TABLE と Format の数が一致しません");
	}

	const FormatInfo& FormatInfoGet(Format format)
	{
		const size_t i = (size_t)format < (size_t)Format::Count ? (size_t)format : 0;
		return FORMAT_TABLE[i].info;
	}

	const char* FormatNameGet(Format format)
	{
		const size_t i = (size_t)format < (size_t)Format::Count ? (size_t)format : 0;
		return FORMAT_TABLE[i].name;
	}

	/*
	* 関数名　FormatRowPitchGet
	* 引　数　format：フォーマット　width：幅（ピクセル）
	* 戻り値　1 行（圧縮形式は 1 ブロック行）のバイト数
	* 説　明　アップロード時の行ピッチ計算に使う
	*/
	uint32_t FormatRowPitchGet(Format format, uint32_t width)
	{
		const FormatInfo& f = FormatInfoGet(format);
		const uint32_t blocks = (width + f.i_BlockWidth - 1) / f.i_BlockWidth;
		return blocks * f.i_BlockSize;
	}

	uint32_t FormatRowCountGet(Format format, uint32_t height)
	{
		const FormatInfo& f = FormatInfoGet(format);
		return (height + f.i_BlockWidth - 1) / f.i_BlockWidth;
	}

	const char* BackendNameGet(Backend backend)
	{
		switch (backend) {
		case Backend::D3D12:  return "DirectX 12";
		case Backend::Vulkan: return "Vulkan";
		default:              return "DirectX 11";
		}
	}

	void LogCallbackSet(LogCallback callback)
	{
		g_LogCallback = callback;
	}

	/*
	* 関数名　LogWrite
	* 引　数　format：printf 形式
	* 戻り値　なし
	* 説　明　RHI 内部のログ。コールバック未設定なら OutputDebugString / stderr へ出す
	*/
	void LogWrite(const char* format, ...)
	{
		char buffer[2048];
		va_list args;
		va_start(args, format);
		std::vsnprintf(buffer, sizeof(buffer), format, args);
		va_end(args);
		if (g_LogCallback) {
			g_LogCallback(buffer);
			return;
		}
#if defined(_WIN32)
		OutputDebugStringA("[RHI] ");
		OutputDebugStringA(buffer);
		OutputDebugStringA("\n");
#else
		std::fprintf(stderr, "[RHI] %s\n", buffer);
#endif
	}

	/*
	* 関数名　InputLayout
	* 引　数　elements：頂点要素　count：要素数
	* 戻り値　なし
	* 説　明　セマンティクス文字列を自前で保持し、APPEND_ALIGNED のオフセットを確定させる
	*/
	InputLayout::InputLayout(const InputElement* elements, uint32_t count)
	{
		m_SemanticStorage.reserve(count);
		uint32_t offsets[16] = {};
		for (uint32_t i = 0; i < count; ++i) {
			InputElement e = elements[i];
			m_SemanticStorage.emplace_back(e.c_Semantic ? e.c_Semantic : "");
			const uint32_t slot = e.i_Slot < 16 ? e.i_Slot : 15;
			if (e.i_Offset == APPEND_ALIGNED) e.i_Offset = offsets[slot];
			offsets[slot] = e.i_Offset + FormatInfoGet(e.e_Format).i_BlockSize;
			m_Elements.push_back(e);
		}
		for (uint32_t i = 0; i < count; ++i) m_Elements[i].c_Semantic = m_SemanticStorage[i].c_str();
	}

	Device* DeviceGet()
	{
		return g_cls_CurrentDevice;
	}

	void DeviceSet(Device* device)
	{
		g_cls_CurrentDevice = device;
	}

	bool IsBackendAvailable(Backend backend)
	{
		switch (backend) {
#if defined(PIXEON_RHI_D3D11)
		case Backend::D3D11: return true;
#endif
#if defined(PIXEON_RHI_D3D12)
		case Backend::D3D12: return true;
#endif
#if defined(PIXEON_RHI_VULKAN)
		case Backend::Vulkan: return true;
#endif
		default: return false;
		}
	}

	/*
	* 関数名　DeviceCreate
	* 引　数　backend：バックエンド　info：生成情報　out_error：エラー内容
	* 戻り値　デバイス（失敗時 nullptr）
	* 説　明　指定バックエンドのデバイスを生成する
	*/
	std::unique_ptr<Device> DeviceCreate(Backend backend, const DeviceCreateInfo& info, std::string* out_error)
	{
		switch (backend) {
#if defined(PIXEON_RHI_D3D11)
		case Backend::D3D11: return D3D11DeviceCreate(info, out_error);
#endif
#if defined(PIXEON_RHI_D3D12)
		case Backend::D3D12: return D3D12DeviceCreate(info, out_error);
#endif
#if defined(PIXEON_RHI_VULKAN)
		case Backend::Vulkan: return VulkanDeviceCreate(info, out_error);
#endif
		default:
			if (out_error) *out_error = std::string(BackendNameGet(backend)) + " はこのビルドに含まれていません";
			return nullptr;
		}
	}

	/*
	* 関数名　ImGuiRendererCreate
	* 引　数　device：デバイス
	* 戻り値　ImGui 描画クラス
	* 説　明　デバイスのバックエンドに対応する ImGui 描画クラスを生成する
	*/
	std::unique_ptr<ImGuiRenderer> ImGuiRendererCreate(Device* device)
	{
		if (!device) return nullptr;
		switch (device->BackendGet()) {
#if defined(PIXEON_RHI_D3D11)
		case Backend::D3D11: return D3D11ImGuiRendererCreate(device);
#endif
#if defined(PIXEON_RHI_D3D12)
		case Backend::D3D12: return D3D12ImGuiRendererCreate(device);
#endif
#if defined(PIXEON_RHI_VULKAN) && !defined(PIXEON_RHI_NO_IMGUI)
		case Backend::Vulkan: return VulkanImGuiRendererCreate(device);
#endif
		default: return nullptr;
		}
	}
}
