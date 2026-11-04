/*
* ファイル名　TextureLoader
* 説　　　明　画像データから RHI テクスチャを生成する
*/
#include "TextureLoader.h"
#include "System.h"
#include "RHI/DXGI/DXGICommon.h"
#include "DirectXTex/DirectXTex.h"

#if defined(_MSC_VER)
#if defined(_M_IX86)
#ifdef _DEBUG
#pragma comment(lib, "DirectXTex/Win32/Debug/DirectXTex.lib")
#else
#pragma comment(lib, "DirectXTex/Win32/Release/DirectXTex.lib")
#endif
#else
#ifdef _DEBUG
#pragma comment(lib, "DirectXTex/x64/Debug/DirectXTex.lib")
#else
#pragma comment(lib, "DirectXTex/x64/Release/DirectXTex.lib")
#endif
#endif
#endif

namespace TextureLoader {
	namespace {
		/*
		* 関数名　ImageDecode
		* 引　数　data / size：画像ファイル　extension：拡張子　out_image：出力
		* 戻り値　HRESULT
		* 説　明　拡張子に応じたデコーダを使い、不明なら WIC → TGA → DDS の順に試す
		*/
		HRESULT ImageDecode(const uint8_t* data, size_t size, const std::string& extension, DirectX::ScratchImage& out_image)
		{
			if (extension == "dds") return DirectX::LoadFromDDSMemory(data, size, DirectX::DDS_FLAGS_NONE, nullptr, out_image);
			if (extension == "tga") return DirectX::LoadFromTGAMemory(data, size, nullptr, out_image);
			if (extension == "hdr") return DirectX::LoadFromHDRMemory(data, size, nullptr, out_image);
			HRESULT hr = DirectX::LoadFromWICMemory(data, size, DirectX::WIC_FLAGS_NONE, nullptr, out_image);
			if (FAILED(hr) && extension.empty()) hr = DirectX::LoadFromTGAMemory(data, size, nullptr, out_image);
			if (FAILED(hr) && extension.empty()) hr = DirectX::LoadFromDDSMemory(data, size, DirectX::DDS_FLAGS_NONE, nullptr, out_image);
			return hr;
		}
	}

	/*
	* 関数名　FromScratchImage
	* 引　数　image：DirectXTex の画像　out_gpu_bytes：GPU 使用量の概算
	* 戻り値　テクスチャ（失敗時 nullptr）
	* 説　明　2D テクスチャ（配列含む）を全ミップごと転送する
	*/
	RHI::TextureRef FromScratchImage(const DirectX::ScratchImage& image, size_t* out_gpu_bytes)
	{
		RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
		if (!dev || image.GetImageCount() == 0) return nullptr;

		const DirectX::ScratchImage* src = &image;
		DirectX::ScratchImage converted;
		RHI::Format format = RHI::FormatFromDxgi(image.GetMetadata().format);
		if (format == RHI::Format::Unknown || RHI::FormatInfoGet(format).b_Depth) {
			const auto& meta = image.GetMetadata();
			DirectX::ScratchImage decompressed;
			if (DirectX::IsCompressed(meta.format)) {
				if (FAILED(DirectX::Decompress(image.GetImages(), image.GetImageCount(), meta, DXGI_FORMAT_R8G8B8A8_UNORM, decompressed))) return nullptr;
				src = &decompressed;
			}
			if (FAILED(DirectX::Convert(src->GetImages(), src->GetImageCount(), src->GetMetadata(), DXGI_FORMAT_R8G8B8A8_UNORM,
				DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted))) return nullptr;
			src = &converted;
			format = RHI::Format::R8G8B8A8_UNorm;
		}

		const auto& meta = src->GetMetadata();
		RHI::TextureDesc desc;
		desc.i_Width = (uint32_t)meta.width;
		desc.i_Height = (uint32_t)meta.height;
		desc.i_MipLevels = (uint32_t)meta.mipLevels;
		desc.i_ArraySize = (uint32_t)meta.arraySize;
		desc.e_Format = format;
		desc.i_BindFlags = RHI::Bind_ShaderResource;
		desc.e_Usage = RHI::Usage::Immutable;

		std::vector<RHI::SubresourceData> subresources;
		size_t bytes = 0;
		for (size_t item = 0; item < meta.arraySize; ++item) {
			for (size_t mip = 0; mip < meta.mipLevels; ++mip) {
				const DirectX::Image* img = src->GetImage(mip, item, 0);
				if (!img) return nullptr;
				subresources.push_back({ img->pixels, (uint32_t)img->rowPitch, (uint32_t)img->slicePitch });
				bytes += img->slicePitch;
			}
		}
		if (out_gpu_bytes) *out_gpu_bytes = bytes;
		return dev->TextureCreate(desc, subresources.data());
	}

	/*
	* 関数名　FromMemory
	* 引　数　data / size：画像ファイル　extension：拡張子　generate_mips：ミップを生成するか
	* 戻り値　生成結果（失敗時は error に理由）
	* 説　明　画像をデコードし、必要ならミップマップを生成してテクスチャを作る
	*/
	Result FromMemory(const void* data, size_t size, const std::string& extension, bool generate_mips)
	{
		Result result;
		DirectX::ScratchImage img;
		const HRESULT hr = ImageDecode((const uint8_t*)data, size, extension, img);
		if (FAILED(hr)) {
			char buf[128];
			sprintf_s(buf, "DecodeFailed hr=0x%08X ext=%s size=%zu", (unsigned)hr, extension.c_str(), size);
			result.error = buf;
			return result;
		}
		if (generate_mips && img.GetMetadata().mipLevels <= 1 && !DirectX::IsCompressed(img.GetMetadata().format)) {
			DirectX::ScratchImage mip;
			if (SUCCEEDED(DirectX::GenerateMipMaps(img.GetImages(), img.GetImageCount(), img.GetMetadata(),
				DirectX::TEX_FILTER_DEFAULT, 0, mip))) {
				img = std::move(mip);
			}
		}
		result.texture = FromScratchImage(img, &result.i_GpuBytes);
		result.i_Width = (uint32_t)img.GetMetadata().width;
		result.i_Height = (uint32_t)img.GetMetadata().height;
		if (!result.texture) result.error = "TextureCreateFailed";
		return result;
	}

	RHI::TextureRef FromRGBA8(const void* pixels, uint32_t width, uint32_t height)
	{
		RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
		if (!dev || !pixels) return nullptr;
		RHI::TextureDesc desc;
		desc.i_Width = width;
		desc.i_Height = height;
		desc.e_Format = RHI::Format::R8G8B8A8_UNorm;
		desc.e_Usage = RHI::Usage::Immutable;
		const RHI::SubresourceData init{ pixels, width * 4, width * height * 4 };
		return dev->TextureCreate(desc, &init);
	}
}
