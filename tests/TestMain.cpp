#include "TestHarness.h"

int main()
{
	using namespace bo::test;
	for (const TestCase& Tc : Registry())
	{
		S().Current = Tc.Name;
		const int Before = S().Failures;
		Tc.Fn();
		std::printf("%s %s\n", S().Failures == Before ? "PASS" : "FAIL", Tc.Name);
	}
	std::printf("\n%zu tests, %d checks, %d failures\n", Registry().size(), S().Checks, S().Failures);
	return S().Failures == 0 ? 0 : 1;
}
