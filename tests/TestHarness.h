// Minimal self-registering test harness (no dependencies).
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace bo::test
{
	struct State
	{
		int Failures = 0;
		int Checks = 0;
		std::string Current;
	};
	inline State& S()
	{
		static State St;
		return St;
	}

	inline void Expect(bool Condition, const char* Expr, const char* File, int Line)
	{
		++S().Checks;
		if (!Condition)
		{
			++S().Failures;
			std::printf("  FAIL [%s] %s:%d: %s\n", S().Current.c_str(), File, Line, Expr);
		}
	}

	struct TestCase
	{
		const char* Name;
		std::function<void()> Fn;
	};
	inline std::vector<TestCase>& Registry()
	{
		static std::vector<TestCase> R;
		return R;
	}
	struct Register
	{
		Register(const char* Name, std::function<void()> Fn) { Registry().push_back({Name, std::move(Fn)}); }
	};
}

#define EXPECT(x) ::bo::test::Expect((x), #x, __FILE__, __LINE__)
#define TEST(name) static void name(); static ::bo::test::Register reg_##name(#name, name); static void name()
