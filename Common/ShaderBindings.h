/*
* ファイル名　ShaderBindings
* 説　　　明　HLSL のレジスタと各 API のバインド位置の対応表
*
*   D3D11 : ステージごとに b# / t# / s# がそのまま使われる
*   D3D12 : ルートシグネチャをステージ別に分けて同じ番号を使う
*   Vulkan: 1 つのディスクリプタセットに、ステージ別のオフセットを足した binding で並べる
*           HLSL → SPIR-V 変換時に glslang の --shift-*-binding でオフセットを与える
*
* この表をシェーダーコンパイラ（ビルド / エディタ）と Vulkan バックエンドの両方が参照する
*/
#ifndef SHADER_BINDINGS_H
#define SHADER_BINDINGS_H

#include <cstdint>

namespace ShaderBindings {
	// 1 ステージあたりのスロット数
	constexpr uint32_t CB_SLOT_COUNT = 8;        // b0 - b7
	constexpr uint32_t SRV_SLOT_COUNT = 16;      // t0 - t15
	constexpr uint32_t SAMPLER_SLOT_COUNT = 4;   // s0 - s3

	// ステージ
	enum class Stage : uint32_t { Vertex = 0, Pixel = 1, Count = 2 };
	constexpr uint32_t STAGE_COUNT = (uint32_t)Stage::Count;

	// Vulkan の binding 番号
	constexpr uint32_t STAGE_STRIDE = 32;
	constexpr uint32_t CB_OFFSET = 0;
	constexpr uint32_t SRV_OFFSET = CB_OFFSET + CB_SLOT_COUNT;          // 8
	constexpr uint32_t SAMPLER_OFFSET = SRV_OFFSET + SRV_SLOT_COUNT;    // 24
	static_assert(SAMPLER_OFFSET + SAMPLER_SLOT_COUNT <= STAGE_STRIDE, "ステージ内のスロットが溢れています");

	constexpr uint32_t StageBase(Stage stage) { return (uint32_t)stage * STAGE_STRIDE; }
	constexpr uint32_t VulkanCbBinding(Stage stage, uint32_t slot) { return StageBase(stage) + CB_OFFSET + slot; }
	constexpr uint32_t VulkanSrvBinding(Stage stage, uint32_t slot) { return StageBase(stage) + SRV_OFFSET + slot; }
	constexpr uint32_t VulkanSamplerBinding(Stage stage, uint32_t slot) { return StageBase(stage) + SAMPLER_OFFSET + slot; }
}

#endif // !SHADER_BINDINGS_H
