/*
* ファイル名　FileUtil
* 説　　　明　ファイル操作ユーティリティ実装
*/
#include "FileUtil.h"
#include <fstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace FileUtil {

	/*
	* 関数名　PathToNative
	* 引　数　path：パス文字列（UTF-8 または ANSI）
	* 戻り値　std::filesystem::path
	* 説　明　Windows では UTF-8 として不正なら ANSI として変換する
	*/
	std::filesystem::path PathToNative(const std::string& path)
	{
#if defined(_WIN32)
		if (path.empty()) return {};
		UINT code_page = CP_UTF8;
		int len = MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, path.c_str(), (int)path.size(), nullptr, 0);
		if (len <= 0) {
			code_page = CP_ACP;
			len = MultiByteToWideChar(code_page, 0, path.c_str(), (int)path.size(), nullptr, 0);
		}
		std::wstring wide((size_t)len, L'\0');
		MultiByteToWideChar(code_page, 0, path.c_str(), (int)path.size(), wide.data(), len);
		return std::filesystem::path(wide);
#else
		return std::filesystem::path(path);
#endif
	}

	/*
	* 関数名　PathToUtf8
	* 引　数　path：ネイティブパス
	* 戻り値　UTF-8 文字列
	* 説　明　ネイティブパスを UTF-8 文字列に変換する
	*/
	std::string PathToUtf8(const std::filesystem::path& path)
	{
		auto u8 = path.generic_u8string();
		return std::string(u8.begin(), u8.end());
	}

	/*
	* 関数名　FileReadAll
	* 引　数　path：パス　out_data：読み込み先
	* 戻り値　成功なら true
	* 説　明　バイナリファイルを丸ごと読み込む
	*/
	bool FileReadAll(const std::string& path, std::vector<uint8_t>& out_data)
	{
		std::ifstream ifs(PathToNative(path), std::ios::binary | std::ios::ate);
		if (!ifs) return false;
		const std::streamoff size = ifs.tellg();
		if (size < 0) return false;
		ifs.seekg(0, std::ios::beg);
		out_data.resize((size_t)size);
		if (size > 0 && !ifs.read((char*)out_data.data(), size)) return false;
		return true;
	}

	/*
	* 関数名　FileWriteAll
	* 引　数　path：パス　data：内容　size：サイズ
	* 戻り値　成功なら true
	* 説　明　バイナリファイルを書き込む（親ディレクトリは自動生成）
	*/
	bool FileWriteAll(const std::string& path, const void* data, size_t size)
	{
		const auto native = PathToNative(path);
		std::error_code ec;
		if (native.has_parent_path()) std::filesystem::create_directories(native.parent_path(), ec);
		std::ofstream ofs(native, std::ios::binary | std::ios::trunc);
		if (!ofs) return false;
		if (size > 0) ofs.write((const char*)data, (std::streamsize)size);
		return (bool)ofs;
	}

	/*
	* 関数名　TextReadAll
	* 引　数　path：パス　out_text：読み込み先
	* 戻り値　成功なら true
	* 説　明　テキストファイルを丸ごと読み込む（UTF-8 BOM は除去）
	*/
	bool TextReadAll(const std::string& path, std::string& out_text)
	{
		std::vector<uint8_t> data;
		if (!FileReadAll(path, data)) return false;
		size_t start = (data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) ? 3 : 0;
		out_text.assign((const char*)data.data() + start, data.size() - start);
		return true;
	}
}
