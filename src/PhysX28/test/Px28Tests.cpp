// Runner for the shim's dependency-free checks.
//
// No backend, no simulation, no window -- these are all things that can be
// wrong silently and that cost nothing to verify: numeric values the shipped
// data serialises, and API conventions the game's call sites depend on.

#include <cstdio>

int RunEnumTests();
int RunVec3Tests();
int RunQuatTests();

int main()
{
	int failures = 0;

	failures += RunEnumTests();
	printf("\n");
	failures += RunVec3Tests();
	printf("\n");
	failures += RunQuatTests();

	printf("\n================================\n");
	printf("%s: %d failure%s\n", failures ? "FAILED" : "OK",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
