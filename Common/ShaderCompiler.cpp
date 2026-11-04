/*
* ファイル名　ShaderCompiler
* 説　　　明　外部シェーダーコンパイラ呼び出しの実装
*/
#include "ShaderCompiler.h"
#include "FileUtil.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace ShaderCompiler {

	/*
	* 関数名　StageFromName
	* 引　数　shader_name：シェーダー名　out_stage：出力
	* 戻り値　判定できたら true
	* 説　明　エンジンの命名規約 (VS_xxx / PS_xxx) からステージを判定する
	*/
	bool StageFromName(const std::string& shader_name, ShaderBindings::Stage& out_stage)
	{
		const std::string name = fs::path(shader_name).filename().string();
		if (name.rfind("VS_", 0) == 0) { out_stage = ShaderBindings::Stage::Vertex; return true; }
		if (name.rfind("PS_", 0) == 0) { out_stage = ShaderBindings::Stage::Pixel; return true; }
		return false;
	}

	/*
	* 関数名　SpirvCompilerFind
	* 引　数　なし
	* 戻り値　glslangValidator のパス（見つからなければ空）
	* 説　明　VULKAN_SDK/Bin → PATH の順に探す
	*/
	std::string SpirvCompilerFind()
	{
#if defined(_WIN32)
		const char* exe_name = "glslangValidator.exe";
		const char sep = ';';
#else
		const char* exe_name = "glslangValidator";
		const char sep = ':';
#endif
		std::error_code ec;
		if (const char* sdk = std::getenv("VULKAN_SDK")) {
			for (const char* bin : { "Bin", "bin" }) {
				fs::path p = fs::path(sdk) / bin / exe_name;
				if (fs::exists(p, ec)) return p.string();
			}
		}
		if (const char* path = std::getenv("PATH")) {
			std::string all = path;
			size_t start = 0;
			while (start <= all.size()) {
				size_t end = all.find(sep, start);
				if (end == std::string::npos) end = all.size();
				const std::string dir = all.substr(start, end - start);
				if (!dir.empty() && fs::exists(fs::path(dir) / exe_name, ec)) return (fs::path(dir) / exe_name).string();
				start = end + 1;
			}
		}
		return "";
	}

	/*
	* 関数名　SpirvCompile
	* 引　数　compiler_path：glslangValidator　hlsl_path：入力　stage：ステージ
	* 　　　　out_spv_path：出力　out_log：コンパイラの出力
	* 戻り値　成功なら true
	* 説　明　ShaderBindings の表に従って binding をずらし、HLSL のパッキング規則で SPIR-V を生成する
	*/
	bool SpirvCompile(const std::string& compiler_path, const std::string& hlsl_path,
		ShaderBindings::Stage stage, const std::string& out_spv_path, std::string& out_log)
	{
		using namespace ShaderBindings;
		std::error_code ec;
		const fs::path out_native = FileUtil::PathToNative(out_spv_path);
		if (out_native.has_parent_path()) fs::create_directories(out_native.parent_path(), ec);

		const std::vector<std::string> args = {
			compiler_path, "-V", "-D", "-e", "main",
			"-S", (stage == Stage::Vertex) ? "vert" : "frag",
			"--hlsl-iomap", "--hlsl-offsets", "-fhlsl_functionality1",
			"--shift-cbuffer-binding", std::to_string(StageBase(stage) + CB_OFFSET),
			"--shift-texture-binding", std::to_string(StageBase(stage) + SRV_OFFSET),
			"--shift-sampler-binding", std::to_string(StageBase(stage) + SAMPLER_OFFSET),
			"-o", out_spv_path, hlsl_path,
		};
		int exit_code = -1;
		if (!ProcessRun(args, out_log, exit_code)) return false;
		return exit_code == 0;
	}

#if defined(_WIN32)
	/*
	* 関数名　ProcessRun (Windows)
	* 引　数　args：コマンドライン　out_log：出力　out_exit_code：終了コード
	* 戻り値　プロセスを起動できたら true
	* 説　明　コンソールウィンドウを出さずに実行し、パイプで出力を回収する
	*/
	bool ProcessRun(const std::vector<std::string>& args, std::string& out_log, int& out_exit_code)
	{
		std::string cmd;
		for (auto& a : args) {
			if (!cmd.empty()) cmd += ' ';
			cmd += "\"" + a + "\"";
		}
		SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
		HANDLE read_pipe = nullptr, write_pipe = nullptr;
		if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) return false;
		SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

		STARTUPINFOA si{};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdOutput = write_pipe;
		si.hStdError = write_pipe;
		PROCESS_INFORMATION pi{};
		std::vector<char> cmd_buf(cmd.begin(), cmd.end());
		cmd_buf.push_back('\0');
		const BOOL ok = CreateProcessA(nullptr, cmd_buf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
		CloseHandle(write_pipe);
		if (!ok) {
			CloseHandle(read_pipe);
			return false;
		}
		char buffer[4096];
		DWORD n = 0;
		while (ReadFile(read_pipe, buffer, sizeof(buffer), &n, nullptr) && n > 0) out_log.append(buffer, n);
		WaitForSingleObject(pi.hProcess, INFINITE);
		DWORD code = 0;
		GetExitCodeProcess(pi.hProcess, &code);
		out_exit_code = (int)code;
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		CloseHandle(read_pipe);
		return true;
	}
#else
	/*
	* 関数名　ProcessRun (POSIX)
	* 引　数　args：コマンドライン　out_log：出力　out_exit_code：終了コード
	* 戻り値　プロセスを起動できたら true
	* 説　明　popen で実行する（引数は単一引用符でエスケープ）
	*/
	bool ProcessRun(const std::vector<std::string>& args, std::string& out_log, int& out_exit_code)
	{
		std::string cmd;
		for (auto& a : args) {
			std::string q = "'";
			for (char c : a) q += (c == '\'') ? std::string("'\\''") : std::string(1, c);
			q += "'";
			if (!cmd.empty()) cmd += ' ';
			cmd += q;
		}
		cmd += " 2>&1";
		FILE* pipe = popen(cmd.c_str(), "r");
		if (!pipe) return false;
		char buffer[4096];
		size_t n = 0;
		while ((n = std::fread(buffer, 1, sizeof(buffer), pipe)) > 0) out_log.append(buffer, n);
		const int status = pclose(pipe);
		out_exit_code = (status >= 0 && (status & 0x7f) == 0) ? ((status >> 8) & 0xff) : -1;
		return true;
	}
#endif
}
