// NxVec3's conventions -- the ones that are silent when wrong.
//
// These are not arithmetic checks for their own sake. Each corresponds to a
// place where a plausible-looking reimplementation would compile and then
// quietly do something else.

#include "NxVec3.h"

#include <cstdio>
#include <cmath>
#include <type_traits>

static int gFailures = 0;

static void Check(const char *what, bool ok)
{
	printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok) ++gFailures;
}

static bool Near(NxReal a, NxReal b) { return std::fabs(double(a - b)) < 1e-5; }

int RunVec3Tests()
{
	printf("NxVec3 conventions\n\n");

	// The scalar constructor is explicit and the array constructor is not.
	// This is what makes NxVec3(someD3DXVECTOR3) work while keeping a bare
	// float from becoming a vector by accident.
	Check("array ctor is implicit (float* converts)",
	      std::is_convertible<const NxReal *, NxVec3>::value);
	Check("scalar ctor is explicit (float does NOT convert)",
	      !std::is_convertible<NxReal, NxVec3>::value);

	// Layout: three floats, no vtable, addressable as an array through get().
	//
	// Not "trivially copyable" -- 2.8 declares a user-provided copy constructor
	// and assignment operator, so the real type is not trivially copyable
	// either, and asserting that would be asserting something false about the
	// thing being reproduced. Standard layout is the property that actually
	// matters, and it is what makes &x behave as a three-element array.
	Check("layout is exactly three floats", sizeof(NxVec3) == 3 * sizeof(NxReal));
	Check("is standard layout", std::is_standard_layout<NxVec3>::value);
	{
		NxVec3 v(1, 2, 3);
		Check("x, y, z are contiguous", v.get()[0] == 1 && v.get()[1] == 2 && v.get()[2] == 3);
	}

	{
		const NxReal raw[3] = {1, 2, 3};
		NxVec3 v(raw);
		Check("array ctor reads v[0..2] in order", v.x == 1 && v.y == 2 && v.z == 3);
	}

	{
		NxVec3 v(2);
		Check("scalar ctor broadcasts", v.x == 2 && v.y == 2 && v.z == 2);
	}

	// get() has a const and a non-const form, and the game uses both -- one on
	// an rvalue to build a D3DXVECTOR3, one on an lvalue to fill a float array.
	{
		NxVec3 v(1, 2, 3);
		const NxVec3 &cv = v;
		Check("get() returns a pointer to x", v.get() == &v.x);
		Check("const get() returns a pointer to x", cv.get() == &cv.x);

		NxF32 out[3] = {0, 0, 0};
		v.get(out);
		Check("get(NxF32*) fills three floats", out[0] == 1 && out[1] == 2 && out[2] == 3);
	}

	// cross() computes into *this* from two arguments. It is NOT `this` crossed
	// with an argument, and it must tolerate aliasing -- 2.8 uses temporaries
	// "in case left or right is this".
	{
		NxVec3 result;
		result.cross(NxVec3(1, 0, 0), NxVec3(0, 1, 0));
		Check("cross(a,b) writes a x b into this",
		      Near(result.x, 0) && Near(result.y, 0) && Near(result.z, 1));

		NxVec3 self(1, 0, 0);
		self.cross(self, NxVec3(0, 1, 0));
		Check("cross() tolerates this as an argument",
		      Near(self.x, 0) && Near(self.y, 0) && Near(self.z, 1));
	}

	// normalize() returns the magnitude it had BEFORE normalising. Callers use
	// the return value to spot a degenerate vector, so returning the new
	// magnitude (1) would look right and break every such check.
	{
		NxVec3 v(0, 3, 4);
		const NxReal previous = v.normalize();
		Check("normalize() returns the OLD magnitude", Near(previous, 5));
		Check("normalize() leaves a unit vector", Near(v.magnitude(), 1));

		NxVec3 zero(0, 0, 0);
		const NxReal m = zero.normalize();
		Check("normalize() of a zero vector returns 0 and does not divide",
		      Near(m, 0) && zero.isFinite());
	}

	// 2.8 spells the cross product with ^ and the dot product with |, and the
	// game uses both spellings.
	{
		const NxVec3 c = NxVec3(1, 0, 0) ^ NxVec3(0, 1, 0);
		Check("operator^ is the cross product", Near(c.z, 1) && Near(c.x, 0));
		const NxReal d = NxVec3(1, 2, 3) | NxVec3(4, 5, 6);
		Check("operator| is the dot product", Near(d, 32));
	}

	{
		NxVec3 v(1, 2, 3);
		v.setNegative();
		Check("setNegative() negates in place", v.x == -1 && v.y == -2 && v.z == -3);
	}

	printf("\n%s (%d failure%s)\n", gFailures ? "FAILED" : "all conventions hold",
	       gFailures, gFailures == 1 ? "" : "s");
	return gFailures;
}
