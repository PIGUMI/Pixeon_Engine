/*
* ファイル名　ShaderCompiler
* 説　　　明　外部シェーダーコンパイラの呼び出し
*             HLSL → SPIR-V (Vulkan 用) は glslangValidator を使う
*             （Vulkan SDK に同梱。環境変数 VULKAN_SDK または PATH から探す）
*             DXBC (.cso) はエディタ起動時に ShaderManager が d3dcompiler で生成する
*/
#ifndef SHADER_COMPILER_H
#define SHADER_COMPILER_H

#include "ShaderBindings.h"
#include <string>
#include <vector>

namespace ShaderCompiler {
	// ファイル名の接頭辞 (VS_ / PS_) からステージを判定する。判定できなければ false
	bool StageFromName(const std::string& shader_name, ShaderBindings::Stage& out_stage);

	// glslangValidator を探す（見つからなければ空文字）
	std::string SpirvCompilerFind();

	// HLSL を SPIR-V へコンパイルする
	bool SpirvCompile(const std::string& compiler_path, const std::string& hlsl_path,
		ShaderBindings::Stage stage, const std::string& out_spv_path, std::string& out_log);

	// コマンドを実行し、標準出力と標準エラーを取得する
	bool ProcessRun(const std::vector<std::string>& args, std::string& out_log, int& out_exit_code);
}

#endif // !SHADER_COMPILER_H
