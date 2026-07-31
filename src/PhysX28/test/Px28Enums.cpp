// The numeric truth of the shim, checked against the values that are actually
// serialised into shipped game data.
//
// This test needs no physics engine and no simulation. It exists because a
// wrong enumerator here is silent: db.xml stores these as bare integers, so a
// mistake does not fail to load, it just quietly means something else -- a
// track with no friction tuning, or a car whose centre of mass is no longer
// locked.

#include "Nxp.h"

#include <cstdio>

static int gFailures = 0;

static void Expect(const char *what, long long actual, long long expected)
{
	const bool ok = actual == expected;
	printf("  [%s] %-46s %lld%s\n", ok ? "PASS" : "FAIL", what, actual,
	       ok ? "" : " <- WRONG");
	if (!ok)
	{
		printf("         expected %lld\n", expected);
		++gFailures;
	}
}

int RunEnumTests()
{
	printf("PhysX 2.8 enumeration values\n\n");

	// -- Values that appear literally in bin/Debug/db.xml -------------------
	//
	// These are the ones with teeth. Each is a number the shipped data stores
	// and the shim has to interpret the same way 2.8 did.

	printf(" values serialised in db.xml:\n");
	Expect("NX_WF_CLAMPED_FRICTION  (<wheelFlags>64</>)",
	       NX_WF_CLAMPED_FRICTION, 64);
	Expect("NX_AF_LOCK_COM|NX_AF_CONTACT_MODIFICATION (<flags>20</>)",
	       NX_AF_LOCK_COM | NX_AF_CONTACT_MODIFICATION, 20);
	Expect("NX_BF_VISUALIZATION|NX_BF_ENERGY_SLEEP_TEST (<flags>2304</>)",
	       NX_BF_VISUALIZATION | NX_BF_ENERGY_SLEEP_TEST, 2304);

	// -- Positional enums, where a dropped member shifts everything ---------

	printf("\n positional enums:\n");
	Expect("NX_SHAPE_WHEEL", NX_SHAPE_WHEEL, 4);
	Expect("NX_SHAPE_MESH", NX_SHAPE_MESH, 6);
	Expect("NX_ACCELERATION", NX_ACCELERATION, 5);
	Expect("NX_SMOOTH_VELOCITY_CHANGE", NX_SMOOTH_VELOCITY_CHANGE, 4);
	Expect("NX_VELOCITY_CHANGE", NX_VELOCITY_CHANGE, 2);
	Expect("NX_IMPULSE", NX_IMPULSE, 1);
	Expect("NX_CM_MIN", NX_CM_MIN, 1);
	Expect("NX_CM_MAX", NX_CM_MAX, 3);
	Expect("NX_CM_AVERAGE", NX_CM_AVERAGE, 0);
	Expect("NX_TIMESTEP_VARIABLE", NX_TIMESTEP_VARIABLE, 1);
	Expect("NX_FILTEROP_AND", NX_FILTEROP_AND, 0);
	Expect("NX_FILTEROP_OR", NX_FILTEROP_OR, 1);

	// -- Bit flags ----------------------------------------------------------

	printf("\n bit flags:\n");
	Expect("NX_AF_DISABLE_RESPONSE", NX_AF_DISABLE_RESPONSE, 2);
	Expect("NX_BF_DISABLE_GRAVITY", NX_BF_DISABLE_GRAVITY, 1);
	Expect("NX_MF_ANISOTROPIC", NX_MF_ANISOTROPIC, 1);
	Expect("NX_MF_DISABLE_FRICTION", NX_MF_DISABLE_FRICTION, 16);
	Expect("NX_MF_DISABLE_STRONG_FRICTION", NX_MF_DISABLE_STRONG_FRICTION, 32);
	Expect("NX_STATIC_SHAPES", NX_STATIC_SHAPES, 1);
	Expect("NX_ALL_SHAPES", NX_ALL_SHAPES, 3);
	Expect("NX_RIGID_BODY_FINISHED", NX_RIGID_BODY_FINISHED, 1);
	Expect("NX_IGNORE_PAIR", NX_IGNORE_PAIR, 1);

	// NX_NOTIFY_ALL is ten flags, not the handful an abbreviation suggests,
	// and NX_NOTIFY_CONTACT_MODIFICATION is deliberately not one of them --
	// which is why GameCar asks for both by name.
	Expect("NX_NOTIFY_ALL", NX_NOTIFY_ALL, 0x7fe);
	Expect("NX_NOTIFY_CONTACT_MODIFICATION", NX_NOTIFY_CONTACT_MODIFICATION, 0x10000);
	Expect("NX_NOTIFY_ALL excludes CONTACT_MODIFICATION",
	       (NX_NOTIFY_ALL & NX_NOTIFY_CONTACT_MODIFICATION) == 0, 1);

	// The NX_CCC_* friction-basis change flags are deliberately absent here.
	// In 2.8 they are nested inside class NxUserContactModify, not at namespace
	// scope, and putting them in Nxp.h to make this test tidier would misplace
	// them. They get transcribed and asserted when that interface lands.

	printf("\n raycast bits:\n");
	Expect("NX_RAYCAST_SHAPE", NX_RAYCAST_SHAPE, 1);
	Expect("NX_RAYCAST_IMPACT", NX_RAYCAST_IMPACT, 2);
	Expect("NX_RAYCAST_NORMAL", NX_RAYCAST_NORMAL, 4);
	Expect("NX_RAYCAST_DISTANCE", NX_RAYCAST_DISTANCE, 16);

	printf("\n%s (%d failure%s)\n", gFailures ? "FAILED" : "all values match",
	       gFailures, gFailures == 1 ? "" : "s");
	return gFailures;
}
