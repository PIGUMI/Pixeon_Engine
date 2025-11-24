#include "HashUtill.h"
#include <cstring>

namespace HashUtil {
	// ---- CRC32 ----
	static uint32_t CRC32Table[256];
	void InitCRC32() {
		static bool inited = false;
		if (inited) return;
		inited = true;
		uint32_t poly = 0xEDB88320u;
		for (uint32_t i = 0; i < 256; ++i) {
			uint32_t r = i;
			for (int j = 0; j < 8; ++j) {
				if (r & 1) r = (r >> 1) ^ poly;
				else r >>= 1;
			}
			CRC32Table[i] = r;
		}
	}
	uint32_t CalcCRC32(const void* data, size_t len, uint32_t crc) {
		const uint8_t* p = (const uint8_t*)data;
		for (size_t i = 0; i < len; ++i) {
			crc = CRC32Table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
		}
		return crc ^ 0xFFFFFFFFu;
	}

	// ---- SHA-256 ----
	static inline uint32_t ROR(uint32_t v, uint32_t n) { return (v >> n) | (v << (32 - n)); }
	static const uint32_t K[64] = {
	  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
	  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
	  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
	  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
	  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
	  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
	  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
	  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
	};

	void SHA256Init(SHA256Ctx& ctx) {
		ctx.bitlen = 0;
		ctx.bufferLen = 0;
		ctx.state[0] = 0x6a09e667;
		ctx.state[1] = 0xbb67ae85;
		ctx.state[2] = 0x3c6ef372;
		ctx.state[3] = 0xa54ff53a;
		ctx.state[4] = 0x510e527f;
		ctx.state[5] = 0x9b05688c;
		ctx.state[6] = 0x1f83d9ab;
		ctx.state[7] = 0x5be0cd19;
	}

	static void SHA256Transform(SHA256Ctx& ctx, const uint8_t data[64]) {
		uint32_t w[64];
		for (int i = 0; i < 16; ++i) {
			w[i] = (uint32_t)data[i * 4] << 24 |
				(uint32_t)data[i * 4 + 1] << 16 |
				(uint32_t)data[i * 4 + 2] << 8 |
				(uint32_t)data[i * 4 + 3];
		}
		for (int i = 16; i < 64; ++i) {
			uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
			uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
			w[i] = w[i - 16] + s0 + w[i - 7] + s1;
		}

		uint32_t a = ctx.state[0], b = ctx.state[1], c = ctx.state[2], d = ctx.state[3],
			e = ctx.state[4], f = ctx.state[5], g = ctx.state[6], h = ctx.state[7];

		for (int i = 0; i < 64; ++i) {
			uint32_t S1 = ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25);
			uint32_t ch = (e & g) ^ ((~e) & f);
			uint32_t temp1 = h + S1 + ch + K[i] + w[i];
			uint32_t S0 = ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22);
			uint32_t maj = (a & c) ^ (a & b) ^ (b & c);
			uint32_t temp2 = S0 + maj;
			h = g;
			g = f;
			f = e;
			e = d + temp1;
			d = c;
			c = b;
			b = a;
			a = temp1 + temp2;
		}
		ctx.state[0] += a;
		ctx.state[1] += b;
		ctx.state[2] += c;
		ctx.state[3] += d;
		ctx.state[4] += e;
		ctx.state[5] += f;
		ctx.state[6] += g;
		ctx.state[7] += h;
	}

	void SHA256Update(SHA256Ctx& ctx, const uint8_t* data, size_t len) {
		while (len > 0) {
			size_t toCopy = 64 - ctx.bufferLen;
			if (toCopy > len) toCopy = len;
			memcpy(ctx.buffer + ctx.bufferLen, data, toCopy);
			ctx.bufferLen += toCopy;
			data += toCopy;
			len -= toCopy;
			if (ctx.bufferLen == 64) {
				SHA256Transform(ctx, ctx.buffer);
				ctx.bitlen += 512;
				ctx.bufferLen = 0;
			}
		}
	}
	void SHA256Final(SHA256Ctx& ctx, uint8_t out[32]) {
		ctx.bitlen += ctx.bufferLen * 8ULL;
		size_t i = ctx.bufferLen;
		ctx.buffer[i++] = 0x80;
		if (i > 56) {
			while (i < 64) ctx.buffer[i++] = 0;
			SHA256Transform(ctx, ctx.buffer);
			i = 0;
		}
		while (i < 56) ctx.buffer[i++] = 0;
		for (int j = 7; j >= 0; --j) {
			ctx.buffer[i++] = (uint8_t)((ctx.bitlen >> (j * 8)) & 0xFFu);
		}
		SHA256Transform(ctx, ctx.buffer);
		for (int s = 0; s < 8; ++s) {
			out[s * 4 + 0] = (uint8_t)(ctx.state[s] >> 24);
			out[s * 4 + 1] = (uint8_t)(ctx.state[s] >> 16);
			out[s * 4 + 2] = (uint8_t)(ctx.state[s] >> 8);
			out[s * 4 + 3] = (uint8_t)(ctx.state[s] & 0xFFu);
		}
	}
} // namespace HashUtil