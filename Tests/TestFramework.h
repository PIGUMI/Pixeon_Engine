/*
* ファイル名　TestFramework
* 説　　　明　外部ライブラリに依存しない最小限のテスト基盤
*/
#pragma once
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace PixTest {
	struct Case {
		const char* name;
		std::function<void()> func;
	};

	inline std::vector<Case>& Registry() { static std::vector<Case> r; return r; }
	inline int& FailCount() { static int n = 0; return n; }

	struct Registrar {
		Registrar(const char* name, std::function<void()> f) { Registry().push_back({ name, std::move(f) }); }
	};

	inline int AllRun()
	{
		int failed_cases = 0;
		for (auto& c : Registry()) {
			const int before = FailCount();
			c.func();
			const bool ok = FailCount() == before;
			if (!ok) ++failed_cases;
			std::printf("[%s] %s\n", ok ? " OK " : "FAIL", c.name);
		}
		std::printf("%zu cases, %d failed\n", Registry().size(), failed_cases);
		return failed_cases == 0 ? 0 : 1;
	}
}

#define PIX_TEST_CAT2(a, b) a##b
#define PIX_TEST_CAT(a, b) PIX_TEST_CAT2(a, b)
#define PIX_TEST(name) \
	static void PIX_TEST_CAT(PixTestFn_, name)(); \
	static PixTest::Registrar PIX_TEST_CAT(PixTestReg_, name)(#name, &PIX_TEST_CAT(PixTestFn_, name)); \
	static void PIX_TEST_CAT(PixTestFn_, name)()

#define PIX_CHECK(cond) do { if (!(cond)) { \
	std::printf("  %s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); ++PixTest::FailCount(); } } while (0)
