#include <windows.h>
#include <cstdint>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include "ArchiveFormat.h"
#include "HashUtill.h"

namespace fs = std::filesystem;

static uint64_t AlignValue(uint64_t v, uint64_t a) { return (v + (a - 1)) & ~(a - 1); }

struct TempFileEntry {
    std::string relativePath;
    uint64_t originalSize = 0;
    uint64_t storedSize = 0;
    uint64_t offset = 0;
    uint32_t crc32 = 0;
    uint8_t  compression = 0;
    std::array<uint8_t, 32> sha256{};
};

static void CollectFiles(const fs::path& root, std::vector<TempFileEntry>& out) {
    for (auto& p : fs::recursive_directory_iterator(root)) {
        if (!p.is_regular_file()) continue;
        auto rel = fs::relative(p.path(), root);
        std::string r = rel.generic_string();
        std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return (char)tolower(c); });
        TempFileEntry e;
        e.relativePath = r;
        out.push_back(std::move(e));
    }
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 3 || argc > 4) {
        std::cout << "使用方法: PixAssetPacker.exe <input_dir> <output.pak> [alignment]\n";
        return 1;
    }

    fs::path inputDir = argv[1];
    fs::path outputPak = argv[2];
    uint32_t alignment = 16;
    if (argc == 4) alignment = std::max<uint32_t>(1, std::stoul(argv[3]));

    if (!fs::exists(inputDir) || !fs::is_directory(inputDir)) {
        std::cout << "入力ディレクトリが存在しません\n";
        return 1;
    }

    std::vector<TempFileEntry> files;
    CollectFiles(inputDir, files);
    if (files.empty()) {
        std::cout << "対象ファイルがありません\n";
        return 1;
    }

    HashUtil::InitCRC32();

    PakHeader header{};
    memcpy(header.magic, "PIXPAK\0", 8);
    header.version = 2;  // v2
    header.fileCount = (uint32_t)files.size();
    header.tocOffset = 0;
    header.flags = 0;
    header.alignment = alignment;
    memset(header.reserved, 0, sizeof(header.reserved));

    std::ofstream ofs(outputPak, std::ios::binary);
    if (!ofs) {
        std::cout << "出力を開けません: " << outputPak << "\n";
        return 1;
    }

    ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));
    uint64_t currentOffset = sizeof(header);

    size_t idx = 0;
    for (auto& fe : files) {
        uint64_t aligned = AlignValue(currentOffset, header.alignment);
        if (aligned != currentOffset) {
            size_t pad = (size_t)(aligned - currentOffset);
            static const char padBuf[4096]{};
            while (pad > 0) {
                size_t chunk = std::min<size_t>(pad, sizeof(padBuf));
                ofs.write(padBuf, chunk);
                pad -= chunk;
            }
            currentOffset = aligned;
        }

        fs::path full = inputDir / fe.relativePath;
        std::ifstream ifs(full, std::ios::binary | std::ios::ate);
        if (!ifs) {
            std::cout << "読込失敗: " << full << "\n";
            return 1;
        }
        uint64_t sz = (uint64_t)ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        std::vector<uint8_t> buf;
        buf.resize((size_t)sz);
        if (sz > 0) {
            if (!ifs.read(reinterpret_cast<char*>(buf.data()), sz)) {
                std::cout << "読込中エラー: " << full << "\n";
                return 1;
            }
        }

        fe.originalSize = sz;
        fe.storedSize = sz;
        fe.offset = currentOffset;
        fe.crc32 = HashUtil::CalcCRC32(buf.data(), buf.size());
        auto h = HashUtil::SHA256(buf);
        fe.sha256 = h;

        if (sz > 0) ofs.write(reinterpret_cast<const char*>(buf.data()), sz);
        currentOffset += sz;

        ++idx;
        if (idx % 10 == 0 || idx == files.size()) {
            double prog = (double)idx / files.size() * 100.0;
            std::cout << "\r書き込み中... " << std::fixed << std::setprecision(1) << prog << "%";
        }
    }
    std::cout << "\n";

    header.tocOffset = currentOffset;

    // TOC 書き込み (v2)
    for (auto& fe : files) {
        uint16_t nameLen = (uint16_t)fe.relativePath.size();
        ofs.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
        ofs.write(fe.relativePath.data(), nameLen);
        ofs.write(reinterpret_cast<const char*>(&fe.compression), 1);
        uint8_t reservedC[3]{};
        ofs.write(reinterpret_cast<const char*>(reservedC), 3);
        ofs.write(reinterpret_cast<const char*>(&fe.crc32), sizeof(fe.crc32));
        ofs.write(reinterpret_cast<const char*>(&fe.originalSize), sizeof(fe.originalSize));
        ofs.write(reinterpret_cast<const char*>(&fe.storedSize), sizeof(fe.storedSize));
        ofs.write(reinterpret_cast<const char*>(&fe.offset), sizeof(fe.offset));
        ofs.write(reinterpret_cast<const char*>(fe.sha256.data()), fe.sha256.size());
    }

    ofs.seekp(0, std::ios::beg);
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));
    ofs.flush();

    std::cout << "パック完了: " << outputPak << "\n";
    std::cout << "ファイル数: " << files.size() << "\n";
    std::cout << "TOC Offset: " << header.tocOffset << "\n";
    return 0;
}