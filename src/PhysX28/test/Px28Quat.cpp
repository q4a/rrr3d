// NxQuat's conventions.
//
// The one that matters most is the angle unit, because it is asymmetric and
// both halves are used: DataBase.cpp builds wheel poses with
// fromAngleAxis(90, ...) and GameCar.cpp steers with fromAngleAxisFast(rad).
// Swapping them rotates every wheel by 90 radians, which is only visible once
// something renders.

#include "NxQuat.h"

#include <cstdio>
#include <cmath>
#include <type_traits>

static int gFailures = 0;

static void Check(const char *what, bool ok)
{
	printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok) ++gFailures;
}

static bool Near(NxReal a, NxReal b) { return std::fabs(double(a - b)) < 1e-4; }
static bool NearVec(const NxVec3 &v, NxReal x, NxReal y, NxReal z)
{ return Near(v.x, x) && Near(v.y, y) && Near(v.z, z); }

int RunQuatTests()
{
	printf("NxQuat conventions\n\n");

	Check("layout is exactly four floats", sizeof(NxQuat) == 4 * sizeof(NxReal));
	Check("is standard layout", std::is_standard_layout<NxQuat>::value);

	// The whole point. A quarter turn about X takes +Y to +Z.
	{
		NxQuat q;
		q.fromAngleAxis(90, NxVec3(1, 0, 0));
		Check("fromAngleAxis takes DEGREES (90 about X maps +Y to +Z)",
		      NearVec(q.rot(NxVec3(0, 1, 0)), 0, 0, 1));
	}

	{
		NxQuat q;
		q.fromAngleAxisFast(NxReal(M_PI / 2), NxVec3(1, 0, 0));
		Check("fromAngleAxisFast takes RADIANS (pi/2 about X maps +Y to +Z)",
		      NearVec(q.rot(NxVec3(0, 1, 0)), 0, 0, 1));
	}

	// If the two ever agreed on a unit, this would catch it: 90 as radians is
	// not a quarter turn, and pi/2 as degrees is barely a nudge.
	{
		NxQuat deg, rad;
		deg.fromAngleAxis(90, NxVec3(1, 0, 0));
		rad.fromAngleAxisFast(90, NxVec3(1, 0, 0));
		Check("the two entry points disagree, as they must",
		      !Near(deg.x, rad.x) || !Near(deg.w, rad.w));
	}

	// The constructor delegates to fromAngleAxis, so it is degrees too.
	{
		const NxQuat q(90, NxVec3(0, 0, 1));
		Check("NxQuat(angle, axis) is DEGREES (90 about Z maps +X to +Y)",
		      NearVec(q.rot(NxVec3(1, 0, 0)), 0, 1, 0));
	}

	// Storage order is x,y,z,w -- the game stores quaternions that way and
	// uses setXYZW, so a w-first layout would transpose every saved rotation.
	{
		NxQuat q;
		q.setXYZW(1, 2, 3, 4);
		Check("setXYZW stores x,y,z,w in order",
		      q.x == 1 && q.y == 2 && q.z == 3 && q.w == 4);

		NxF32 out[4] = {0, 0, 0, 0};
		q.getXYZW(out);
		Check("getXYZW returns x,y,z,w in order",
		      out[0] == 1 && out[1] == 2 && out[2] == 3 && out[3] == 4);

		NxQuat w;
		w.setWXYZ(1, 2, 3, 4);
		Check("setWXYZ stores w first, and differs from setXYZW",
		      w.w == 1 && w.x == 2 && w.y == 3 && w.z == 4);
	}

	{
		NxQuat q;
		q.id();
		Check("id() is the identity rotation", q.isIdentityRotation());
		Check("identity rotates nothing", NearVec(q.rot(NxVec3(1, 2, 3)), 1, 2, 3));
	}

	// invert() then rot() must undo the original rotation.
	{
		NxQuat q;
		q.fromAngleAxis(37, NxVec3(0, 0, 1));
		const NxVec3 v(1, 2, 3);
		const NxVec3 rotated = q.rot(v);
		NxQuat inv(q);
		inv.invert();
		Check("invert() undoes rot()", NearVec(inv.rot(rotated), 1, 2, 3));
		Check("invRot() matches the inverted quaternion",
		      NearVec(q.invRot(rotated), 1, 2, 3));
	}

	// Composition order: (a * b) applied to v must equal a applied to b's result.
	{
		NxQuat a, b;
		a.fromAngleAxis(90, NxVec3(1, 0, 0));
		b.fromAngleAxis(90, NxVec3(0, 0, 1));
		const NxQuat ab = a * b;
		const NxVec3 v(1, 0, 0);
		Check("operator* composes as a(b(v))", NearVec(ab.rot(v), a.rot(b.rot(v)).x,
		                                               a.rot(b.rot(v)).y, a.rot(b.rot(v)).z));
	}

	printf("\n%s (%d failure%s)\n", gFailures ? "FAILED" : "all conventions hold",
	       gFailures, gFailures == 1 ? "" : "s");
	return gFailures;
}
