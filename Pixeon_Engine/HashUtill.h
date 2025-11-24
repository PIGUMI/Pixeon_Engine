#ifndef HASHUTIL_H
#define HASHUTIL_H

#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>

namespace HashUtil {
	void InitCRC32();
	uint32_t CalcCRC32(const void* data, size_t len, uint32_t crc = 0xFFFFFFFFu);

	// ŠÈˆÕ SHA-256 ŽÀ‘•
	struct SHA256Ctx {
		uint64_t bitlen = 0;
		uint32_t state[8];
		uint8_t  buffer[64];
		size_t   bufferLen = 0;
	};

	void SHA256Init(SHA256Ctx& ctx);
	void SHA256Update(SHA256Ctx& ctx, const uint8_t* data, size_t len);
	void SHA256Final(SHA256Ctx& ctx, uint8_t out[32]);

	inline std::array<uint8_t, 32> SHA256(const std::vector<uint8_t>& data) {
		SHA256Ctx ctx;
		SHA256Init(ctx);
		if (!data.empty())
			SHA256Update(ctx, reinterpret_cast<const uint8_t*>(data.data()), data.size());
		std::array<uint8_t, 32> h{};
		SHA256Final(ctx, h.data());
		return h;
	}
	inline std::array<uint8_t, 32> SHA256(const void* data, size_t len) {
		SHA256Ctx ctx;
		SHA256Init(ctx);
		if (len)
			SHA256Update(ctx, reinterpret_cast<const uint8_t*>(data), len);
		std::array<uint8_t, 32> h{};
		SHA256Final(ctx, h.data());
		return h;
	}
}

#endif // !HASHUTIL_H
