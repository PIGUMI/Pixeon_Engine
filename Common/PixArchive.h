/*
* ファイル名　PixArchive
* 説　　　明　アセットアーカイブ (PIXPAK) の形式定義と読み書きクラス
*             エンジン・パッキングツール・ビルドツールで共有する
*
* ファイル構造
*   [PakHeader 64byte][ファイルデータ (alignment 境界)]...[TOC]
*
* TOC エントリ
*   v1 : nameLen(u16) name compression(u8) reserved(3) crc32 originalSize storedSize offset
*   v2 : v1 + sha256[32]
*   v3 : nameLen(u16) name compression(u8) assetType(u8) reserved(2) crc32
*        originalSize storedSize offset sha256[32]
*        ヘッダに TOC サイズと TOC の CRC32 を保持し、破損を検出できる
*/
#ifndef PIX_ARCHIVE_H
#define PIX_ARCHIVE_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace PixArchive {

	constexpr char     MAGIC[8] = { 'P','I','X','P','A','K','\0','\0' };
	constexpr uint32_t VERSION_LATEST = 3;
	constexpr uint32_t ALIGNMENT_DEFAULT = 16;

#pragma pack(push, 1)
	struct PakHeader {
		char     magic[8];       // "PIXPAK\0\0"
		uint32_t version;        // 1 / 2 / 3
		uint32_t fileCount;
		uint64_t tocOffset;
		uint32_t flags;          // 予約 (0)
		uint32_t alignment;      // 16 / 4096 など
		uint64_t tocSize;        // v3: TOC のバイト数
		uint32_t tocCrc32;       // v3: TOC の CRC32
		uint8_t  reserved[20];   // 0
	};
#pragma pack(pop)
	static_assert(sizeof(PakHeader) == 64, "PakHeader は 64byte 固定");

	// 圧縮種別
	enum class Compression : uint8_t {
		None = 0,
	};

	// TOC エントリ（メモリ上の表現）
	struct Entry {
		std::string path;                 // 仮想パス（'/' 区切り・大文字小文字は登録時のまま）
		uint8_t  compression = 0;
		uint8_t  assetType = 0;           // AssetType の値
		uint32_t crc32 = 0;
		uint64_t originalSize = 0;
		uint64_t storedSize = 0;
		uint64_t offset = 0;
		std::array<uint8_t, 32> sha256{};
	};

	// 仮想パスの区切りを整える（'\' → '/'、先頭の "./" と "/" と重複 '/' を除去）
	std::string PathClean(const std::string& path);

	// 検索キー用に正規化する（PathClean + 小文字化）。検索は大文字小文字を区別しない
	std::string PathNormalize(const std::string& path);

	// ================================================================
	// Reader: アーカイブを開き、必要なファイルだけをオンデマンドで読む
	// ================================================================
	class Reader {
	public:
		Reader() = default;
		~Reader();
		Reader(const Reader&) = delete;
		Reader& operator=(const Reader&) = delete;

		bool Open(const std::string& file_path);
		void Close();
		bool IsOpen() const { return m_cls_File != nullptr; }

		bool FileExists(const std::string& path) const;
		bool FileRead(const std::string& path, std::vector<uint8_t>& out_data);
		const Entry* EntryFind(const std::string& path) const;
		std::vector<std::string> FileListGet(const std::string& prefix = "") const;

		void SetVerifyCrc(bool verify) { m_b_VerifyCrc = verify; }
		uint32_t GetVersion() const { return m_str_Header.version; }
		size_t GetFileCount() const { return m_Entries.size(); }
		const std::string& GetLastError() const { return m_LastError; }

	private:
		bool HeaderRead();
		bool TocRead();
		bool TocEntryParse(const uint8_t*& cursor, const uint8_t* end, Entry& out_entry) const;

		FILE* m_cls_File = nullptr;
		PakHeader m_str_Header{};
		std::vector<Entry> m_Entries;
		std::unordered_map<std::string, size_t> m_IndexByPath;
		bool m_b_VerifyCrc = true;
		std::string m_LastError;
		mutable std::mutex m_Mutex;
	};

	// ================================================================
	// Writer: 仮想パスとディスク上のファイルを登録し、アーカイブを書き出す
	// ================================================================
	class Writer {
	public:
		using ProgressCallback = std::function<void(size_t done, size_t total, const std::string& path)>;

		void FileAdd(const std::string& virtual_path, const std::string& disk_path, uint8_t asset_type = 0);
		void MemoryAdd(const std::string& virtual_path, std::vector<uint8_t> data, uint8_t asset_type = 0);
		size_t GetFileCount() const { return m_Sources.size(); }

		void SetAlignment(uint32_t alignment) { m_i_Alignment = alignment ? alignment : 1; }
		bool ArchiveWrite(const std::string& out_path, const ProgressCallback& progress = nullptr);
		const std::string& GetLastError() const { return m_LastError; }

	private:
		struct Source {
			std::string virtualPath;
			std::string diskPath;           // 空なら memory を使用
			std::vector<uint8_t> memory;
			uint8_t assetType = 0;
		};
		bool SourceLoad(const Source& src, std::vector<uint8_t>& out_data);
		static void TocEntryWrite(std::vector<uint8_t>& toc, const Entry& entry);

		std::vector<Source> m_Sources;
		uint32_t m_i_Alignment = ALIGNMENT_DEFAULT;
		std::string m_LastError;
	};
}

#endif // !PIX_ARCHIVE_H
