#ifndef ARCHIVE_FORMAT_H
#define ARCHIVE_FORMAT_H

#include <cstdint>
#include <cstring>

#pragma pack(push,1)
struct PakHeader {
	char     magic[8];      // "PIXPAK\0"
	uint32_t version;       // 1 or 2
	uint32_t fileCount;
	uint64_t tocOffset;
	uint32_t flags;         // 0
	uint32_t alignment;     // 16 / 4096 Ç»Ç«
	uint8_t  reserved[32];  // 0
};
#pragma pack(pop)

// à≥èkéÌï 
enum class AssetCompression : uint16_t {
	None = 0,
	LZ4 = 1,
};

// ÉtÉâÉO
enum AssetFlags : uint16_t {
	AssetFlag_None = 0,
	AssetFlag_Encrypted = 1 << 0,
	AssetFlag_Streamable = 1 << 1,
	AssetFlag_PatchData = 1 << 2,
};
inline bool HasFlag(uint16_t f, AssetFlags bit) {
	return (f & static_cast<uint16_t>(bit)) != 0;
}

#endif // !ARCHIVE_FORMAT_H