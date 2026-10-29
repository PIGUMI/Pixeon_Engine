/*
* ファイル名　PixArchive
* 説　　　明　アセットアーカイブ (PIXPAK) の読み書き実装
*/
#include "PixArchive.h"
#include "HashUtil.h"
#include "FileUtil.h"
#include <algorithm>
#include <cstring>
#include <fstream>

namespace PixArchive {

	namespace {
		/*
		* 関数名　FileSeek
		* 引　数　file：ファイル　offset：先頭からのオフセット
		* 戻り値　成功なら true
		* 説　明　2GB を超えるアーカイブにも対応したシーク
		*/
		bool FileSeek(FILE* file, uint64_t offset)
		{
#if defined(_WIN32)
			return _fseeki64(file, (long long)offset, SEEK_SET) == 0;
#else
			return fseeko(file, (off_t)offset, SEEK_SET) == 0;
#endif
		}

		/*
		* 関数名　FileOpenUtf8
		* 引　数　path：パス　mode：fopen のモード
		* 戻り値　FILE*（失敗時 nullptr）
		* 説　明　Windows でも日本語パスを開けるようにネイティブパスで開く
		*/
		FILE* FileOpenUtf8(const std::string& path, const char* mode)
		{
#if defined(_WIN32)
			std::wstring wmode(mode, mode + std::strlen(mode));
			return _wfopen(FileUtil::PathToNative(path).c_str(), wmode.c_str());
#else
			return std::fopen(path.c_str(), mode);
#endif
		}

		template<typename T>
		bool ValueRead(const uint8_t*& cursor, const uint8_t* end, T& out_value)
		{
			if ((size_t)(end - cursor) < sizeof(T)) return false;
			std::memcpy(&out_value, cursor, sizeof(T));
			cursor += sizeof(T);
			return true;
		}

		template<typename T>
		void ValueWrite(std::vector<uint8_t>& out, const T& value)
		{
			const uint8_t* p = reinterpret_cast<const uint8_t*>(&value);
			out.insert(out.end(), p, p + sizeof(T));
		}

		uint64_t AlignValue(uint64_t v, uint64_t a) { return (v + (a - 1)) / a * a; }
	}

	/*
	* 関数名　PathClean
	* 引　数　path：パス
	* 戻り値　区切りを整えたパス
	* 説　明　区切り文字を '/' に統一し、先頭の "./" "/" と連続する '/' を除去する
	*/
	std::string PathClean(const std::string& path)
	{
		std::string s = path;
		for (auto& c : s) if (c == '\\') c = '/';
		while (s.rfind("./", 0) == 0) s.erase(0, 2);
		while (!s.empty() && s.front() == '/') s.erase(0, 1);
		std::string out;
		out.reserve(s.size());
		for (char c : s) {
			if (c == '/' && !out.empty() && out.back() == '/') continue;
			out.push_back(c);
		}
		return out;
	}

	/*
	* 関数名　PathNormalize
	* 引　数　path：パス
	* 戻り値　検索キー
	* 説　明　PathClean したうえで英字を小文字に統一する
	*/
	std::string PathNormalize(const std::string& path)
	{
		std::string s = PathClean(path);
		for (auto& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
		return s;
	}

	// ================================================================
	// Reader
	// ================================================================

	Reader::~Reader()
	{
		Close();
	}

	/*
	* 関数名　Open
	* 引　数　file_path：アーカイブのパス
	* 戻り値　成功なら true
	* 説　明　ヘッダと TOC を読み込み、索引を構築する（データ本体は読まない）
	*/
	bool Reader::Open(const std::string& file_path)
	{
		Close();
		std::lock_guard<std::mutex> lk(m_Mutex);
		m_cls_File = FileOpenUtf8(file_path, "rb");
		if (!m_cls_File) {
			m_LastError = "アーカイブを開けません: " + file_path;
			return false;
		}
		if (!HeaderRead() || !TocRead()) {
			std::fclose(m_cls_File);
			m_cls_File = nullptr;
			m_Entries.clear();
			m_IndexByPath.clear();
			return false;
		}
		return true;
	}

	/*
	* 関数名　Close
	* 引　数　なし
	* 戻り値　なし
	* 説　明　アーカイブを閉じる
	*/
	void Reader::Close()
	{
		std::lock_guard<std::mutex> lk(m_Mutex);
		if (m_cls_File) {
			std::fclose(m_cls_File);
			m_cls_File = nullptr;
		}
		m_Entries.clear();
		m_IndexByPath.clear();
	}

	/*
	* 関数名　HeaderRead
	* 引　数　なし
	* 戻り値　成功なら true
	* 説　明　ヘッダを読み込み、マジックとバージョンを検証する
	*/
	bool Reader::HeaderRead()
	{
		if (std::fread(&m_str_Header, sizeof(m_str_Header), 1, m_cls_File) != 1) {
			m_LastError = "ヘッダの読み込みに失敗しました";
			return false;
		}
		if (std::memcmp(m_str_Header.magic, "PIXPAK", 6) != 0) {
			m_LastError = "PIXPAK 形式ではありません";
			return false;
		}
		if (m_str_Header.version < 1 || m_str_Header.version > VERSION_LATEST) {
			m_LastError = "未対応のバージョンです: " + std::to_string(m_str_Header.version);
			return false;
		}
		return true;
	}

	/*
	* 関数名　TocRead
	* 引　数　なし
	* 戻り値　成功なら true
	* 説　明　TOC を読み込みエントリ一覧と索引を構築する
	*/
	bool Reader::TocRead()
	{
		std::vector<uint8_t> toc;
		if (!FileSeek(m_cls_File, m_str_Header.tocOffset)) {
			m_LastError = "TOC へのシークに失敗しました";
			return false;
		}
		// v1/v2 は TOC サイズを持たないため末尾まで読む
		uint8_t buffer[64 * 1024];
		size_t n = 0;
		while ((n = std::fread(buffer, 1, sizeof(buffer), m_cls_File)) > 0) {
			toc.insert(toc.end(), buffer, buffer + n);
		}
		if (m_str_Header.version >= 3) {
			if (toc.size() < m_str_Header.tocSize) {
				m_LastError = "TOC が途中で切れています";
				return false;
			}
			toc.resize((size_t)m_str_Header.tocSize);
			if (HashUtil::CalcCRC32(toc.data(), toc.size()) != m_str_Header.tocCrc32) {
				m_LastError = "TOC の CRC が一致しません（アーカイブが破損しています）";
				return false;
			}
		}

		const uint8_t* cursor = toc.data();
		const uint8_t* end = toc.data() + toc.size();
		m_Entries.reserve(m_str_Header.fileCount);
		for (uint32_t i = 0; i < m_str_Header.fileCount; ++i) {
			Entry e;
			if (!TocEntryParse(cursor, end, e)) {
				m_LastError = "TOC の解析に失敗しました (index=" + std::to_string(i) + ")";
				return false;
			}
			m_IndexByPath[PathNormalize(e.path)] = m_Entries.size();
			m_Entries.push_back(std::move(e));
		}
		return true;
	}

	/*
	* 関数名　TocEntryParse
	* 引　数　cursor：読み取り位置（進む）　end：終端　out_entry：出力
	* 戻り値　成功なら true
	* 説　明　バージョンに応じて TOC エントリを 1 件解析する
	*/
	bool Reader::TocEntryParse(const uint8_t*& cursor, const uint8_t* end, Entry& out_entry) const
	{
		uint16_t name_len = 0;
		if (!ValueRead(cursor, end, name_len)) return false;
		if ((size_t)(end - cursor) < name_len) return false;
		out_entry.path = PathClean(std::string((const char*)cursor, name_len));
		cursor += name_len;

		uint8_t reserved[3]{};
		if (!ValueRead(cursor, end, out_entry.compression)) return false;
		if (m_str_Header.version >= 3) {
			if (!ValueRead(cursor, end, out_entry.assetType)) return false;
			if (!ValueRead(cursor, end, reserved[0]) || !ValueRead(cursor, end, reserved[1])) return false;
		}
		else {
			for (auto& r : reserved) if (!ValueRead(cursor, end, r)) return false;
		}
		if (!ValueRead(cursor, end, out_entry.crc32)) return false;
		if (!ValueRead(cursor, end, out_entry.originalSize)) return false;
		if (!ValueRead(cursor, end, out_entry.storedSize)) return false;
		if (!ValueRead(cursor, end, out_entry.offset)) return false;
		if (m_str_Header.version >= 2) {
			if ((size_t)(end - cursor) < out_entry.sha256.size()) return false;
			std::memcpy(out_entry.sha256.data(), cursor, out_entry.sha256.size());
			cursor += out_entry.sha256.size();
		}
		return true;
	}

	/*
	* 関数名　EntryFind
	* 引　数　path：仮想パス
	* 戻り値　エントリ（存在しなければ nullptr）
	* 説　明　仮想パスからエントリを検索する
	*/
	const Entry* Reader::EntryFind(const std::string& path) const
	{
		auto it = m_IndexByPath.find(PathNormalize(path));
		if (it == m_IndexByPath.end()) return nullptr;
		return &m_Entries[it->second];
	}

	/*
	* 関数名　FileExists
	* 引　数　path：仮想パス
	* 戻り値　存在すれば true
	* 説　明　アーカイブ内にファイルが存在するか調べる
	*/
	bool Reader::FileExists(const std::string& path) const
	{
		return EntryFind(path) != nullptr;
	}

	/*
	* 関数名　FileRead
	* 引　数　path：仮想パス　out_data：読み込み先
	* 戻り値　成功なら true
	* 説　明　1 ファイル分のデータを読み込み、CRC を検証する
	*/
	bool Reader::FileRead(const std::string& path, std::vector<uint8_t>& out_data)
	{
		const Entry* e = EntryFind(path);
		if (!e) return false;
		if (e->compression != (uint8_t)Compression::None) {
			m_LastError = "未対応の圧縮形式です: " + e->path;
			return false;
		}

		std::lock_guard<std::mutex> lk(m_Mutex);
		if (!m_cls_File || !FileSeek(m_cls_File, e->offset)) return false;
		out_data.resize((size_t)e->storedSize);
		if (e->storedSize > 0 && std::fread(out_data.data(), 1, out_data.size(), m_cls_File) != out_data.size()) {
			m_LastError = "データの読み込みに失敗しました: " + e->path;
			return false;
		}
		if (m_b_VerifyCrc && HashUtil::CalcCRC32(out_data.data(), out_data.size()) != e->crc32) {
			m_LastError = "CRC が一致しません: " + e->path;
			return false;
		}
		return true;
	}

	/*
	* 関数名　FileListGet
	* 引　数　prefix：仮想パスの接頭辞（空なら全件）
	* 戻り値　該当する仮想パスの一覧（昇順）
	* 説　明　アーカイブ内のファイル一覧を取得する
	*/
	std::vector<std::string> Reader::FileListGet(const std::string& prefix) const
	{
		const std::string norm = PathNormalize(prefix);
		std::vector<std::string> result;
		for (auto& kv : m_IndexByPath) {
			if (kv.first.rfind(norm, 0) == 0) result.push_back(m_Entries[kv.second].path);
		}
		std::sort(result.begin(), result.end());
		return result;
	}

	// ================================================================
	// Writer
	// ================================================================

	/*
	* 関数名　FileAdd
	* 引　数　virtual_path：アーカイブ内パス　disk_path：実ファイル　asset_type：種別
	* 戻り値　なし
	* 説　明　ディスク上のファイルを登録する（読み込みは書き出し時に行う）
	*/
	void Writer::FileAdd(const std::string& virtual_path, const std::string& disk_path, uint8_t asset_type)
	{
		Source s;
		s.virtualPath = PathClean(virtual_path);
		s.diskPath = disk_path;
		s.assetType = asset_type;
		m_Sources.push_back(std::move(s));
	}

	/*
	* 関数名　MemoryAdd
	* 引　数　virtual_path：アーカイブ内パス　data：内容　asset_type：種別
	* 戻り値　なし
	* 説　明　メモリ上のデータを登録する（マニフェストなどの生成物用）
	*/
	void Writer::MemoryAdd(const std::string& virtual_path, std::vector<uint8_t> data, uint8_t asset_type)
	{
		Source s;
		s.virtualPath = PathClean(virtual_path);
		s.memory = std::move(data);
		s.assetType = asset_type;
		m_Sources.push_back(std::move(s));
	}

	/*
	* 関数名　SourceLoad
	* 引　数　src：登録情報　out_data：読み込み先
	* 戻り値　成功なら true
	* 説　明　登録されたファイルの内容を取得する
	*/
	bool Writer::SourceLoad(const Source& src, std::vector<uint8_t>& out_data)
	{
		if (src.diskPath.empty()) {
			out_data = src.memory;
			return true;
		}
		if (!FileUtil::FileReadAll(src.diskPath, out_data)) {
			m_LastError = "読み込み失敗: " + src.diskPath;
			return false;
		}
		return true;
	}

	/*
	* 関数名　TocEntryWrite
	* 引　数　toc：TOC バッファ　entry：エントリ
	* 戻り値　なし
	* 説　明　v3 形式で TOC エントリを 1 件書き込む
	*/
	void Writer::TocEntryWrite(std::vector<uint8_t>& toc, const Entry& entry)
	{
		ValueWrite(toc, (uint16_t)entry.path.size());
		toc.insert(toc.end(), entry.path.begin(), entry.path.end());
		ValueWrite(toc, entry.compression);
		ValueWrite(toc, entry.assetType);
		ValueWrite(toc, (uint16_t)0);
		ValueWrite(toc, entry.crc32);
		ValueWrite(toc, entry.originalSize);
		ValueWrite(toc, entry.storedSize);
		ValueWrite(toc, entry.offset);
		toc.insert(toc.end(), entry.sha256.begin(), entry.sha256.end());
	}

	/*
	* 関数名　ArchiveWrite
	* 引　数　out_path：出力先　progress：進捗通知（任意）
	* 戻り値　成功なら true
	* 説　明　登録ファイルを仮想パス順に並べ、v3 形式のアーカイブを書き出す
	*         同じ入力からは常に同じバイナリが生成される（差分パッチや検証が容易）
	*/
	bool Writer::ArchiveWrite(const std::string& out_path, const ProgressCallback& progress)
	{
		std::sort(m_Sources.begin(), m_Sources.end(), [](const Source& a, const Source& b) {
			const std::string ka = PathNormalize(a.virtualPath), kb = PathNormalize(b.virtualPath);
			return (ka != kb) ? (ka < kb) : (a.virtualPath < b.virtualPath);
			});
		for (size_t i = 1; i < m_Sources.size(); ++i) {
			if (PathNormalize(m_Sources[i].virtualPath) == PathNormalize(m_Sources[i - 1].virtualPath)) {
				m_LastError = "仮想パスが重複しています: " + m_Sources[i].virtualPath;
				return false;
			}
		}

		std::ofstream ofs(FileUtil::PathToNative(out_path), std::ios::binary | std::ios::trunc);
		if (!ofs) {
			m_LastError = "出力を開けません: " + out_path;
			return false;
		}

		PakHeader header{};
		std::memcpy(header.magic, MAGIC, sizeof(header.magic));
		header.version = VERSION_LATEST;
		header.fileCount = (uint32_t)m_Sources.size();
		header.alignment = m_i_Alignment;
		ofs.write((const char*)&header, sizeof(header));

		std::vector<uint8_t> toc;
		uint64_t offset = sizeof(header);
		std::vector<uint8_t> data;
		static const char pad[4096]{};
		for (size_t i = 0; i < m_Sources.size(); ++i) {
			const Source& src = m_Sources[i];
			if (!SourceLoad(src, data)) return false;

			const uint64_t aligned = AlignValue(offset, m_i_Alignment);
			for (uint64_t remain = aligned - offset; remain > 0;) {
				const size_t chunk = (size_t)std::min<uint64_t>(remain, sizeof(pad));
				ofs.write(pad, chunk);
				remain -= chunk;
			}
			offset = aligned;

			Entry e;
			e.path = src.virtualPath;
			e.assetType = src.assetType;
			e.originalSize = e.storedSize = data.size();
			e.offset = offset;
			e.crc32 = HashUtil::CalcCRC32(data.data(), data.size());
			e.sha256 = HashUtil::SHA256(data);
			if (!data.empty()) ofs.write((const char*)data.data(), (std::streamsize)data.size());
			offset += data.size();
			TocEntryWrite(toc, e);
			if (progress) progress(i + 1, m_Sources.size(), src.virtualPath);
		}

		header.tocOffset = offset;
		header.tocSize = toc.size();
		header.tocCrc32 = HashUtil::CalcCRC32(toc.data(), toc.size());
		ofs.write((const char*)toc.data(), (std::streamsize)toc.size());
		ofs.seekp(0, std::ios::beg);
		ofs.write((const char*)&header, sizeof(header));
		ofs.flush();
		if (!ofs) {
			m_LastError = "書き込みに失敗しました: " + out_path;
			return false;
		}
		return true;
	}
}
