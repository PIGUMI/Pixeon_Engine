/*
* ファイル名　FileUtil
* 説　　　明　プラットフォーム差を吸収するファイル操作ユーティリティ
*             エンジンは MultiByte 文字セットのため、std::string のパスは
*             UTF-8 として正しければ UTF-8、そうでなければ ANSI(CP932) として解釈する
*/
#ifndef FILE_UTIL_H
#define FILE_UTIL_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace FileUtil {
	// std::string のパスを OS ネイティブのパスへ変換する
	std::filesystem::path PathToNative(const std::string& path);

	// ネイティブパスを UTF-8 文字列へ変換する
	std::string PathToUtf8(const std::filesystem::path& path);

	// ファイル全体の読み込み / 書き込み
	bool FileReadAll(const std::string& path, std::vector<uint8_t>& out_data);
	bool FileWriteAll(const std::string& path, const void* data, size_t size);
	bool TextReadAll(const std::string& path, std::string& out_text);
}

#endif // !FILE_UTIL_H
