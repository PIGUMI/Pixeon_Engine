/*
* ファイル名　SpirvReflect
* 説　　　明　SPIR-V の最小限の反射と書き換え
*             - 定数バッファ（変数名・オフセット・サイズ）、テクスチャ、サンプラーのバインド位置
*             - 入出力変数の location と HLSL セマンティクス (SPV_GOOGLE_hlsl_functionality1)
*             - PS 入力の location を VS 出力のセマンティクスに合わせて書き換える
*               （D3D はセマンティクス名で VS と PS を接続するが、SPIR-V は location で接続するため）
*/
#ifndef RHI_SPIRV_REFLECT_H
#define RHI_SPIRV_REFLECT_H

#include "../RHITypes.h"
#include <cstdint>
#include <vector>

namespace RHI {
	// SPIR-V を解析して反射情報を作る。stage は binding 番号からスロットを求めるのに使う
	bool SpirvReflect(const uint32_t* code, size_t word_count, ShaderStage stage, ShaderReflection& out_reflection);

	// PS の入力 location を VS 出力のセマンティクスに一致させた SPIR-V を返す
	std::vector<uint32_t> SpirvInputLocationsRemap(const std::vector<uint32_t>& pixel_code,
		const std::vector<ShaderInputAttribute>& vertex_outputs);

	// 反射用の HLSL セマンティクス情報 (SPV_GOOGLE_hlsl_functionality1) を取り除く
	// この拡張は VK_GOOGLE_hlsl_functionality1 が無いデバイスでは使えないため、モジュール作成前に外す
	std::vector<uint32_t> SpirvSemanticsStrip(const std::vector<uint32_t>& code);

	// "TEXCOORD3" → ("TEXCOORD", 3) のように分解する（大文字化）
	void SemanticSplit(const std::string& semantic, std::string& out_name, uint32_t& out_index);
}

#endif // !RHI_SPIRV_REFLECT_H
