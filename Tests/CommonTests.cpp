/*
* ファイル名　CommonTests
* 説　　　明　Common ライブラリ（アーカイブ・ハッシュ・種別判定・マニフェスト）のテスト
*/
#include "TestFramework.h"
#include "AssetType.h"
#include "FileUtil.h"
#include "GameManifest.h"
#include "HashUtil.h"
#include "PixArchive.h"
#include "VirtualFileSystem.h"
#include <thread>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {
	fs::path TempDirMake(const char* name)
	{
		fs::path p = fs::temp_directory_path() / ("pixeon_test_" + std::string(name));
		fs::remove_all(p);
		fs::create_directories(p);
		return p;
	}

	void TextWrite(const fs::path& p, const std::string& text)
	{
		fs::create_directories(p.parent_path());
		std::ofstream(p, std::ios::binary) << text;
	}
}

PIX_TEST(HashKnownVectors)
{
	PIX_CHECK(HashUtil::CalcCRC32("123456789", 9) == 0xCBF43926u);
	auto h = HashUtil::SHA256("abc", 3);
	PIX_CHECK(h[0] == 0xba && h[1] == 0x78 && h[31] == 0xad);
	PIX_CHECK(HashUtil::CalcFNV1a64("", 0) == 0xcbf29ce484222325ull);
}

PIX_TEST(PathNormalize)
{
	PIX_CHECK(PixArchive::PathNormalize("Assets\\Tex\\A.PNG") == "assets/tex/a.png");
	PIX_CHECK(PixArchive::PathNormalize("./a//b") == "a/b");
	PIX_CHECK(PixArchive::PathNormalize("/scene/x.scene") == "scene/x.scene");
}

PIX_TEST(AssetTypeClassify)
{
	PIX_CHECK(AssetTypeUtil::TypeFromPath("a/b/c.PNG") == AssetType::Texture);
	PIX_CHECK(AssetTypeUtil::TypeFromPath("model.fbx") == AssetType::Model);
	PIX_CHECK(AssetTypeUtil::TypeFromPath("x.efkefc") == AssetType::Effect);
	PIX_CHECK(AssetTypeUtil::TypeFromPath("dir.v2/noext") == AssetType::Unknown);
	PIX_CHECK(AssetTypeUtil::TypeFromExtension(".WAV") == AssetType::Sound);
	PIX_CHECK(!AssetTypeUtil::IsRuntimeAsset(AssetType::ShaderSource));
	PIX_CHECK(AssetTypeUtil::IsRuntimeAsset(AssetType::Texture));
}

PIX_TEST(ArchiveRoundTrip)
{
	const fs::path dir = TempDirMake("archive");
	TextWrite(dir / "src/Tex/B.png", "png-data");
	TextWrite(dir / "src/a.txt", "hello");
	std::string big(100000, 'x');
	for (size_t i = 0; i < big.size(); ++i) big[i] = (char)(i * 31);
	TextWrite(dir / "src/big.bin", big);

	PixArchive::Writer w;
	w.SetAlignment(4096);
	w.FileAdd("assets/Tex/B.png", (dir / "src/Tex/B.png").string(), (uint8_t)AssetType::Texture);
	w.FileAdd("assets/a.txt", (dir / "src/a.txt").string());
	w.FileAdd("assets/big.bin", (dir / "src/big.bin").string());
	w.MemoryAdd("game.json", std::vector<uint8_t>{ '{', '}' });
	const std::string pak = (dir / "out.pak").string();
	PIX_CHECK(w.ArchiveWrite(pak));

	PixArchive::Reader r;
	PIX_CHECK(r.Open(pak));
	PIX_CHECK(r.GetVersion() == 3);
	PIX_CHECK(r.GetFileCount() == 4);
	std::vector<uint8_t> out;
	PIX_CHECK(r.FileRead("ASSETS/tex/b.PNG", out));
	PIX_CHECK(std::string(out.begin(), out.end()) == "png-data");
	PIX_CHECK(r.FileRead("assets/big.bin", out) && out.size() == big.size() && std::memcmp(out.data(), big.data(), big.size()) == 0);
	PIX_CHECK(r.EntryFind("assets/tex/b.png")->assetType == (uint8_t)AssetType::Texture);
	PIX_CHECK(r.EntryFind("assets/big.bin")->offset % 4096 == 0);
	PIX_CHECK(!r.FileRead("assets/missing.png", out));
	PIX_CHECK(r.FileListGet("assets/").size() == 3);
	PIX_CHECK(r.FileListGet("ASSETS/tex/")[0] == "assets/Tex/B.png");  // 大文字小文字は保持される
}

PIX_TEST(ArchiveDeterministic)
{
	const fs::path dir = TempDirMake("determ");
	TextWrite(dir / "a.txt", "A");
	TextWrite(dir / "b.txt", "B");
	auto build = [&](const char* name, bool reverse) {
		PixArchive::Writer w;
		if (reverse) { w.FileAdd("b", (dir / "b.txt").string()); w.FileAdd("a", (dir / "a.txt").string()); }
		else { w.FileAdd("a", (dir / "a.txt").string()); w.FileAdd("b", (dir / "b.txt").string()); }
		w.ArchiveWrite((dir / name).string());
		std::vector<uint8_t> d;
		FileUtil::FileReadAll((dir / name).string(), d);
		return d;
	};
	PIX_CHECK(build("1.pak", false) == build("2.pak", true));
}

PIX_TEST(ArchiveDetectsCorruption)
{
	const fs::path dir = TempDirMake("corrupt");
	TextWrite(dir / "a.txt", "0123456789");
	PixArchive::Writer w;
	w.FileAdd("a.txt", (dir / "a.txt").string());
	const std::string pak = (dir / "c.pak").string();
	PIX_CHECK(w.ArchiveWrite(pak));
	{
		// データ部 (ヘッダ直後) を 1byte 書き換える
		std::fstream f(pak, std::ios::in | std::ios::out | std::ios::binary);
		f.seekp(64 + 3);
		f.put('Z');
	}
	PixArchive::Reader r;
	PIX_CHECK(r.Open(pak));
	std::vector<uint8_t> out;
	PIX_CHECK(!r.FileRead("a.txt", out));

	PIX_CHECK(!r.Open((dir / "a.txt").string()));  // PIXPAK ではない
}

PIX_TEST(ArchiveReadsLegacyV2)
{
	// 旧パッキングツール (v2) と同じレイアウトで手書きする
	const fs::path dir = TempDirMake("v2");
	const std::string body = "legacy";
	PixArchive::PakHeader h{};
	std::memcpy(h.magic, "PIXPAK\0", 8);
	h.version = 2;
	h.fileCount = 1;
	h.alignment = 16;
	h.tocOffset = sizeof(h) + body.size();
	std::ofstream ofs(dir / "v2.pak", std::ios::binary);
	ofs.write((const char*)&h, sizeof(h));
	ofs.write(body.data(), body.size());
	const std::string name = "dir/file.txt";
	uint16_t len = (uint16_t)name.size();
	ofs.write((const char*)&len, 2);
	ofs.write(name.data(), len);
	const char comp_res[4] = {};
	ofs.write(comp_res, 4);
	uint32_t crc = HashUtil::CalcCRC32(body.data(), body.size());
	uint64_t size = body.size(), offset = sizeof(h);
	ofs.write((const char*)&crc, 4);
	ofs.write((const char*)&size, 8);
	ofs.write((const char*)&size, 8);
	ofs.write((const char*)&offset, 8);
	auto sha = HashUtil::SHA256(body.data(), body.size());
	ofs.write((const char*)sha.data(), 32);
	ofs.close();

	PixArchive::Reader r;
	PIX_CHECK(r.Open((dir / "v2.pak").string()));
	std::vector<uint8_t> out;
	PIX_CHECK(r.FileRead("dir/file.txt", out) && std::string(out.begin(), out.end()) == body);
}

PIX_TEST(ManifestRoundTrip)
{
	GameManifest m;
	m.name = "テストゲーム";
	m.startScene = "Stage1";
	m.backend = GraphicsBackend::Vulkan;
	m.i_Width = 1280;
	GameManifest m2;
	PIX_CHECK(m2.JsonFromString(m.JsonToString()));
	PIX_CHECK(m2.name == m.name && m2.startScene == "Stage1" && m2.backend == GraphicsBackend::Vulkan && m2.i_Width == 1280);
	GraphicsBackend b{};
	PIX_CHECK(GraphicsBackendUtil::NameParse("d3d12", b) && b == GraphicsBackend::D3D12);
	PIX_CHECK(!GraphicsBackendUtil::NameParse("metal", b));
	std::string err;
	PIX_CHECK(!m2.JsonFromString("{\"Graphics\":{\"Backend\":\"metal\"}}", &err) && !err.empty());
}

PIX_TEST(VfsMountAndPriority)
{
	const fs::path dir = TempDirMake("vfs");
	TextWrite(dir / "Assets/Tex/A.png", "dirA");
	TextWrite(dir / "Assets/model.fbx", "fbx");
	TextWrite(dir / "Scene/SampleScene.scene", "{}");

	VirtualFileSystem vfs;
	PIX_CHECK(vfs.DirectoryMount("assets", (dir / "Assets").string(), false));
	PIX_CHECK(vfs.DirectoryMount("scene/", (dir / "Scene").string(), false));
	PIX_CHECK(!vfs.DirectoryMount("missing", (dir / "Nope").string(), false));

	std::string text;
	PIX_CHECK(vfs.TextRead("assets/Tex/A.png", text) && text == "dirA");
	PIX_CHECK(vfs.TextRead("ASSETS/tex/a.PNG", text) && text == "dirA");  // 大文字小文字を区別しない
	PIX_CHECK(vfs.FileExists("scene/SampleScene.scene"));
	PIX_CHECK(!vfs.FileExists("assets/none.png"));

	auto scenes = vfs.FileListGet("scene/");
	PIX_CHECK(scenes.size() == 1 && scenes[0].path == "scene/SampleScene.scene");
	PIX_CHECK(vfs.FileListGet("assets/").size() == 2);

	// アーカイブを後からマウントすると優先される
	PixArchive::Writer w;
	w.MemoryAdd("assets/Tex/A.png", std::vector<uint8_t>{ 'p', 'a', 'k' });
	w.MemoryAdd("assets/OnlyInPak.wav", std::vector<uint8_t>{ 'w' });
	const std::string pak = (dir / "data.pak").string();
	PIX_CHECK(w.ArchiveWrite(pak));
	PIX_CHECK(vfs.ArchiveMount(pak));
	PIX_CHECK(vfs.TextRead("assets/Tex/A.png", text) && text == "pak");
	PIX_CHECK(vfs.FileListGet("assets/").size() == 3);
	std::string err;
	PIX_CHECK(!vfs.ArchiveMount((dir / "none.pak").string(), &err) && !err.empty());
}

PIX_TEST(VfsCacheAndHotReload)
{
	const fs::path dir = TempDirMake("vfswatch");
	TextWrite(dir / "a.txt", "v1");
	VirtualFileSystem vfs;
	vfs.DirectoryMount("assets", dir.string(), true);
	std::string text;
	PIX_CHECK(vfs.TextRead("assets/a.txt", text) && text == "v1");
	PIX_CHECK(vfs.TextRead("assets/a.txt", text) && text == "v1");
	PIX_CHECK(vfs.CacheStatsGet().i_Hits == 1);

	// 内容とサイズを変える → 走査でキャッシュが破棄され、変更イベントが届く
	TextWrite(dir / "a.txt", "version2");
	TextWrite(dir / "b.txt", "new");
	vfs.ChangesScanNow();
	std::vector<FileChange> changes;
	PIX_CHECK(vfs.ChangesPoll(changes) == 2);
	bool modified = false, added = false;
	for (auto& c : changes) {
		modified |= (c.e_Type == FileChange::Type::Modified && c.path == "assets/a.txt");
		added |= (c.e_Type == FileChange::Type::Added && c.path == "assets/b.txt");
	}
	PIX_CHECK(modified && added);
	PIX_CHECK(vfs.TextRead("assets/a.txt", text) && text == "version2");

	fs::remove(dir / "b.txt");
	vfs.ChangesScanNow();
	changes.clear();
	PIX_CHECK(vfs.ChangesPoll(changes) == 1 && changes[0].e_Type == FileChange::Type::Removed);

	// バックグラウンド監視の開始・停止
	vfs.WatchStart(std::chrono::milliseconds(10));
	const uint64_t before = vfs.ScanCountGet();
	for (int i = 0; i < 200 && vfs.ScanCountGet() < before + 2; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));
	PIX_CHECK(vfs.ScanCountGet() >= before + 2);
	vfs.WatchStop();
	PIX_CHECK(!vfs.IsWatching());
}

PIX_TEST(VfsCacheBudget)
{
	const fs::path dir = TempDirMake("vfsbudget");
	for (int i = 0; i < 10; ++i) TextWrite(dir / (std::to_string(i) + ".bin"), std::string(1000, 'a'));
	VirtualFileSystem vfs;
	vfs.CacheBudgetSet(4500);  // 1 ファイルは予算の 1/4 未満 → キャッシュ対象
	vfs.DirectoryMount("", dir.string(), false);
	std::vector<uint8_t> d;
	for (int i = 0; i < 10; ++i) PIX_CHECK(vfs.FileRead(std::to_string(i) + ".bin", d));
	auto st = vfs.CacheStatsGet();
	PIX_CHECK(st.i_Bytes <= 4500 && st.i_Entries == 4);
}

int main()
{
	return PixTest::AllRun();
}
