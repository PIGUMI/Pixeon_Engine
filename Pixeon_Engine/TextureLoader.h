/*
* ファイル名　TextureLoader
* 説　　　明　画像データ（PNG / JPG / TGA / DDS / HDR など）を RHI テクスチャへ変換する
*             以前は TextureManager / ModelManager / EffectManager / GUI がそれぞれ
*             DirectXTex で D3D11 の SRV を直接作っていたため、ここに一本化した
*/
#pragma once
#include "RHI/RHI.h"
#include <string>

namespace DirectX { class ScratchImage; }

namespace TextureLoader {
	struct Result {
		RHI::TextureRef texture;
		uint32_t i_Width = 0;
		uint32_t i_Height = 0;
		size_t i_GpuBytes = 0;
		std::string error;
	};

	// 画像ファイルのバイト列から生成する。extension は "png" "dds" など（空なら順に試す）
	Result FromMemory(const void* data, size_t size, const std::string& extension, bool generate_mips);

	// RGBA8 の画素配列から生成する
	RHI::TextureRef FromRGBA8(const void* pixels, uint32_t width, uint32_t height);

	// DirectXTex の画像から生成する（RHI 非対応のフォーマットは RGBA8 に変換）
	RHI::TextureRef FromScratchImage(const DirectX::ScratchImage& image, size_t* out_gpu_bytes = nullptr);
}
