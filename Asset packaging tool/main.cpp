/*
* ファイル名　main (Asset packaging tool)
* 説　　　明　ディレクトリを PIXPAK アーカイブへまとめるコマンドラインツール
*
* 使用方法
*   旧形式 : PixAssetPacker <input_dir> <output.pak> [alignment]
*   新形式 : PixAssetPacker --out <output.pak> [--align N] [--exclude ext,ext]
*                           --mount <prefix>=<dir> [--mount <prefix>=<dir> ...]
*            例) --mount assets=SceneRoot/Assets --mount scene=SceneRoot/Scene
*   --list <archive.pak>    : アーカイブの中身を表示する
*/
#include "../Common/PixArchive.h"
#include "../Common/AssetType.h"
#include "../Common/FileUtil.h"
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {
	struct MountSpec {
		std::string prefix;
		std::string dir;
	};

	struct PackOptions {
		std::string outPath;
		uint32_t i_Alignment = PixArchive::ALIGNMENT_DEFAULT;
		std::vector<MountSpec> mounts;
		std::set<std::string> excludeExts;
		std::string listPath;
	};

	/*
	* 関数名　UsagePrint
	* 説　明　使用方法を表示する
	*/
	void UsagePrint()
	{
		std::cout <<
			"使用方法:\n"
			"  PixAssetPacker <input_dir> <output.pak> [alignment]\n"
			"  PixAssetPacker --out <output.pak> [--align N] [--exclude ext,ext] --mount <prefix>=<dir> ...\n"
			"  PixAssetPacker --list <archive.pak>\n";
	}

	/*
	* 関数名　ExcludeParse
	* 引　数　csv：カンマ区切りの拡張子　out_set：出力
	* 説　明　除外拡張子リストを解釈する
	*/
	void ExcludeParse(const std::string& csv, std::set<std::string>& out_set)
	{
		std::stringstream ss(csv);
		std::string item;
		while (std::getline(ss, item, ',')) {
			if (!item.empty() && item.front() == '.') item.erase(0, 1);
			if (!item.empty()) out_set.insert(AssetTypeUtil::ExtensionGet("x." + item));
		}
	}

	/*
	* 関数名　ArgumentsParse
	* 引　数　argc/argv：コマンドライン　out_opt：出力
	* 戻り値　解釈できたら true
	* 説　明　新旧どちらの形式も受け付ける
	*/
	bool ArgumentsParse(int argc, char* argv[], PackOptions& out_opt)
	{
		if (argc >= 3 && argv[1][0] != '-') {
			out_opt.mounts.push_back({ "", argv[1] });
			out_opt.outPath = argv[2];
			if (argc >= 4) out_opt.i_Alignment = (uint32_t)std::max(1ul, std::stoul(argv[3]));
			return true;
		}
		for (int i = 1; i < argc; ++i) {
			const std::string a = argv[i];
			auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : ""; };
			if (a == "--out") out_opt.outPath = next();
			else if (a == "--align") out_opt.i_Alignment = (uint32_t)std::max(1ul, std::stoul(next()));
			else if (a == "--exclude") ExcludeParse(next(), out_opt.excludeExts);
			else if (a == "--list") out_opt.listPath = next();
			else if (a == "--mount") {
				const std::string spec = next();
				const size_t eq = spec.find('=');
				if (eq == std::string::npos) return false;
				out_opt.mounts.push_back({ spec.substr(0, eq), spec.substr(eq + 1) });
			}
			else return false;
		}
		return !out_opt.listPath.empty() || (!out_opt.outPath.empty() && !out_opt.mounts.empty());
	}

	/*
	* 関数名　MountCollect
	* 引　数　mount：マウント指定　opt：オプション　writer：登録先
	* 戻り値　成功なら true
	* 説　明　ディレクトリを再帰的に走査して Writer に登録する
	*/
	bool MountCollect(const MountSpec& mount, const PackOptions& opt, PixArchive::Writer& writer)
	{
		const fs::path root = FileUtil::PathToNative(mount.dir);
		std::error_code ec;
		if (!fs::is_directory(root, ec)) {
			std::cout << "入力ディレクトリが存在しません: " << mount.dir << "\n";
			return false;
		}
		for (auto it = fs::recursive_directory_iterator(root, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
			if (!it->is_regular_file()) continue;
			const std::string rel = FileUtil::PathToUtf8(fs::relative(it->path(), root));
			if (opt.excludeExts.count(AssetTypeUtil::ExtensionGet(rel))) continue;
			const std::string vpath = mount.prefix.empty() ? rel : (mount.prefix + "/" + rel);
			writer.FileAdd(vpath, FileUtil::PathToUtf8(it->path()), (uint8_t)AssetTypeUtil::TypeFromPath(rel));
		}
		return true;
	}

	/*
	* 関数名　ArchiveList
	* 引　数　path：アーカイブ
	* 戻り値　終了コード
	* 説　明　アーカイブの内容を一覧表示する
	*/
	int ArchiveList(const std::string& path)
	{
		PixArchive::Reader reader;
		if (!reader.Open(path)) {
			std::cout << reader.GetLastError() << "\n";
			return 1;
		}
		std::cout << "version=" << reader.GetVersion() << " files=" << reader.GetFileCount() << "\n";
		for (auto& p : reader.FileListGet()) {
			const auto* e = reader.EntryFind(p);
			std::cout << std::setw(10) << e->originalSize << "  "
				<< std::setw(12) << AssetTypeUtil::NameGet((AssetType)e->assetType) << "  " << p << "\n";
		}
		return 0;
	}
}

int main(int argc, char* argv[])
{
#if defined(_WIN32)
	SetConsoleOutputCP(CP_UTF8);
#endif
	PackOptions opt;
	if (!ArgumentsParse(argc, argv, opt)) {
		UsagePrint();
		return 1;
	}
	if (!opt.listPath.empty()) return ArchiveList(opt.listPath);

	PixArchive::Writer writer;
	writer.SetAlignment(opt.i_Alignment);
	for (auto& m : opt.mounts) {
		if (!MountCollect(m, opt, writer)) return 1;
	}
	if (writer.GetFileCount() == 0) {
		std::cout << "対象ファイルがありません\n";
		return 1;
	}

	const bool ok = writer.ArchiveWrite(opt.outPath, [](size_t done, size_t total, const std::string&) {
		if (done % 10 == 0 || done == total)
			std::cout << "\r書き込み中... " << std::fixed << std::setprecision(1) << (100.0 * done / total) << "%" << std::flush;
		});
	std::cout << "\n";
	if (!ok) {
		std::cout << "エラー: " << writer.GetLastError() << "\n";
		return 1;
	}
	std::cout << "パック完了: " << opt.outPath << " (" << writer.GetFileCount() << " files)\n";
	return 0;
}
